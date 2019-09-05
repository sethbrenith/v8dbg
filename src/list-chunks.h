#pragma once

#include <crtdbg.h>
#include <optional>
#include <string>
#include <vector>
#include "../utilities.h"
#include "extension.h"
#include "v8.h"

struct ListChunksAlias : winrt::implements<ListChunksAlias, IModelMethod> {
  HRESULT __stdcall Call(IModelObject* pContextObject, ULONG64 arg_count,
                         _In_reads_(arg_count) IModelObject** pp_arguments,
                         IModelObject** pp_result,
                         IKeyStore** pp_metadata) noexcept override;
};

struct ChunkData {
  winrt::com_ptr<IModelObject> area_start;
  winrt::com_ptr<IModelObject> area_end;
  winrt::com_ptr<IModelObject> space;
};


struct MemoryChunkIterator: winrt::implements<MemoryChunkIterator, IModelIterator> {
  MemoryChunkIterator(winrt::com_ptr<IDebugHostContext>& host_context): sp_ctx(host_context){};

  HRESULT PopulateChunkData();

  HRESULT __stdcall Reset() noexcept override {
    _RPT0(_CRT_WARN, "Reset called on MemoryChunkIterator\n");
    position = 0;
    return S_OK;
  }

  HRESULT __stdcall GetNext(IModelObject** object, ULONG64 dimensions,
                            IModelObject** indexers,
                            IKeyStore** metadata) noexcept override;

  ULONG position = 0;
  std::vector<ChunkData> chunks;
  winrt::com_ptr<IDebugHostContext> sp_ctx;
};

struct MemoryChunks
    : winrt::implements<MemoryChunks, IIndexableConcept, IIterableConcept> {
  // IIndexableConcept members
  HRESULT __stdcall GetDimensionality(
      IModelObject* context_object, ULONG64* dimensionality) noexcept override {
    *dimensionality = 1;
    return S_OK;
  }

  HRESULT __stdcall GetAt(IModelObject* context_object, ULONG64 indexer_count,
                          IModelObject** indexers, IModelObject** object,
                          IKeyStore** metadata) noexcept override {
    _RPT0(_CRT_WARN, "In IIndexableConcept::GetAt\n");
    if (indexer_count != 1) return E_INVALIDARG;
    if (metadata != nullptr) *metadata = nullptr;
    HRESULT hr = S_OK;
    winrt::com_ptr<IDebugHostContext> sp_ctx;
    hr = context_object->GetContext(sp_ctx.put());
    if (FAILED(hr)) return hr;

    // This should be instantiated once for each synthetic object returned,
    // so should be able to cache/reuse an iterator
    if (!opt_chunks.has_value()) {
      _RPT0(_CRT_WARN, "Caching memory chunks for the indexer\n");
      opt_chunks.emplace(sp_ctx);
      _ASSERT(opt_chunks.has_value());
      opt_chunks->PopulateChunkData();
    }

    VARIANT vt_index;
    hr = indexers[0]->GetIntrinsicValueAs(VT_UI8, &vt_index);
    if (FAILED(hr)) return hr;

    if (vt_index.ullVal >= opt_chunks->chunks.size()) return E_BOUNDS;

    ChunkData curr_chunk = opt_chunks->chunks.at(vt_index.ullVal);
    winrt::com_ptr<IModelObject> sp_value;
    hr = sp_data_model_manager->CreateSyntheticObject(sp_ctx.get(), sp_value.put());
    if (FAILED(hr)) return hr;
    hr = sp_value->SetKey(L"area_start", curr_chunk.area_start.get(), nullptr);
    if (FAILED(hr)) return hr;
    hr = sp_value->SetKey(L"area_end", curr_chunk.area_end.get(), nullptr);
    if (FAILED(hr)) return hr;
    hr = sp_value->SetKey(L"space", curr_chunk.space.get(), nullptr);
    if (FAILED(hr)) return hr;

    *object = sp_value.detach();
    return S_OK;
  }

  HRESULT __stdcall SetAt(IModelObject* context_object, ULONG64 indexer_count,
                          IModelObject** indexers,
                          IModelObject* value) noexcept override {
    _RPT0(_CRT_ERROR, "IndexableConcept::SetAt not implemented\n");
    return E_NOTIMPL;
  }

  // IIterableConcept
  HRESULT __stdcall GetDefaultIndexDimensionality(
      IModelObject* context_object, ULONG64* dimensionality) noexcept override {
    *dimensionality = 1;
    return S_OK;
  }

  HRESULT __stdcall GetIterator(IModelObject* context_object,
                                IModelIterator** iterator) noexcept override {
    _RPT0(_CRT_WARN, "In MemoryChunks::GetIterator\n");
    winrt::com_ptr<IDebugHostContext> sp_ctx;
    HRESULT hr = context_object->GetContext(sp_ctx.put());
    if (FAILED(hr)) return hr;
    auto sp_memory_iterator{winrt::make<MemoryChunkIterator>(sp_ctx)};
    *iterator = sp_memory_iterator.as<IModelIterator>().detach();
    return S_OK;
  }

  std::optional<MemoryChunkIterator> opt_chunks;
};
