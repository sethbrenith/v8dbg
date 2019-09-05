#include "../utilities.h"
#include "extension.h"
#include "curisolate.h"
#include "list-chunks.h"
#include "object.h"
#include <iostream>

Extension* Extension::current_extension = nullptr;
const wchar_t *pcur_isolate = L"curisolate";
const wchar_t *plist_chunks = L"listchunks";

bool CreateExtension() {
  _RPTF0(_CRT_WARN, "Entered CreateExtension\n");
  if (Extension::current_extension != nullptr || sp_data_model_manager == nullptr ||
      sp_debug_host == nullptr) {
    return false;
  } else {
    Extension* new_extension = new (std::nothrow) Extension();
    if (new_extension && new_extension->Initialize()) {
      Extension::current_extension = new_extension;
      return true;
    } else {
      delete new_extension;
      return false;
    }
  }
}

void DestroyExtension() {
  _RPTF0(_CRT_WARN, "Entered DestroyExtension\n");
  if (Extension::current_extension != nullptr) {
    delete Extension::current_extension;
    Extension::current_extension = nullptr;
  }
  return;
}

bool DoesTypeDeriveFromObject(winrt::com_ptr<IDebugHostType>& sp_type) {
  _bstr_t name;
  HRESULT hr = sp_type->GetName(name.GetAddress());
  if (!SUCCEEDED(hr)) return false;
  if (std::wstring(static_cast<wchar_t*>(name)) == L"v8::internal::Object") return true;

  winrt::com_ptr<IDebugHostSymbolEnumerator> sp_super_class_enumerator;
  hr = sp_type->EnumerateChildren(SymbolKind::SymbolBaseClass, nullptr, sp_super_class_enumerator.put());
  if (!SUCCEEDED(hr)) return false;

  while (true) {
    winrt::com_ptr<IDebugHostSymbol> sp_type_symbol;
    if (sp_super_class_enumerator->GetNext(sp_type_symbol.put()) != S_OK) break;
    winrt::com_ptr<IDebugHostBaseClass> sp_base_class = sp_type_symbol.as<IDebugHostBaseClass>();
    winrt::com_ptr<IDebugHostType> sp_base_type;
    hr = sp_base_class->GetType(sp_base_type.put());
    if (!SUCCEEDED(hr)) continue;
    if (DoesTypeDeriveFromObject(sp_base_type)) {
      return true;
    }
  }

  return false;
}

void Extension::TryRegisterType(winrt::com_ptr<IDebugHostType>& sp_type, std::u16string type_name) {
  auto insertion_result = registered_handler_types.insert({type_name, nullptr});
  if (!insertion_result.second) return;
  if (DoesTypeDeriveFromObject(sp_type)) {
    winrt::com_ptr<IDebugHostTypeSignature> sp_object_type_signature;
    HRESULT hr = sp_debug_host_symbols->CreateTypeSignature(reinterpret_cast<const wchar_t*>(type_name.c_str()), nullptr,
                                            sp_object_type_signature.put());
    if (FAILED(hr)) return;
    hr = sp_data_model_manager->RegisterModelForTypeSignature(
        sp_object_type_signature.get(), sp_object_data_model.get());
    insertion_result.first->second = sp_object_type_signature;
  }
}

winrt::com_ptr<IDebugHostType> Extension::GetV8ObjectType(winrt::com_ptr<IDebugHostContext>& sp_ctx, const char16_t* type_name) {
  bool is_equal;
  if (sp_v8_module_ctx == nullptr || !SUCCEEDED(sp_v8_module_ctx->IsEqualTo(sp_ctx.get(), &is_equal)) || !is_equal) {
    // Context changed; clear the dictionary.
    sp_v8_object_types.clear();
  }

  GetV8Module(sp_ctx); // Will force the correct module to load
  if (sp_v8_module == nullptr) return nullptr;

  auto& dictionary_entry = sp_v8_object_types[type_name];
  if (dictionary_entry == nullptr) {
    HRESULT hr = sp_v8_module->FindTypeByName(reinterpret_cast<PCWSTR>(type_name), dictionary_entry.put());
    if (SUCCEEDED(hr)) {
      // It's too slow to enumerate all types in the v8 module up front and
      // register type handlers for all of them, but we can opportunistically do
      // so here for any types that we happen to be using. This makes the user
      // experience a little nicer because you can avoid opening one extra level
      // of data.
      TryRegisterType(dictionary_entry, type_name);
    }
  }
  return dictionary_entry;
}

