#pragma once

#include "../dbgext.h"
#include "extension.h"
#include "v8.h"
#include <vector>
#include <string>
#include <comutil.h>

// The representation of the underlying V8 object that will be cached on the
// DataModel representation. (Needs to implement IUnknown).
struct __declspec(uuid("6392E072-37BB-4220-A5FF-114098923A02")) IV8CachedObject: IUnknown {
  virtual HRESULT __stdcall GetCachedV8HeapObject(V8HeapObject** pp_heap_object) = 0;
};

struct V8CachedObject: winrt::implements<V8CachedObject, IV8CachedObject> {
  V8CachedObject(IModelObject* p_v8_object_instance) {
    Location loc;
    HRESULT hr = p_v8_object_instance->GetLocation(&loc);
    if(FAILED(hr)) return; // TODO error handling

    winrt::com_ptr<IDebugHostContext> sp_context;
    hr = p_v8_object_instance->GetContext(sp_context.put());
    if (FAILED(hr)) return;

    auto mem_reader = [&sp_context](uint64_t address, size_t size, uint8_t *p_buffer) {
      ULONG64 bytes_read;
      Location loc{address};
      HRESULT hr = Extension::current_extension_->sp_debug_host_memory_->ReadBytes(sp_context.get(), loc, p_buffer, size, &bytes_read);
      return SUCCEEDED(hr);
    };

    winrt::com_ptr<IDebugHostType> sp_type;
    _bstr_t type_name;
    bool compressed_pointer = SUCCEEDED(p_v8_object_instance->GetTypeInfo(sp_type.put()))
        && SUCCEEDED(sp_type->GetName(type_name.GetAddress()))
        && static_cast<wchar_t*>(type_name) == std::wstring(L"v8::internal::TaggedValue");

    uint64_t tagged_ptr;
    Extension::current_extension_->sp_debug_host_memory_->ReadPointers(sp_context.get(), loc, 1, &tagged_ptr);
    if (compressed_pointer) tagged_ptr = static_cast<uint32_t>(tagged_ptr);
    heap_object = ::GetHeapObject(mem_reader, tagged_ptr, loc.GetOffset());
  }

  V8HeapObject heap_object;

  HRESULT __stdcall GetCachedV8HeapObject(V8HeapObject** pp_heap_object) noexcept override {
    *pp_heap_object = &this->heap_object;
    return S_OK;
  }
};

struct V8ObjectKeyEnumerator: winrt::implements<V8ObjectKeyEnumerator, IKeyEnumerator>
{
  V8ObjectKeyEnumerator(winrt::com_ptr<IV8CachedObject> &v8_cached_object)
      :sp_v8_cached_object{v8_cached_object} {}

  int index = 0;
  winrt::com_ptr<IV8CachedObject> sp_v8_cached_object;

  HRESULT __stdcall Reset() noexcept override
  {
    index = 0;
    return S_OK;
  }

  // This method will be called with a nullptr 'value' for each key if returned
  // from an IDynamicKeyProviderConcept. It will call GetKey on the
  // IDynamicKeyProviderConcept interface after each key returned.
  HRESULT __stdcall GetNext(
      BSTR* key,
      IModelObject** value,
      IKeyStore** metadata
  ) noexcept override
  {
    V8HeapObject *p_v8_heap_object;
    HRESULT hr = sp_v8_cached_object->GetCachedV8HeapObject(&p_v8_heap_object);

    if (index >= p_v8_heap_object->properties.size()) return E_BOUNDS;

    auto name_ptr = p_v8_heap_object->properties[index].name.c_str();
    *key = ::SysAllocString(U16ToWChar(name_ptr));
    ++index;
    return S_OK;
  }
};

struct V8LocalDataModel: winrt::implements<V8LocalDataModel, IDataModelConcept> {
    HRESULT __stdcall InitializeObject(
        IModelObject* model_object,
        IDebugHostTypeSignature* matching_type_signature,
        IDebugHostSymbolEnumerator* wildcard_matches
    ) noexcept override
    {
        return S_OK;
    }

    HRESULT __stdcall GetName( BSTR* model_name) noexcept override
    {
        return E_NOTIMPL;
    }
};

struct V8ObjectDataModel: winrt::implements<V8ObjectDataModel, IDataModelConcept, IStringDisplayableConcept, IDynamicKeyProviderConcept>
{
    winrt::com_ptr<IV8CachedObject> GetCachedObject(IModelObject* context_object) {
      // Get the IModelObject for this parent object. As it is a dynamic provider,
      // there is only one parent directly on the object.
      winrt::com_ptr<IModelObject> sp_parent_model, sp_context_adjuster;
      HRESULT hr = context_object->GetParentModel(0, sp_parent_model.put(), sp_context_adjuster.put());

      // See if the cached object is already present
      winrt::com_ptr<IUnknown> sp_context;
      hr = context_object->GetContextForDataModel(sp_parent_model.get(), sp_context.put());

      winrt::com_ptr<IV8CachedObject> sp_v8_cached_object;

      if(SUCCEEDED(hr)) {
        sp_v8_cached_object = sp_context.as<IV8CachedObject>();
      } else {
        sp_v8_cached_object = winrt::make<V8CachedObject>(context_object);
        sp_v8_cached_object.as(sp_context);
        context_object->SetContextForDataModel(sp_parent_model.get(), sp_context.get());
      }

      return sp_v8_cached_object;
    }

