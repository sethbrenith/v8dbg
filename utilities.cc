#include "utilities.h"

HRESULT CreateProperty(IDataModelManager* pManager,
                       IModelPropertyAccessor* pProperty,
                       IModelObject** pp_property_object) {
  *pp_property_object = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_UNKNOWN;
  vt_val.punkVal = pProperty;

  HRESULT hr = pManager->CreateIntrinsicObject(ObjectPropertyAccessor, &vt_val,
                                               pp_property_object);
  return hr;
}

HRESULT CreateULong64(ULONG64 value, IModelObject** pp_int) {
  HRESULT hr = S_OK;
  *pp_int = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_UI8;
  vt_val.ullVal = value;

  hr =
      sp_data_model_manager->CreateIntrinsicObject(ObjectIntrinsic, &vt_val, pp_int);
  return hr;
}

HRESULT CreateInt32(int value, IModelObject** pp_int) {
  HRESULT hr = S_OK;
  *pp_int = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_I4;
  vt_val.intVal = value;

  hr =
      sp_data_model_manager->CreateIntrinsicObject(ObjectIntrinsic, &vt_val, pp_int);
  return hr;
}

HRESULT CreateUInt32(uint32_t value, IModelObject** pp_int) {
  HRESULT hr = S_OK;
  *pp_int = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_UI4;
  vt_val.uintVal = value;

  hr =
      sp_data_model_manager->CreateIntrinsicObject(ObjectIntrinsic, &vt_val, pp_int);
  return hr;
}

HRESULT CreateBool(bool value, IModelObject** pp_val) {
  HRESULT hr = S_OK;
  *pp_val = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_BOOL;
  vt_val.boolVal = value;

  hr =
      sp_data_model_manager->CreateIntrinsicObject(ObjectIntrinsic, &vt_val, pp_val);
  return hr;
}

HRESULT CreateNumber(double value, IModelObject** pp_val) {
  HRESULT hr = S_OK;
  *pp_val = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_R8;
  vt_val.dblVal = value;

  hr =
      sp_data_model_manager->CreateIntrinsicObject(ObjectIntrinsic, &vt_val, pp_val);
  return hr;
}

HRESULT CreateString(std::u16string value, IModelObject** pp_val) {
  HRESULT hr = S_OK;
  *pp_val = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_BSTR;
  vt_val.bstrVal =
      ::SysAllocString(reinterpret_cast<const OLECHAR*>(value.c_str()));

  hr =
      sp_data_model_manager->CreateIntrinsicObject(ObjectIntrinsic, &vt_val, pp_val);
  return hr;
}

bool GetModelAtIndex(winrt::com_ptr<IModelObject>& sp_parent,
                     winrt::com_ptr<IModelObject>& sp_index,
                     IModelObject** pResult) {
  winrt::com_ptr<IIndexableConcept> sp_indexable_concept;
  HRESULT hr = sp_parent->GetConcept(
      __uuidof(IIndexableConcept),
      reinterpret_cast<IUnknown**>(sp_indexable_concept.put()), nullptr);
  if (FAILED(hr)) return false;

  std::vector<IModelObject*> pIndexers{sp_index.get()};
  hr = sp_indexable_concept->GetAt(sp_parent.get(), 1, pIndexers.data(), pResult,
                                 nullptr);
  return SUCCEEDED(hr);
}

bool GetCurrentThread(winrt::com_ptr<IDebugHostContext>& sp_host_context,
                      IModelObject** pCurrentThread) {
  HRESULT hr = S_OK;
  winrt::com_ptr<IModelObject> sp_boxed_context, sp_root_namespace;
  winrt::com_ptr<IModelObject> sp_debugger, sp_sessions, sp_processes, sp_threads;
  winrt::com_ptr<IModelObject> sp_curr_session, sp_curr_process, sp_curr_thread;

  // Get the current context boxed as an IModelObject
  VARIANT vt_context;
  vt_context.vt = VT_UNKNOWN;
  vt_context.punkVal = sp_host_context.get();
  hr = sp_data_model_manager->CreateIntrinsicObject(ObjectContext, &vt_context,
                                                 sp_boxed_context.put());
  if (FAILED(hr)) return false;

  hr = sp_data_model_manager->GetRootNamespace(sp_root_namespace.put());
  if (FAILED(hr)) return false;

  hr = sp_root_namespace->GetKeyValue(L"Debugger", sp_debugger.put(), nullptr);
  if (FAILED(hr)) return false;

  hr = sp_debugger->GetKeyValue(L"Sessions", sp_sessions.put(), nullptr);
  if (!GetModelAtIndex(sp_sessions, sp_boxed_context, sp_curr_session.put())) {
    return false;
  }

  hr = sp_curr_session->GetKeyValue(L"Processes", sp_processes.put(), nullptr);
  if (!GetModelAtIndex(sp_processes, sp_boxed_context, sp_curr_process.put())) {
    return false;
  }

  hr = sp_curr_process->GetKeyValue(L"Threads", sp_threads.put(), nullptr);
  if (!GetModelAtIndex(sp_threads, sp_boxed_context, sp_curr_thread.put())) {
    return false;
  }
  *pCurrentThread = sp_curr_thread.detach();
  return true;
}