winrt::com_ptr<IDebugHostModule> Extension::GetV8Module(winrt::com_ptr<IDebugHostContext>& sp_ctx) {
  // Return the cached version if it exists and the context is the same

  // Note: Context will often have the CUSTOM flag set, which never compares equal.
  // So for now DON'T compare by context, but by proc_id. (An API is in progress
  // to compare by address space, which should be usable when shipped).
  /*
  if (sp_v8_module != nullptr) {
    bool is_equal;
    if (SUCCEEDED(sp_v8_module_ctx->IsEqualTo(sp_ctx.get(), &is_equal)) && is_equal) {
      return sp_v8_module;
    } else {
      sp_v8_module = nullptr;
      sp_v8_module_ctx = nullptr;
    }
  }
  */
  winrt::com_ptr<IDebugSystemObjects> sp_sys_objects;
  ULONG proc_id = 0;
  if (sp_debug_control.try_as(sp_sys_objects))
  {
    if(SUCCEEDED(sp_sys_objects->GetCurrentProcessSystemId(&proc_id))) {
      if (proc_id == v8_module_proc_id && sp_v8_module != nullptr) return sp_v8_module;
    }
  }

  // Loop through the modules looking for the one that holds the "isolate_key_"
  winrt::com_ptr<IDebugHostSymbolEnumerator> sp_enum;
  if (SUCCEEDED(sp_debug_host_symbols->EnumerateModules(sp_ctx.get(), sp_enum.put()))) {
    HRESULT hr = S_OK;
    while (true) {
      winrt::com_ptr<IDebugHostSymbol> sp_mod_sym;
      hr = sp_enum->GetNext(sp_mod_sym.put());
      // hr == E_BOUNDS : hit the end of the enumerator
      // hr == E_ABORT  : a user interrupt was requested
      if (FAILED(hr)) break;
      winrt::com_ptr<IDebugHostModule> sp_module;
      if (sp_mod_sym.try_as(sp_module)) /* should always succeed */
      {
        winrt::com_ptr<IDebugHostSymbol> sp_isolate_sym;
        // The below symbol is specific to the main V8 module
        hr = sp_module->FindSymbolByName(L"isolate_key_", sp_isolate_sym.put());
        if (SUCCEEDED(hr)) {
          sp_v8_module = sp_module;
          sp_v8_module_ctx = sp_ctx;
          v8_module_proc_id = proc_id;
          // Output location
          BSTR module_name;
          if(SUCCEEDED(sp_module->GetImageName(true, &module_name))) {
            _RPTW1(_CRT_WARN, L"V8 symbols loaded from %s\n", module_name);
            sp_debug_control->Output(DEBUG_OUTPUT_NORMAL, "V8 symbols loaded from %S\n", module_name);
            ::SysFreeString(module_name);
          }
          break;
        }
      }
    }
  }
  // This will be the located module, or still nullptr if above fails
  return sp_v8_module;
}