    HRESULT __stdcall InitializeObject(
        IModelObject* model_object,
        IDebugHostTypeSignature* matching_type_signature,
        IDebugHostSymbolEnumerator* wildcard_matches
    ) noexcept override
    {
        return S_OK;
    }

    HRESULT __stdcall GetName( BSTR* model_name) noexcept override
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall ToDisplayString(
        IModelObject* context_object,
        IKeyStore* metadata,
        BSTR* display_string
    ) noexcept override
    {
      winrt::com_ptr<IV8CachedObject> sp_v8_cached_object = GetCachedObject(context_object);
      V8HeapObject* p_v8_heap_object;
      HRESULT hr = sp_v8_cached_object->GetCachedV8HeapObject(&p_v8_heap_object);
      *display_string = ::SysAllocString(reinterpret_cast<wchar_t*>(p_v8_heap_object->friendly_name.data()));
      return S_OK;
    }

    // IDynamicKeyProviderConcept
    HRESULT __stdcall GetKey(
        IModelObject *context_object,
        PCWSTR key,
        IModelObject** key_value,
        IKeyStore** metadata,
        bool *has_key
    ) noexcept override
    {
      winrt::com_ptr<IV8CachedObject> sp_v8_cached_object = GetCachedObject(context_object);
      V8HeapObject* p_v8_heap_object;
      HRESULT hr = sp_v8_cached_object->GetCachedV8HeapObject(&p_v8_heap_object);

      *has_key = false;
      for(auto &k: p_v8_heap_object->properties) {
        winrt::com_ptr<IDebugHostType> sp_v8_object;
        winrt::com_ptr<IDebugHostContext> sp_ctx;

        const char16_t *p_key = reinterpret_cast<const char16_t*>(key);
        if (k.name.compare(p_key) == 0) {
          *has_key = true;
          if(key_value != nullptr) {
            winrt::com_ptr<IModelObject> sp_value;
            // TODO: if this property was a compressed pointer, then can we
            // somehow keep its uncompressed type? That would let us supply
            // a type hint on subsequent calls, which is good for working in
            // partial dumps.
            hr = context_object->GetContext(sp_ctx.put());
            if (FAILED(hr)) return hr;
            sp_v8_object = Extension::current_extension_->GetV8ObjectType(sp_ctx, k.type_name.c_str());
            if (sp_v8_object == nullptr) return E_FAIL;

            if (k.type == PropertyType::kArray) {
              ULONG64 object_size{};
              sp_v8_object->GetSize(&object_size);
              ArrayDimension dimensions[] = {{/*start=*/0, /*length=*/k.length, /*stride=*/object_size}};
              winrt::com_ptr<IDebugHostType> sp_v8_object_array;
              sp_v8_object->CreateArrayOf(/*dimensions=*/1, dimensions, sp_v8_object_array.put());
              sp_v8_object = sp_v8_object_array;
            }

            sp_data_model_manager->CreateTypedObject(sp_ctx.get(), Location{k.addr_value},
                sp_v8_object.get(), sp_value.put());
            *key_value = sp_value.detach();
          }
          return S_OK;
        }
      }

      // TODO: Should this be E_* if not found?
      return S_OK;
    }

    HRESULT __stdcall SetKey(
        IModelObject *context_object,
        PCWSTR key,
        IModelObject *key_value,
        IKeyStore *metadata
    ) noexcept override
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall EnumerateKeys(
        IModelObject *context_object,
        IKeyEnumerator **pp_enumerator
    ) noexcept override
    {
      auto sp_v8_cached_object = GetCachedObject(context_object);

      auto enumerator{ winrt::make<V8ObjectKeyEnumerator>(sp_v8_cached_object)};
      *pp_enumerator = enumerator.detach();
      return S_OK;
    }
};

struct V8LocalValueProperty: winrt::implements<V8LocalValueProperty, IModelPropertyAccessor>
{
    HRESULT __stdcall GetValue(
        PCWSTR pwsz_key,
        IModelObject *p_v8_object_instance,
        IModelObject **pp_value);

    HRESULT __stdcall SetValue(
        PCWSTR /*pwsz_key*/,
        IModelObject * /*p_process_instance*/,
        IModelObject * /*p_value*/)
    {
        return E_NOTIMPL;
    }
};
