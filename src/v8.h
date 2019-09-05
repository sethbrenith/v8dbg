#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

using MemReader =
  std::function<bool(uint64_t address, size_t size, uint8_t* buffer)>;

enum class PropertyType {
  kPointer,
  kArray,
};

struct Property {
  Property(std::u16string property_name, std::u16string type_name, uint64_t address)
      : name(property_name), type_name(type_name), type(PropertyType::kPointer), addr_value(address) {}

  std::u16string name;
  PropertyType type;
  std::u16string type_name;
  uint64_t addr_value;
  size_t length = 0; // Only relevant for PropertyType::kArray
};

struct V8HeapObject {
  std::u16string friendly_name;  // String to print in single-line description.
  std::vector<Property> properties;
};

V8HeapObject GetHeapObject(MemReader mem_reader, uint64_t address, uint64_t referring_pointer);