bool Extension::Initialize() {
  _RPTF0(_CRT_WARN, "Entered ExtensionInitialize\n");

  if (!sp_debug_host.try_as(sp_debug_host_memory)) return false;
  if (!sp_debug_host.try_as(sp_debug_host_symbols)) return false;
  if (!sp_debug_host.try_as(sp_debug_host_extensibility)) return false;

  // Create an instance of the DataModel 'parent' for v8::internal::Object types
  auto object_data_model{winrt::make<V8ObjectDataModel>()};
  HRESULT hr = sp_data_model_manager->CreateDataModelObject(
      object_data_model.get(), sp_object_data_model.put());
  if (FAILED(hr)) return false;
  hr = sp_object_data_model->SetConcept(__uuidof(IStringDisplayableConcept),
                                     object_data_model.get(), nullptr);
  if (FAILED(hr)) return false;
  auto i_dynamic = object_data_model.as<IDynamicKeyProviderConcept>();
  hr = sp_object_data_model->SetConcept(__uuidof(IDynamicKeyProviderConcept),
                                     i_dynamic.get(), nullptr);
  if (FAILED(hr)) return false;

  // Parent the model for the type
  for (const char16_t* name : {u"v8::internal::Object", u"v8::internal::TaggedValue"}) {
    winrt::com_ptr<IDebugHostTypeSignature> sp_object_type_signature;
    hr = sp_debug_host_symbols->CreateTypeSignature(reinterpret_cast<const wchar_t*>(name), nullptr,
                                            sp_object_type_signature.put());
    if (FAILED(hr)) return false;
    hr = sp_data_model_manager->RegisterModelForTypeSignature(
        sp_object_type_signature.get(), sp_object_data_model.get());
    registered_handler_types[name] = sp_object_type_signature;
  }

  // Create an instance of the DataModel 'parent' class for v8::Local<*> types
  auto local_data_model{winrt::make<V8LocalDataModel>()};
  // Create an IModelObject out of it
  hr = sp_data_model_manager->CreateDataModelObject(local_data_model.get(),
                                                 sp_local_data_model.put());
  if (FAILED(hr)) return false;

  // Create a type signature for the v8::Local symbol
  hr = sp_debug_host_symbols->CreateTypeSignature(L"v8::Local<*>", nullptr,
                                          sp_local_type_signature.put());
  if (FAILED(hr)) return false;
  hr = sp_debug_host_symbols->CreateTypeSignature(L"v8::MaybeLocal<*>", nullptr,
                                          sp_maybe_local_type_signature.put());
  if (FAILED(hr)) return false;
  hr = sp_debug_host_symbols->CreateTypeSignature(L"v8::internal::Handle<*>", nullptr,
                                          sp_handle_type_signature.put());
  if (FAILED(hr)) return false;
  hr = sp_debug_host_symbols->CreateTypeSignature(L"v8::internal::MaybeHandle<*>", nullptr,
                                          sp_maybe_handle_type_signature.put());
  if (FAILED(hr)) return false;

  // Add the 'Value' property to the parent model.
  auto local_value_property{winrt::make<V8LocalValueProperty>()};
  winrt::com_ptr<IModelObject> sp_local_value_property_model;
  hr = CreateProperty(sp_data_model_manager.get(), local_value_property.get(),
                      sp_local_value_property_model.put());
  hr = sp_local_data_model->SetKey(L"Value", sp_local_value_property_model.get(),
                                nullptr);
  // Register the DataModel as the viewer for the type signature
  hr = sp_data_model_manager->RegisterModelForTypeSignature(
      sp_local_type_signature.get(), sp_local_data_model.get());
  hr = sp_data_model_manager->RegisterModelForTypeSignature(
      sp_maybe_local_type_signature.get(), sp_local_data_model.get());
  hr = sp_data_model_manager->RegisterModelForTypeSignature(
      sp_handle_type_signature.get(), sp_local_data_model.get());
  hr = sp_data_model_manager->RegisterModelForTypeSignature(
      sp_maybe_handle_type_signature.get(), sp_local_data_model.get());

  // Register the @$currisolate function alias.
  auto curr_isolate_function{winrt::make<CurrIsolateAlias>()};

  VARIANT vt_curr_isolate_function;
  vt_curr_isolate_function.vt = VT_UNKNOWN;
  vt_curr_isolate_function.punkVal =
      static_cast<IModelMethod*>(curr_isolate_function.get());

  hr = sp_data_model_manager->CreateIntrinsicObject(
      ObjectMethod, &vt_curr_isolate_function, sp_curr_isolate_model.put());
  hr = sp_debug_host_extensibility->CreateFunctionAlias(pcur_isolate,
                                                     sp_curr_isolate_model.get());

  // Register the @$listchunks function alias.
  auto list_chunks_function{winrt::make<ListChunksAlias>()};

  VARIANT vt_list_chunks_function;
  vt_list_chunks_function.vt = VT_UNKNOWN;
  vt_list_chunks_function.punkVal =
      static_cast<IModelMethod*>(list_chunks_function.get());

  hr = sp_data_model_manager->CreateIntrinsicObject(
      ObjectMethod, &vt_list_chunks_function, sp_list_chunks_model.put());
  hr = sp_debug_host_extensibility->CreateFunctionAlias(plist_chunks,
                                                     sp_list_chunks_model.get());

  return !FAILED(hr);
}

Extension::~Extension() {
  _RPTF0(_CRT_WARN, "Entered Extension::~Extension\n");
  sp_debug_host_extensibility->DestroyFunctionAlias(pcur_isolate);
  sp_debug_host_extensibility->DestroyFunctionAlias(plist_chunks);

  for (const auto& registered : registered_handler_types) {
    if (registered.second != nullptr) {
      sp_data_model_manager->UnregisterModelForTypeSignature(
          sp_object_data_model.get(), registered.second.get());
    }
  }
  sp_data_model_manager->UnregisterModelForTypeSignature(
      sp_local_data_model.get(), sp_local_type_signature.get());
  sp_data_model_manager->UnregisterModelForTypeSignature(
      sp_local_data_model.get(), sp_maybe_local_type_signature.get());
  sp_data_model_manager->UnregisterModelForTypeSignature(
      sp_local_data_model.get(), sp_handle_type_signature.get());
  sp_data_model_manager->UnregisterModelForTypeSignature(
      sp_local_data_model.get(), sp_maybe_handle_type_signature.get());
}
