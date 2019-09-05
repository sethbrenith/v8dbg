#include <Windows.h>
#include <crtdbg.h>
#include <sstream>
#include "extension.h"
#include "v8.h"
#include "../utilities.h"
#include "debug-helper.h"

namespace d = v8::debug_helper;

// We need a plain C function pointer for interop with v8_debug_helper. We can
// use this to get one as long as we never need two at once.
class MemReaderScope {
 public:
  explicit MemReaderScope(MemReader reader) {
    _ASSERTE(!reader_);
    reader_ = reader;
  }
  ~MemReaderScope() {
    reader_ = MemReader();
  }
  d::MemoryAccessor GetReader() {
    return &MemReaderScope::Read;
  }
 private:
  MemReaderScope(const MemReaderScope&) = delete;
  MemReaderScope& operator=(const MemReaderScope&) = delete;
  static d::MemoryAccessResult Read(uintptr_t address, uint8_t* destination, size_t byte_count) {
    bool result = reader_(address, byte_count, destination);
    // TODO determine when an address is valid but inaccessible
    return result ? d::MemoryAccessResult::kOk : d::MemoryAccessResult::kAddressNotValid;
  }
  static MemReader reader_;
};
MemReader MemReaderScope::reader_;

template <typename T>
T ReadBasicData(MemReader reader, uint64_t address) {
  T data{};
  reader(address, sizeof(data), reinterpret_cast<uint8_t*>(&data));
  return data;
}

std::u16string WidenString(const char* data) {
  std::basic_ostringstream<char16_t> stream;
  stream << data;
  return stream.str();
}

V8HeapObject GetHeapObject(MemReader mem_reader, uint64_t tagged_ptr, uint64_t referring_pointer) {
  // Read the value at the address, and see if it is a tagged pointer

  V8HeapObject obj;
  MemReaderScope reader_scope(mem_reader);

  d::Roots heap_roots = {0};
  // TODO ideally we'd provide real roots here. For now, just testing
  // decompression based on the pointer to wherever we found this value, which
  // is likely (though not guaranteed) to be a heap pointer itself.
  heap_roots.any_heap_pointer = referring_pointer;
  auto props = d::GetObjectProperties(tagged_ptr, reader_scope.GetReader(), heap_roots);
  obj.friendly_name = WidenString(props->brief);
  for (int property_index = 0; property_index < props->num_properties; ++property_index) {
    const auto& source_prop = *props->properties[property_index];
    //printf("%s: %s: %llx\n", source_prop.name, source_prop.type, source_prop.values[0].value);
    Property dest_prop(WidenString(source_prop.name), WidenString(source_prop.type), source_prop.address);
    if (source_prop.kind != d::PropertyKind::kSingle) {
      dest_prop.type = PropertyType::Array;
      dest_prop.length = source_prop.num_values;
    }
    // TODO indexed values
    obj.properties.push_back(dest_prop);
  }

  return obj;
}
