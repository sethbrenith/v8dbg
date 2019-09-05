#pragma once

#include "../utilities.h"
#include <unordered_set>

class Extension {
 public:
  bool Initialize();
  ~Extension();
  winrt::com_ptr<IDebugHostModule> GetV8Module(winrt::com_ptr<IDebugHostContext>& sp_ctx);
  winrt::com_ptr<IDebugHostType> Extension::GetV8ObjectType(winrt::com_ptr<IDebugHostContext>& sp_ctx, const char16_t* type_name = u"v8::internal::Object");
  void TryRegisterType(winrt::com_ptr<IDebugHostType>& sp_type, std::u16string type_name);
  static Extension* current_extension;

  winrt::com_ptr<IDebugHostMemory2> sp_debug_host_memory;
  winrt::com_ptr<IDebugHostSymbols> sp_debug_host_symbols;
  winrt::com_ptr<IDebugHostExtensibility> sp_debug_host_extensibility;

  winrt::com_ptr<IDebugHostTypeSignature> sp_local_type_signature;
  winrt::com_ptr<IDebugHostTypeSignature> sp_maybe_local_type_signature;
  winrt::com_ptr<IDebugHostTypeSignature> sp_handle_type_signature;
  winrt::com_ptr<IDebugHostTypeSignature> sp_maybe_handle_type_signature;
  winrt::com_ptr<IModelObject> sp_object_data_model;
  winrt::com_ptr<IModelObject> sp_local_data_model;
  winrt::com_ptr<IModelObject> sp_curr_isolate_model;
  winrt::com_ptr<IModelObject> sp_list_chunks_model;

 private:
  winrt::com_ptr<IDebugHostModule> sp_v8_module;
  std::unordered_map<std::u16string, winrt::com_ptr<IDebugHostType>> sp_v8_object_types;
  std::unordered_map<std::u16string, winrt::com_ptr<IDebugHostTypeSignature>> registered_handler_types;
  winrt::com_ptr<IDebugHostContext> sp_v8_module_ctx;
  ULONG v8_module_proc_id;
};
