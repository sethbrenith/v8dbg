#include "curisolate.h"

int GetIsolateKey(winrt::com_ptr<IDebugHostContext>& sp_ctx) {
  auto sp_v8_module = Extension::current_extension->GetV8Module(sp_ctx);
  if (sp_v8_module == nullptr) return -1;

  winrt::com_ptr<IDebugHostSymbol> sp_isolate_sym;
  HRESULT hr =
      sp_v8_module->FindSymbolByName(L"isolate_key_", sp_isolate_sym.put());
  if (SUCCEEDED(hr)) {
    SymbolKind kind;
    hr = sp_isolate_sym->GetSymbolKind(&kind);
    if (SUCCEEDED(hr)) {
      if (kind == SymbolData) {
        winrt::com_ptr<IDebugHostData> sp_isolate_key_data;
        sp_isolate_sym.as(sp_isolate_key_data);
        Location loc;
        hr = sp_isolate_key_data->GetLocation(&loc);
        if (SUCCEEDED(hr)) {
          int isolate_key;
          ULONG64 bytes_read;
          hr = Extension::current_extension->sp_debug_host_memory->ReadBytes(
              sp_ctx.get(), loc, &isolate_key, 4, &bytes_read);
          return isolate_key;
        }
      }
    }
  }
  return -1;
}

HRESULT GetCurrentIsolate(winrt::com_ptr<IModelObject>& sp_result) {
  HRESULT hr = S_OK;
  sp_result = nullptr;

  // Get the current context
  winrt::com_ptr<IDebugHostContext> sp_host_context;
  winrt::com_ptr<IModelObject> sp_root_namespace;
  hr = sp_debug_host->GetCurrentContext(sp_host_context.put());
  if (FAILED(hr)) return hr;

  winrt::com_ptr<IModelObject> sp_curr_thread;
  if (!GetCurrentThread(sp_host_context, sp_curr_thread.put())) {
    return E_FAIL;
  }

  winrt::com_ptr<IModelObject> sp_environment, sp_environment_block;
  winrt::com_ptr<IModelObject> sp_tls_slots, sp_slot_index, sp_isolate_ptr;
  hr = sp_curr_thread->GetKeyValue(L"Environment", sp_environment.put(), nullptr);
  if (FAILED(hr)) return E_FAIL;

  hr = sp_environment->GetKeyValue(L"EnvironmentBlock", sp_environment_block.put(),
                                  nullptr);
  if (FAILED(hr)) return E_FAIL;

  // EnvironmentBlock and TlsSlots are native types (TypeUDT) and thus GetRawValue
  // rather than GetKeyValue should be used to get field (member) values.
  ModelObjectKind kind;
  hr = sp_environment_block->GetKind(&kind);
  if (kind != ModelObjectKind::ObjectTargetObject) return E_FAIL;

  hr = sp_environment_block->GetRawValue(SymbolField, L"TlsSlots", 0,
                                       sp_tls_slots.put());
  if (FAILED(hr)) return E_FAIL;

  int isolate_key = GetIsolateKey(sp_host_context);
  hr = CreateInt32(isolate_key, sp_slot_index.put());
  if (isolate_key == -1 || FAILED(hr)) return E_FAIL;

  hr = GetModelAtIndex(sp_tls_slots, sp_slot_index, sp_isolate_ptr.put());
  if (FAILED(hr)) return E_FAIL;

  // Need to dereference the slot and then get the address held in it
  winrt::com_ptr<IModelObject> sp_dereferenced_slot;
  hr = sp_isolate_ptr->Dereference(sp_dereferenced_slot.put());
  if (FAILED(hr)) return hr;

  VARIANT vt_isolate_ptr;
  hr = sp_dereferenced_slot->GetIntrinsicValue(&vt_isolate_ptr);
  if (FAILED(hr) || vt_isolate_ptr.vt != VT_UI8) {
    return E_FAIL;
  }
  Location isolate_addr{vt_isolate_ptr.ullVal};

  // If we got the isolate_key OK, then must have the V8 module loaded
  // Get the internal Isolate type from it
  winrt::com_ptr<IDebugHostType> sp_isolate_type, sp_isolate_ptr_type;
  hr = Extension::current_extension->GetV8Module(sp_host_context)
           ->FindTypeByName(L"v8::internal::Isolate", sp_isolate_type.put());
  if (FAILED(hr)) return hr;
  hr = sp_isolate_type->CreatePointerTo(PointerStandard, sp_isolate_ptr_type.put());
  if (FAILED(hr)) return hr;

  hr = sp_data_model_manager->CreateTypedObject(
      sp_host_context.get(), isolate_addr, sp_isolate_type.get(), sp_result.put());
  if (FAILED(hr)) return hr;

  return S_OK;
}

HRESULT __stdcall CurrIsolateAlias::Call(IModelObject* pContextObject,
                                         ULONG64 arg_count,
                                         IModelObject** pp_arguments,
                                         IModelObject** pp_result,
                                         IKeyStore** pp_metadata) noexcept {
  HRESULT hr = S_OK;
  *pp_result = nullptr;
  winrt::com_ptr<IModelObject> sp_result;
  hr = GetCurrentIsolate(sp_result);
  if (SUCCEEDED(hr)) *pp_result = sp_result.detach();
  return hr;
}
