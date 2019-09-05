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
  static Extension* current_extension_;

  winrt::com_ptr<IDebugHostMemory2> sp_debug_host_memory_;
  winrt::com_ptr<IDebugHostSymbols> sp_debug_host_symbols_;
  winrt::com_ptr<IDebugHostExtensibility> sp_debug_host_extensibility_;

  winrt::com_ptr<IDebugHostTypeSignature> sp_local_type_signature_;
  winrt::com_ptr<IDebugHostTypeSignature> sp_maybe_local_type_signature_;
  winrt::com_ptr<IDebugHostTypeSignature> sp_handle_type_signature_;
  winrt::com_ptr<IDebugHostTypeSignature> sp_maybe_handle_type_signature_;
  winrt::com_ptr<IModelObject> sp_object_data_model_;
  winrt::com_ptr<IModelObject> sp_local_data_model_;
  winrt::com_ptr<IModelObject> sp_curr_isolate_model_;
  winrt::com_ptr<IModelObject> sp_list_chunks_model_;

 private:
  winrt::com_ptr<IDebugHostModule> sp_v8_module_;
  std::unordered_map<std::u16string, winrt::com_ptr<IDebugHostType>> sp_v8_object_types_;
  std::unordered_map<std::u16string, winrt::com_ptr<IDebugHostTypeSignature>> registered_handler_types_;
  winrt::com_ptr<IDebugHostContext> sp_v8_module_ctx_;
  ULONG v8_module_proc_id_;
};
