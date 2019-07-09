# Architecture

This extension is layed out as follows:

- The `v8.{cpp,h}` files in this directory interoperate with the V8 postmortem
  debugging API. This is written to only depend on the standard library.
- The `object.{cpp,h}` files in this directory provide the integration
  between the WinDbg specific APIs and the generic V8 source files. This code
  can read raw bytes in memory and return WinDbg representations of objects.
- The `extension.{cpp,h}` files in this directory provide implementations for the
  CreateExtension and DestroyExtension methods the generic extension files in
  the root directory require, and provide the integration with the above source.

When the extension is initialized, it can register a model with a type, for
example `v8::internal::HeapObject`. When the debugger needs to display that type
it will call a property getter. That getter can return a "synthetic object"
which may contain other properties and types. These properties are typically
"boxed" VARIANTs, such as BSTR, UI4, etc.

The model/property getting should be implemented in extension.cpp, and should
ask `object.cpp` to get for example "v8::internal::HeapObject" at address 0x0100.

Object will then ask V8 for a representation, which will look-up the type in
the v8_debug_helper library, and construct dynamic properties based on this
result. It will then construct the object to return from the property getter.

The Object created should be recursive, in that it should create properties for
itself, then recursively for its base types. It should also create a brief display
string that provides a useful summary of the object (e.g. value of a smi or string, name of
a function or script, constructor name or first few props of a JSObject).
