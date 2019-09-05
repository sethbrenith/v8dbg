#include "dbgext.h"
#include <crtdbg.h>

// See https://docs.microsoft.com/en-us/visualstudio/debugger/crt-debugging-techniques
// for the memory leak and debugger reporting macros used from <crtdbg.h>
_CrtMemState mem_old, mem_new, mem_diff;

winrt::com_ptr<IDataModelManager> sp_data_model_manager;
winrt::com_ptr<IDebugHost> sp_debug_host;
winrt::com_ptr<IDebugControl5> sp_debug_control;

extern "C" {

__declspec(dllexport) HRESULT
    __stdcall DebugExtensionInitialize(PULONG /*pVersion*/, PULONG /*pFlags*/) {
  _RPTF0(_CRT_WARN, "Entered DebugExtensionInitialize\n");
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
  _CrtMemCheckpoint(&mem_old);

  winrt::com_ptr<IDebugClient> sp_debug_client;
  winrt::com_ptr<IHostDataModelAccess> sp_data_model_access;

  HRESULT hr = DebugCreate(__uuidof(IDebugClient), sp_debug_client.put_void());
  if (FAILED(hr)) return E_FAIL;

  if (!sp_debug_client.try_as(sp_data_model_access)) return E_FAIL;
  if (!sp_debug_client.try_as(sp_debug_control)) return E_FAIL;

  hr = sp_data_model_access->GetDataModel(sp_data_model_manager.put(),
                                       sp_debug_host.put());
  if (FAILED(hr)) return E_FAIL;

  return CreateExtension() ? S_OK : E_FAIL;
  return S_OK;
}

__declspec(dllexport) void __stdcall DebugExtensionUninitialize() {
  _RPTF0(_CRT_WARN, "Entered DebugExtensionUninitialize\n");
  DestroyExtension();
  sp_debug_host = nullptr;
  sp_data_model_manager = nullptr;

  _CrtMemCheckpoint(&mem_new);
  if (_CrtMemDifference(&mem_diff, &mem_old, &mem_new)) {
    _CrtMemDumpStatistics(&mem_diff);
  }
  if (_CrtDumpMemoryLeaks()) {
    _RPTF0(_CRT_ERROR, "Memory leaks detected!\n");
  }
}

__declspec(dllexport) HRESULT __stdcall DebugExtensionCanUnload(void) {
  _RPTF0(_CRT_WARN, "Entered DebugExtensionCanUnload\n");
  uint32_t lock_count = winrt::get_module_lock().load();
  _RPTF1(_CRT_WARN, "module_lock count in DebugExtensionCanUnload is %d\n",
         lock_count);
  return lock_count == 0 ? S_OK : S_FALSE;
}

__declspec(dllexport) void __stdcall DebugExtensionUnload() {
  _RPTF0(_CRT_WARN, "Entered DebugExtensionUnload\n");
  return;
}

}  // extern "C"
