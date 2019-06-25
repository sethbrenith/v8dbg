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

V8HeapObject GetHeapObject(MemReader memReader, uint64_t taggedPtr, uint64_t referringPointer) {
  // Read the value at the address, and see if it is a tagged pointer

  V8HeapObject obj;
  MemReaderScope reader_scope(memReader);

  d::Roots heap_roots = {0};
  // TODO ideally we'd provide real roots here. For now, just testing
  // decompression based on the pointer to wherever we found this value, which
  // is likely (though not guaranteed) to be a heap pointer itself.
  heap_roots.any_heap_pointer = referringPointer;
  auto props = d::GetObjectProperties(taggedPtr, reader_scope.GetReader(), heap_roots);
  obj.FriendlyName = WidenString(props->brief);
  for (int propertyIndex = 0; propertyIndex < props->num_properties; ++propertyIndex) {
    const auto& sourceProp = *props->properties[propertyIndex];
    //printf("%s: %s: %llx\n", sourceProp.name, sourceProp.type, sourceProp.values[0].value);
    Property destProp(WidenString(sourceProp.name), WidenString(sourceProp.type));
    destProp.type = PropertyType::TaggedPtr;
    destProp.addrValue = sourceProp.address;
    // TODO indexed values
    obj.Properties.push_back(destProp);
  }

  return obj;

  // Check for non-objects first
  if (!(taggedPtr & 0x01)) {
    if (taggedPtr == 0xCCCCCCCCCCCCCCCC)
    {
      obj.HeapAddress = taggedPtr;
      obj.FriendlyName = u"<uninitialized>";
    } else {
      // TODO: Only handles 64-bit platforms with uncompressed pointers currently
      obj.IsSmi = true;
      obj.HeapAddress = static_cast<int>(static_cast<intptr_t>(taggedPtr) >> 32);
      std::wstring smiValue = std::to_wstring(obj.HeapAddress);
      obj.FriendlyName = std::u16string{u"<Smi>: "} + reinterpret_cast<const char16_t*>(smiValue.c_str());
    }
    return obj;
  }

  obj.HeapAddress = UnTagPtr(taggedPtr);
  // The first value in the Object's memory space is the Map pointer.
  // This is also tagged, so clear the low 2 bits.
  ReadTypeFromMemory(memReader, obj.HeapAddress, &obj.Map.HeapAddress);
  obj.Map.HeapAddress = UnTagPtr(obj.Map.HeapAddress);

  uint64_t instanceTypeAddr = obj.Map.HeapAddress + 12;
  ReadTypeFromMemory(memReader, instanceTypeAddr, &obj.Map.InstanceType);

  V8::Layout::V8Layout& v8Layout = Extension::currentExtension->v8Layout;

  auto typeName = v8Layout.InstanceTypeToTypeName.find(obj.Map.InstanceType);

  if (typeName == v8Layout.InstanceTypeToTypeName.end()) {
    // Couldn't find the type of object from the map. Not much else can be done.
    auto instStr = std::to_wstring(obj.Map.InstanceType);
    obj.FriendlyName = std::u16string{u"Unknown instanceType: "} + reinterpret_cast<char16_t*>(instStr.data());
    return obj;
  }

  obj.Map.TypeName = typeName->second;

  // Construct the dynamic properties by looping through the classes (base
  // first) and adding to the vector.
  auto objectDesc = v8Layout.TypeNameToDescriptor.find(typeName->second);
  if (objectDesc == v8Layout.TypeNameToDescriptor.end() ) {
    obj.FriendlyName = typeName->second + u": NO DESCRIPTOR";
    // TODO: Eventually assert here if the type can't be found?
    _RPTFWN(_CRT_WARN, L"Couldn't find object descriptor for type %s", U16ToWChar(typeName->second));
    return obj;
  }

  // Loop up the base types adding the properties starting from the base.
  // Note: Need to use std::function and capture itself to call recursively.
  std::function<void(std::u16string)> addProps = [&obj, &v8Layout, &addProps, &memReader](std::u16string& type){
    // Try to find the descriptor for this type
    auto typeToDesc = v8Layout.TypeNameToDescriptor.find(type);
    if (typeToDesc == v8Layout.TypeNameToDescriptor.end() ) {
      obj.FriendlyName = type + u": NO LAYOUT INFO";
      // TODO: Eventually assert here if the type can't be found?
      _RPTFWN(_CRT_WARN, L"Couldn't find object descriptor for type %s", U16ToWChar(type));
      return;
    }

    // If it has a base-class, process that first
    if (typeToDesc->second.baseType != u"") addProps(typeToDesc->second.baseType);

    // Add the properties
    V8::Layout::ObjectDesc& descriptor = typeToDesc->second;
    for(auto& prop: descriptor.fields) {
      if (prop.type == V8::Layout::FieldType::kTaggedSize) {
        Property heapObject{prop.name, 0};
        // This is the actual location in memory of the taggedPtr
        uint64_t valueAddr = obj.HeapAddress + prop.offset;
        heapObject.addrValue = valueAddr;
        heapObject.type = PropertyType::TaggedPtr;
        obj.Properties.push_back(heapObject);
      } else if (prop.type == V8::Layout::FieldType::kInt32Size) {
        // Read the value at the address
        uint64_t valueAddr = obj.HeapAddress + prop.offset;
        int32_t value;
        ReadTypeFromMemory(memReader, valueAddr, &value);
        obj.Properties.push_back(Property{prop.name, value});
      } else if (prop.type == V8::Layout::FieldType::kUInt8Size) {
        // Read the value at the address
        uint64_t valueAddr = obj.HeapAddress + prop.offset;
        uint8_t value;
        ReadTypeFromMemory(memReader, valueAddr, &value);
        obj.Properties.push_back(Property{prop.name, value});
      } else if (prop.type == V8::Layout::FieldType::kUInt16Size) {
        // Read the value at the address
        uint64_t valueAddr = obj.HeapAddress + prop.offset;
        uint16_t value;
        ReadTypeFromMemory(memReader, valueAddr, &value);
        obj.Properties.push_back(Property{prop.name, value});
      } else if (prop.type == V8::Layout::FieldType::kUInt32Size) {
        // Read the value at the address
        uint64_t valueAddr = obj.HeapAddress + prop.offset;
        Property theProp{prop.name, 0};
        theProp.type = PropertyType::UInt;
        ReadTypeFromMemory(memReader, valueAddr, &theProp.uintValue);
        obj.Properties.push_back(theProp);
      } else if (prop.type == V8::Layout::FieldType::kInt32Size) {
        // Read the value at the address
        uint64_t valueAddr = obj.HeapAddress + prop.offset;
        int32_t value;
        ReadTypeFromMemory(memReader, valueAddr, &value);
        obj.Properties.push_back(Property{prop.name, value});
      } else {
        // TODO Other types
        _RPTF0(_CRT_WARN, "Data type not yet implemented");
        obj.Properties.push_back(Property{prop.name, u"[Unknown type!]"});
      }
    }
    obj.FriendlyName = u"<" + type + u">";
  };

  addProps(typeName->second);

  if (typeName->second.compare(u"v8::internal::SeqOneByteString") == 0) {
    int strLength;
    ReadTypeFromMemory(memReader, obj.HeapAddress + 12, &strLength);
    uint64_t strAddress = obj.HeapAddress + 16;

    // Need to get the single-byte representation and convert
    // Create a null initialized string one longer than the length
    std::string strValueSingleByte(static_cast<size_t>(strLength + 1), '\0');
    memReader(strAddress, strLength, reinterpret_cast<uint8_t*>(strValueSingleByte.data()));
    std::u16string strValue = ConvertToU16String(strValueSingleByte);

    obj.Properties.push_back(Property{u"Value", strValue});
    obj.FriendlyName = std::u16string{u"<SeqOneByteString>: "} + strValue;
  }

  if (typeName->second.compare(u"v8::internal::Oddball") == 0) {
    for(auto& prop: obj.Properties) {
      if(prop.name.compare(u"Kind") == 0) {
        // Kind is a tagged value, so stored as a Smi
        _ASSERTE(prop.type == PropertyType::TaggedPtr);
        uint64_t taggedVal;
        ReadTypeFromMemory(memReader, prop.addrValue, &taggedVal);
        int kind = UntagSmi(taggedVal);
        auto kindName = v8Layout.OddballKindToName.find(kind);
        if (kindName != v8Layout.OddballKindToName.end()) {
          obj.FriendlyName = u"<Oddball>" + (*kindName).second;
        }
        break;
      }
    }
  }

  return obj;
}
