#pragma once

#include "dbgext.h"

inline const wchar_t* U16ToWChar(const char16_t *p_u16) {
  return reinterpret_cast<const wchar_t*>(p_u16);
}

inline const wchar_t* U16ToWChar(std::u16string& str) {
  return U16ToWChar(str.data());
}

#if defined(WIN32)
inline std::u16string ConvertToU16String(std::string utf8_string) {
  int len_chars = ::MultiByteToWideChar(CP_UTF8, 0, utf8_string.c_str(), -1, nullptr, 0);

  char16_t *p_buff = static_cast<char16_t*>(malloc(len_chars * sizeof(char16_t)));

  // On Windows wchar_t is the same a 16char_t
  static_assert(sizeof(wchar_t) == sizeof(char16_t));
  len_chars = ::MultiByteToWideChar(CP_UTF8, 0, utf8_string.c_str(), -1,
      reinterpret_cast<wchar_t*>(p_buff), len_chars);
  std::u16string result{p_buff};
  free(p_buff);

  return result;
}
#else
  #error String encoding conversion must be provided for the target platform.
#endif

HRESULT CreateProperty(
    IDataModelManager *p_manager,
    IModelPropertyAccessor *p_property,
    IModelObject **pp_property_object);

HRESULT CreateULong64(ULONG64 value, IModelObject **pp_int);

HRESULT CreateInt32(int value, IModelObject **pp_int);

HRESULT CreateUInt32(uint32_t value, IModelObject** pp_int);

HRESULT CreateBool(bool value, IModelObject **pp_val);

HRESULT CreateNumber(double value, IModelObject **pp_val);

HRESULT CreateString(std::u16string value, IModelObject **pp_val);

bool GetModelAtIndex(winrt::com_ptr<IModelObject>& sp_parent,
                     winrt::com_ptr<IModelObject>& sp_index,
                     IModelObject **p_result);

bool GetCurrentThread(winrt::com_ptr<IDebugHostContext>& sp_host_context,
                      IModelObject** p_current_thread);
