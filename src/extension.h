#pragma once

#include "../utilities.h"
#include <unordered_set>

class Extension {
 public:
  bool Initialize();
  ~Extension();
  winrt::com_ptr<IDebugHostModule> GetV8Module(winrt::com_ptr<IDebugHostContext>& spCtx);
  winrt::com_ptr<IDebugHostType> Extension::GetV8ObjectType(winrt::com_ptr<IDebugHostContext>& spCtx, const char16_t* typeName = u"v8::internal::Object");
  void TryRegisterType(winrt::com_ptr<IDebugHostType>& spType, std::u16string typeName);
  static Extension* currentExtension;

  winrt::com_ptr<IDebugHostMemory2> spDebugHostMemory;
  winrt::com_ptr<IDebugHostSymbols> spDebugHostSymbols;
  winrt::com_ptr<IDebugHostExtensibility> spDebugHostExtensibility;

  winrt::com_ptr<IDebugHostTypeSignature> spLocalTypeSignature;
  winrt::com_ptr<IDebugHostTypeSignature> spMaybeLocalTypeSignature;
  winrt::com_ptr<IDebugHostTypeSignature> spHandleTypeSignature;
  winrt::com_ptr<IDebugHostTypeSignature> spMaybeHandleTypeSignature;
  winrt::com_ptr<IModelObject> spObjectDataModel;
  winrt::com_ptr<IModelObject> spLocalDataModel;
  winrt::com_ptr<IModelObject> spCurrIsolateModel;
  winrt::com_ptr<IModelObject> spListChunksModel;

 private:
  winrt::com_ptr<IDebugHostModule> spV8Module;
  std::unordered_map<std::u16string, winrt::com_ptr<IDebugHostType>> spV8ObjectTypes;
  std::unordered_map<std::u16string, winrt::com_ptr<IDebugHostTypeSignature>> registered_handler_types;
  winrt::com_ptr<IDebugHostContext> spV8ModuleCtx;
  ULONG v8ModuleProcId;
};
