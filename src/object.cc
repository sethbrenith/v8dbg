#include "../utilities.h"
#include "object.h"
#include "extension.h"
#include "v8.h"

HRESULT V8LocalValueProperty::GetValue(PCWSTR pwsz_key,
                                       IModelObject* p_v8_local_instance,
                                       IModelObject** pp_value) {
  // Get the parametric type within v8::Local<*>
  // Set value to a pointer to an instance of this type.

  auto ext = Extension::current_extension_;
  if (ext == nullptr) return E_FAIL;

  winrt::com_ptr<IDebugHostType> sp_type;
  HRESULT hr = p_v8_local_instance->GetTypeInfo(sp_type.put());
  if (FAILED(hr)) return hr;

  bool is_generic;
  hr = sp_type->IsGeneric(&is_generic);
  if (FAILED(hr) || !is_generic) return E_FAIL;

  winrt::com_ptr<IDebugHostSymbol> sp_generic_arg;
  hr = sp_type->GetGenericArgumentAt(0, sp_generic_arg.put());
  // TODO: Rather than treat everything as v8::internal::Object, just treat as
  // the generic type if derived from v8::internal::Object.
  if (FAILED(hr)) return hr;

  winrt::com_ptr<IDebugHostContext> sp_ctx;
  hr = p_v8_local_instance->GetContext(sp_ctx.put());
  if (FAILED(hr)) return hr;
  winrt::com_ptr<IDebugHostType> sp_v8_object_type = Extension::current_extension_->GetV8ObjectType(sp_ctx);

  Location loc;
  hr = p_v8_local_instance->GetLocation(&loc);
  if (FAILED(hr)) return hr;

  // Read the pointer at the Object location
  ULONG64 obj_address;
  hr = ext->sp_debug_host_memory_->ReadPointers(sp_ctx.get(), loc, 1, &obj_address);
  if (FAILED(hr)) return hr;

  // If the val_ is a nullptr, then there is no value in the Local.
  if(obj_address == 0) {
    hr = CreateString(std::u16string{u"<empty>"}, pp_value);
  } else {
    // Should be a v8::internal::Object at the address
    hr = sp_data_model_manager->CreateTypedObject(sp_ctx.get(), obj_address, sp_v8_object_type.get(), pp_value);
  }

  return hr;
}
