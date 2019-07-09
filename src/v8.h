#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

using MemReader = std::function<bool(uint64_t address, size_t size, uint8_t* buffer)>;

enum class PropertyType {
  Smi,
  UInt,
  Number,
  String,
  Bool,
  Address,
  TaggedPtr,
  TaggedPtrArray,
};

struct Property {
  Property(std::u16string propertyName, int value)
      : name(propertyName), smiValue(value), type(PropertyType::Smi) {}
  Property(std::u16string propertyName, std::u16string value)
      : name(propertyName), strValue(value), type(PropertyType::String) {}

  std::u16string name;
  PropertyType type;

  // strValue is used if the type is String or Object
  // An NativeObject will have its type in strValue, and pointer in addrValue
  std::u16string strValue;

  union {
    int smiValue;
    uint32_t uintValue;
    double numValue;
    bool boolValue;
    uint64_t addrValue;
  };

  size_t length; // Only relevant for TaggedPtrArray
};

struct V8HeapObject {
  uint64_t HeapAddress;
  bool IsSmi = false;
  std::u16string FriendlyName;  // e.g. string: "Hello, world"
  std::vector<Property> Properties;
};

V8HeapObject GetHeapObject(MemReader memReader, uint64_t address, uint64_t referringPointer);
