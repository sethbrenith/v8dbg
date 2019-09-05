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

V8HeapObject GetHeapObject(MemReader memReader, uint64_t taggedPtr, uint64_t referringPointer, const std::string& type_name) {
  // Read the value at the address, and see if it is a tagged pointer

  V8HeapObject obj;
  MemReaderScope reader_scope(memReader);

  d::Roots heap_roots = {0};
  // TODO ideally we'd provide real roots here. For now, just testing
  // decompression based on the pointer to wherever we found this value, which
  // is likely (though not guaranteed) to be a heap pointer itself.
  heap_roots.any_heap_pointer = referringPointer;
  const char* type_name_ptr = nullptr;
  if (!type_name.empty()) {
    type_name_ptr = type_name.c_str();
  }
  auto props = d::GetObjectProperties(taggedPtr, reader_scope.GetReader(), heap_roots, type_name_ptr);
  obj.FriendlyName = WidenString(props->brief);
  for (int propertyIndex = 0; propertyIndex < props->num_properties; ++propertyIndex) {
    const auto& sourceProp = *props->properties[propertyIndex];
    //printf("%s: %s: %llx\n", sourceProp.name, sourceProp.type, sourceProp.values[0].value);
    Property destProp(WidenString(sourceProp.name), WidenString(sourceProp.type));
    destProp.type = PropertyType::TaggedPtr;
    if (sourceProp.kind != d::PropertyKind::kSingle) {
      destProp.type = PropertyType::TaggedPtrArray;
    }
    destProp.addrValue = sourceProp.address;
    destProp.length = sourceProp.num_values;
    // TODO indexed values
    obj.Properties.push_back(destProp);
  }

  // For each guessed type, create a synthetic property that will request data about
  // the same object again but with a more specific type hint. This only works
  // in non-compressed-pointers mode.
  for (int guessedTypeIndex = 0; guessedTypeIndex < props->num_guessed_types; ++guessedTypeIndex) {
    Property destProp(WidenString(("guessed type " + std::to_string(guessedTypeIndex)).c_str()), WidenString(props->guessed_types[guessedTypeIndex]));
    destProp.type = PropertyType::TaggedPtr;
    destProp.addrValue = referringPointer;
    destProp.length = 1;
    obj.Properties.push_back(destProp);
  }

  return obj;
}
