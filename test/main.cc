#include "common.h"

#include <cstdio>
#include <exception>

// See the docs at
// https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/using-the-debugger-engine-api

const char* v8dbg = "d:\\repos\\v8dbg\\x64\\v8dbg.dll";
const char* SymbolPath = "f:\\repos\\ana\\v8\\out\\debug_x64";
const char* CommandLine =
    "f:\\repos\\ana\\v8\\out\\debug_x64\\d8.exe d:\\scripts\\wrapper.js";

class LoadExtensionScope {
 public:
  LoadExtensionScope(winrt::com_ptr<IDebugControl3> p_debug_control) :
      p_debug_control(p_debug_control) {
    HRESULT hr = p_debug_control->AddExtension(v8dbg, 0, &ext_handle);
    winrt::check_hresult(hr);
    // HACK: Below fails, but is required for the extension to actually
    // initialize. Just the AddExtension call doesn't actually load and
    // initialize it.
    p_debug_control->CallExtension(ext_handle, "Foo", "Bar");
  }
  ~LoadExtensionScope() {
    // Let the extension uninitialize so it can deallocate memory, meaning any
    // reported memory leaks should be real bugs.
    p_debug_control->RemoveExtension(ext_handle);
  }
 private:
  LoadExtensionScope(const LoadExtensionScope&) = delete;
  LoadExtensionScope& operator=(const LoadExtensionScope&) = delete;
  winrt::com_ptr<IDebugControl3> p_debug_control;
  ULONG64 ext_handle;
};

void RunTests() {
  // Get the Debug client
  winrt::com_ptr<IDebugClient5> p_client;
  HRESULT hr = DebugCreate(__uuidof(IDebugClient5), p_client.put_void());
  winrt::check_hresult(hr);

  // Set noisy symbol loading on
  winrt::com_ptr<IDebugSymbols> p_symbols;
  hr = p_client->QueryInterface(__uuidof(IDebugSymbols), p_symbols.put_void());
  winrt::check_hresult(hr);

  // Turn on noisy symbol loading
  // hr = p_symbols->AddSymbolOptions(0x80000000 /*SYMOPT_DEBUG*/);
  // winrt::check_hresult(hr);

  // Symbol loading fails if the pdb is in the same folder as the exe, but it's
  // not on the path.
  hr = p_symbols->SetSymbolPath(SymbolPath);
  winrt::check_hresult(hr);

  // Set the event callbacks
  MyCallback callback;
  hr = p_client->SetEventCallbacks(&callback);
  winrt::check_hresult(hr);

  // Launch the process with the debugger attached
  DEBUG_CREATE_PROCESS_OPTIONS proc_options;
  proc_options.CreateFlags = DEBUG_PROCESS;
  proc_options.EngCreateFlags = 0;
  proc_options.VerifierFlags = 0;
  proc_options.Reserved = 0;
  hr =
      p_client->CreateProcessW(0, const_cast<char*>(CommandLine), DEBUG_PROCESS);
  winrt::check_hresult(hr);

  // Wait for the attach event
  winrt::com_ptr<IDebugControl3> p_debug_control;
  hr = p_client->QueryInterface(__uuidof(IDebugControl3),
                               p_debug_control.put_void());
  winrt::check_hresult(hr);
  hr = p_debug_control->WaitForEvent(0, INFINITE);
  winrt::check_hresult(hr);

  // Set a breakpoint
  // PDEBUG_BREAKPOINT bp;
  winrt::com_ptr<IDebugBreakpoint> bp;
  hr = p_debug_control->AddBreakpoint(DEBUG_BREAKPOINT_CODE, DEBUG_ANY_ID,
                                    bp.put());
  winrt::check_hresult(hr);
  hr = bp->SetOffsetExpression("v8!v8::Script::Run");
  winrt::check_hresult(hr);
  hr = bp->AddFlags(DEBUG_BREAKPOINT_ENABLED);
  winrt::check_hresult(hr);

  hr = p_debug_control->SetExecutionStatus(DEBUG_STATUS_GO);
  winrt::check_hresult(hr);

  // Wait for the breakpoint. Fails here saying device is in invalid state, but
  // everything else was S_OK??
  hr = p_debug_control->WaitForEvent(0, INFINITE);
  winrt::check_hresult(hr);

  ULONG type, proc_id, thread_id, desc_used;
  byte desc[1024];
  hr = p_debug_control->GetLastEventInformation(
      &type, &proc_id, &thread_id, nullptr, 0, nullptr,
      reinterpret_cast<PSTR>(desc), 1024, &desc_used);
  winrt::check_hresult(hr);

  LoadExtensionScope extension_loaded(p_debug_control);

  // Set the output callbacks after the extension is loaded, so it gets
  // destroyed before the extension unloads. This avoids reporting incorrectly
  // reporting that the output buffer was leaked during extension teardown.
  MyOutput output(p_client);

  // Step one line to ensure locals are available
  hr = p_debug_control->SetCodeLevel(DEBUG_LEVEL_SOURCE);
  hr =
      p_debug_control->Execute(DEBUG_OUTCTL_ALL_CLIENTS, "gu", DEBUG_EXECUTE_ECHO);
  hr = p_debug_control->WaitForEvent(0, INFINITE);

  // Do some actual testing
  output.log.clear();
  hr = p_debug_control->Execute(DEBUG_OUTCTL_ALL_CLIENTS,
                              "dx @$curisolate().isolate_data_",
                              DEBUG_EXECUTE_ECHO);
  if (output.log.find("[Type: v8::internal::RootsTable]") ==
      std::string::npos) {
    printf(
        "***ERROR***: 'dx @$curisolate()' did not return the expected isolate "
        "types\n%s\n", output.log.c_str());
  } else {
    printf("SUCCESS: Function alias @$curisolate\n");
  }

  output.log.clear();
  hr = p_debug_control->Execute(DEBUG_OUTCTL_ALL_CLIENTS, "dx name.Value",
                              DEBUG_EXECUTE_ECHO);
  if (output.log.find("<SeqOneByteString>: d:\\scripts\\wrapper.js") == std::string::npos) {
    printf(
        "***ERROR***: 'dx name.Value' did not return the expected local "
        "representation\n%s\n", output.log.c_str());
  } else {
    printf("SUCCESS: v8::Local<v8::Value> decoding\n");
  }

  output.log.clear();
  hr = p_debug_control->Execute(DEBUG_OUTCTL_ALL_CLIENTS, "p;dx maybe_result.Value",
                              DEBUG_EXECUTE_ECHO);
  if (output.log.find("<Oddball>Null") == std::string::npos) {
    printf(
        "***ERROR***: 'dx maybe_result.Value' did not return the expected Oddball "
        "representation\n%s\n", output.log.c_str());
  } else {
    printf("SUCCESS: Oddball support\n");
  }

  printf("=== Run completed! ===\n");
  // Detach before exiting
  hr = p_client->DetachProcesses();
  winrt::check_hresult(hr);
}

int main(int argv, char** pargv) {
  // Initialize COM... Though it doesn't seem to matter if you don't!
  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  winrt::check_hresult(hr);
  RunTests();

  CoUninitialize();
  return 0;
}
