#include "list-chunks.h"
#include "curisolate.h"

// v8dbg!ListChunksAlias::Call
HRESULT __stdcall ListChunksAlias::Call(IModelObject* p_context_object,
                                        ULONG64 arg_count,
                                        _In_reads_(arg_count)
                                            IModelObject** pp_arguments,
                                        IModelObject** pp_result,
                                        IKeyStore** pp_metadata) noexcept {
  HRESULT hr = S_OK;

  winrt::com_ptr<IDebugHostContext> sp_ctx;
  hr = sp_debug_host->GetCurrentContext(sp_ctx.put());
  if (FAILED(hr)) return hr;

  hr = sp_data_model_manager->CreateSyntheticObject(sp_ctx.get(), pp_result);
  if (FAILED(hr)) return hr;

  auto sp_iterator{winrt::make<MemoryChunks>()};
  auto sp_indexable_concept = sp_iterator.as<IIndexableConcept>();
  auto sp_iterable_concept = sp_iterator.as<IIterableConcept>();

  hr = (*pp_result)->SetConcept(__uuidof(IIndexableConcept), sp_indexable_concept.get(), nullptr);
  if (FAILED(hr)) return hr;
  hr = (*pp_result)->SetConcept(__uuidof(IIterableConcept), sp_iterable_concept.get(), nullptr);
  if (FAILED(hr)) return hr;
  return hr;
}

HRESULT MemoryChunkIterator::PopulateChunkData() {
  winrt::com_ptr<IModelObject> sp_isolate, sp_heap, sp_space;
  chunks.clear();

  HRESULT hr = GetCurrentIsolate(sp_isolate);
  if (FAILED(hr)) return hr;

  hr = sp_isolate->GetRawValue(SymbolField, L"heap_", RawSearchNone, sp_heap.put());
  hr = sp_heap->GetRawValue(SymbolField, L"space_", RawSearchNone, sp_space.put());
  if (FAILED(hr)) return hr;

  winrt::com_ptr<IDebugHostType> sp_space_type;
  hr = sp_space->GetTypeInfo(sp_space_type.put());
  if (FAILED(hr)) return hr;

  // Iterate over the array of Space pointers
  winrt::com_ptr<IIterableConcept> sp_iterable;
  hr = sp_space->GetConcept(__uuidof(IIterableConcept),
                           reinterpret_cast<IUnknown**>(sp_iterable.put()),
                           nullptr);
  if (FAILED(hr)) return hr;

  winrt::com_ptr<IModelIterator> sp_space_iterator;
  hr = sp_iterable->GetIterator(sp_space.get(), sp_space_iterator.put());
  if (FAILED(hr)) return hr;

  // Loop through all the spaces in the array
  winrt::com_ptr<IModelObject> sp_space_ptr;
  while (sp_space_iterator->GetNext(sp_space_ptr.put(), 0, nullptr, nullptr) != E_BOUNDS) {
    // Should have gotten a "v8::internal::Space *". Dereference, then get field
    // "memory_chunk_list_" [Type: v8::base::List<v8::internal::MemoryChunk>]
    winrt::com_ptr<IModelObject> sp_space, sp_chunk_list, sp_mem_chunk_ptr, sp_mem_chunk;
    hr = sp_space_ptr->Dereference(sp_space.put());
    if (FAILED(hr)) return hr;
    hr = sp_space->GetRawValue(SymbolField, L"memory_chunk_list_", RawSearchNone, sp_chunk_list.put());
    if (FAILED(hr)) return hr;

    // Then get field "front_" [Type: v8::internal::MemoryChunk *]
    hr = sp_chunk_list->GetRawValue(SymbolField, L"front_", RawSearchNone,
                                  sp_mem_chunk_ptr.put());
    if (FAILED(hr)) return hr;

    // Loop here on the list of MemoryChunks for the space
    while (true) {
      // See if it is a nullptr (i.e. no chunks in this space)
      VARIANT vt_front_val;
      hr = sp_mem_chunk_ptr->GetIntrinsicValue(&vt_front_val);
      if (FAILED(hr) || vt_front_val.vt != VT_UI8) return E_FAIL;
      if (vt_front_val.ullVal == 0) {
        break;
      }

      // Dereference and get fields "area_start_" and "area_end_" (both uint64)
      hr = sp_mem_chunk_ptr->Dereference(sp_mem_chunk.put());
      if (FAILED(hr)) return hr;

      winrt::com_ptr<IModelObject> sp_start, sp_end;
      hr = sp_mem_chunk->GetRawValue(SymbolField, L"area_start_", RawSearchNone, sp_start.put());
      if (FAILED(hr)) return hr;
      hr = sp_mem_chunk->GetRawValue(SymbolField, L"area_end_", RawSearchNone, sp_end.put());
      if (FAILED(hr)) return hr;

      VARIANT vt_start, vt_end;
      hr = sp_start->GetIntrinsicValue(&vt_start);
      if (FAILED(hr) || vt_start.vt != VT_UI8) return E_FAIL;
      hr = sp_end->GetIntrinsicValue(&vt_end);
      if (FAILED(hr) || vt_end.vt != VT_UI8) return E_FAIL;

      ChunkData chunk_entry;
      chunk_entry.area_start = sp_start;
      chunk_entry.area_end = sp_end;
      chunk_entry.space = sp_space;
      chunks.push_back(chunk_entry);

      // Follow the list_node_.next_ to the next memory chunk
      winrt::com_ptr<IModelObject> sp_list_node;
      hr = sp_mem_chunk->GetRawValue(SymbolField, L"list_node_", RawSearchNone, sp_list_node.put());
      if (FAILED(hr)) return hr;

      sp_mem_chunk_ptr = nullptr;
      sp_mem_chunk = nullptr;
      hr = sp_list_node->GetRawValue(SymbolField, L"next_", RawSearchNone, sp_mem_chunk_ptr.put());
      if (FAILED(hr)) return hr;
      // Top of the loop will check if this is a nullptr and exit if so
    }
    sp_space_ptr = nullptr;
    sp_space = nullptr;
  }

  return S_OK;
}

HRESULT MemoryChunkIterator::GetNext(IModelObject** object, ULONG64 dimensions,
                                     IModelObject** indexers,
                                     IKeyStore** metadata) noexcept {
  _RPT1(_CRT_WARN, "In GetNext. Dimensions = %d\n", dimensions);
  HRESULT hr = S_OK;
  if (dimensions > 1) return E_INVALIDARG;

  if (position == 0) {
    hr = PopulateChunkData();
    if (FAILED(hr)) return hr;
  }
  if (position >= chunks.size()) return E_BOUNDS;

  if (metadata != nullptr) *metadata = nullptr;

  winrt::com_ptr<IModelObject> sp_index, sp_value;

  if (dimensions == 1) {
    hr = CreateULong64(position, sp_index.put());
    if (FAILED(hr)) return hr;
    *indexers = sp_index.detach();
  }

  // Create the synthetic object representing the chunk here
  ChunkData& curr_chunk = chunks.at(position++);
  hr = sp_data_model_manager->CreateSyntheticObject(sp_ctx.get(), sp_value.put());
  if (FAILED(hr)) return hr;
  hr = sp_value->SetKey(L"area_start", curr_chunk.area_start.get(), nullptr);
  if (FAILED(hr)) return hr;
  hr = sp_value->SetKey(L"area_end", curr_chunk.area_end.get(), nullptr);
  if (FAILED(hr)) return hr;
  hr = sp_value->SetKey(L"space", curr_chunk.space.get(), nullptr);
  if (FAILED(hr)) return hr;

  *object = sp_value.detach();
  return hr;
}
