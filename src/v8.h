#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

using MemReader = 
  std::function<bool(uint64_t address, size_t size, uint8_t* buffer)>;

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
  Property(std::u16string property_name, int value)
      : name(property_name), smi_value(value), type(PropertyType::Smi) {}
  Property(std::u16string property_name, std::u16string value)
      : name(property_name), str_value(value), type(PropertyType::String) {}

  std::u16string name;
  PropertyType type;

  // str_value is used if the type is String or Object
  // A NativeObject will have its type in str_value, and pointer in addr_value
  std::u16string str_value;

  union {
    int smi_value;
    uint32_t uint_value;
    double num_value;
    bool bool_value;
    uint64_t addr_value;
  };

  size_t length = 0; // Only relevant for TaggedPtrArray
};

struct V8HeapObject {
  uint64_t HeapAddress;
  bool IsSmi = false;
  std::u16string FriendlyName;  // e.g. string: "Hello, world"
  std::vector<Property> Properties;
};

V8HeapObject GetHeapObject(MemReader mem_reader, uint64_t address, uint64_t referring_pointer);
