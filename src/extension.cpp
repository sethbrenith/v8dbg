#include "../utilities.h"
#include "extension.h"
#include "curisolate.h"
#include "object.h"
#include <iostream>

Extension* Extension::currentExtension = nullptr;
const wchar_t *pcurIsolate = L"curisolate";

bool CreateExtension() {
  _RPTF0(_CRT_WARN, "Entered CreateExtension\n");
  if (Extension::currentExtension != nullptr || spDataModelManager == nullptr ||
      spDebugHost == nullptr) {
    return false;
  } else {
    Extension* newExtension = new (std::nothrow) Extension();
    if (newExtension && newExtension->Initialize()) {
      Extension::currentExtension = newExtension;
      return true;
    } else {
      delete newExtension;
      return false;
    }
  }
}

void DestroyExtension() {
  _RPTF0(_CRT_WARN, "Entered DestroyExtension\n");
  if (Extension::currentExtension != nullptr) {
    delete Extension::currentExtension;
    Extension::currentExtension = nullptr;
  }
  return;
}

bool DoesTypeDeriveFromObject(winrt::com_ptr<IDebugHostType>& spType) {
  _bstr_t name;
  HRESULT hr = spType->GetName(name.GetAddress());
  if (!SUCCEEDED(hr)) return false;
  if (std::wstring(static_cast<wchar_t*>(name)) == L"v8::internal::Object") return true;

  winrt::com_ptr<IDebugHostSymbolEnumerator> spSuperClassEnumerator;
  hr = spType->EnumerateChildren(SymbolKind::SymbolBaseClass, nullptr, spSuperClassEnumerator.put());
  if (!SUCCEEDED(hr)) return false;

  while (true) {
    winrt::com_ptr<IDebugHostSymbol> spTypeSymbol;
    if (spSuperClassEnumerator->GetNext(spTypeSymbol.put()) != S_OK) break;
    winrt::com_ptr<IDebugHostBaseClass> spBaseClass = spTypeSymbol.as<IDebugHostBaseClass>();
    winrt::com_ptr<IDebugHostType> spBaseType;
    hr = spBaseClass->GetType(spBaseType.put());
    if (!SUCCEEDED(hr)) continue;
    if (DoesTypeDeriveFromObject(spBaseType)) {
      return true;
    }
  }

  return false;
}

void Extension::TryRegisterType(winrt::com_ptr<IDebugHostType>& spType, std::u16string typeName) {
  auto insertion_result = registered_handler_types.insert({typeName, nullptr});
  if (!insertion_result.second) return;
  if (DoesTypeDeriveFromObject(spType)) {
    winrt::com_ptr<IDebugHostTypeSignature> spObjectTypeSignature;
    HRESULT hr = spDebugHostSymbols->CreateTypeSignature(reinterpret_cast<const wchar_t*>(typeName.c_str()), nullptr,
                                            spObjectTypeSignature.put());
    if (FAILED(hr)) return;
    hr = spDataModelManager->RegisterModelForTypeSignature(
        spObjectTypeSignature.get(), spObjectDataModel.get());
    insertion_result.first->second = spObjectTypeSignature;
  }
}

winrt::com_ptr<IDebugHostType> Extension::GetV8ObjectType(winrt::com_ptr<IDebugHostContext>& spCtx, const char16_t* typeName) {
  bool isEqual;
  if (spV8ModuleCtx == nullptr || !SUCCEEDED(spV8ModuleCtx->IsEqualTo(spCtx.get(), &isEqual)) || !isEqual) {
    // Context changed; clear the dictionary.
    spV8ObjectTypes.clear();
  }

  GetV8Module(spCtx); // Will force the correct module to load
  if (spV8Module == nullptr) return nullptr;

  auto& dictionaryEntry = spV8ObjectTypes[typeName];
  if (dictionaryEntry == nullptr) {
    HRESULT hr = spV8Module->FindTypeByName(reinterpret_cast<PCWSTR>(typeName), dictionaryEntry.put());
    if (SUCCEEDED(hr)) {
      // It's too slow to enumerate all types in the v8 module up front and
      // register type handlers for all of them, but we can opportunistically do
      // so here for any types that we happen to be using. This makes the user
      // experience a little nicer because you can avoid opening one extra level
      // of data.
      TryRegisterType(dictionaryEntry, typeName);
    }
  }
  return dictionaryEntry;
}

winrt::com_ptr<IDebugHostModule> Extension::GetV8Module(winrt::com_ptr<IDebugHostContext>& spCtx) {
  // Return the cached version if it exists and the context is the same
  if (spV8Module != nullptr) {
    bool isEqual;
    if (SUCCEEDED(spV8ModuleCtx->IsEqualTo(spCtx.get(), &isEqual)) && isEqual) {
      return spV8Module;
    } else {
      spV8Module = nullptr;
      spV8ModuleCtx = nullptr;
    }
  }

  // Loop through the modules looking for the one that holds the "isolate_key_"
  winrt::com_ptr<IDebugHostSymbolEnumerator> spEnum;
  if (SUCCEEDED(spDebugHostSymbols->EnumerateModules(spCtx.get(), spEnum.put()))) {
    HRESULT hr = S_OK;
    while (true) {
      winrt::com_ptr<IDebugHostSymbol> spModSym;
      hr = spEnum->GetNext(spModSym.put());
      // hr == E_BOUNDS : hit the end of the enumerator
      // hr == E_ABORT  : a user interrupt was requested
      if (FAILED(hr)) break;
      winrt::com_ptr<IDebugHostModule> spModule;
      if (spModSym.try_as(spModule)) /* should always succeed */
      {
        winrt::com_ptr<IDebugHostSymbol> spIsolateSym;
        // The below symbol is specific to the main V8 module
        hr = spModule->FindSymbolByName(L"isolate_key_", spIsolateSym.put());
        if (SUCCEEDED(hr)) {
          spV8Module = spModule;
          spV8ModuleCtx = spCtx;
          break;
        }
      }
    }
  }
  // This will be the located module, or still nullptr if above fails
  return spV8Module;
}

bool Extension::Initialize() {
  _RPTF0(_CRT_WARN, "Entered ExtensionInitialize\n");

  if (!spDebugHost.try_as(spDebugHostMemory)) return false;
  if (!spDebugHost.try_as(spDebugHostSymbols)) return false;
  if (!spDebugHost.try_as(spDebugHostExtensibility)) return false;

  // Create an instance of the DataModel 'parent' for v8::internal::Object types
  auto objectDataModel{winrt::make<V8ObjectDataModel>()};
  HRESULT hr = spDataModelManager->CreateDataModelObject(
      objectDataModel.get(), spObjectDataModel.put());
  if (FAILED(hr)) return false;
  hr = spObjectDataModel->SetConcept(__uuidof(IStringDisplayableConcept),
                                     objectDataModel.get(), nullptr);
  if (FAILED(hr)) return false;
  auto iDynamic = objectDataModel.as<IDynamicKeyProviderConcept>();
  hr = spObjectDataModel->SetConcept(__uuidof(IDynamicKeyProviderConcept),
                                     iDynamic.get(), nullptr);
  if (FAILED(hr)) return false;

  // Parent the model for the type
  for (const char16_t* name : {u"v8::internal::Object", u"v8::internal::TaggedValue"}) {
    winrt::com_ptr<IDebugHostTypeSignature> spObjectTypeSignature;
    hr = spDebugHostSymbols->CreateTypeSignature(reinterpret_cast<const wchar_t*>(name), nullptr,
                                            spObjectTypeSignature.put());
    if (FAILED(hr)) return false;
    hr = spDataModelManager->RegisterModelForTypeSignature(
        spObjectTypeSignature.get(), spObjectDataModel.get());
    registered_handler_types[name] = spObjectTypeSignature;
  }

  // Create an instance of the DataModel 'parent' class for v8::Local<*> types
  auto localDataModel{winrt::make<V8LocalDataModel>()};
  // Create an IModelObject out of it
  hr = spDataModelManager->CreateDataModelObject(localDataModel.get(),
                                                 spLocalDataModel.put());
  if (FAILED(hr)) return false;

  // Create a type signature for the v8::Local symbol
  hr = spDebugHostSymbols->CreateTypeSignature(L"v8::Local<*>", nullptr,
                                          spLocalTypeSignature.put());
  if (FAILED(hr)) return false;
  hr = spDebugHostSymbols->CreateTypeSignature(L"v8::MaybeLocal<*>", nullptr,
                                          spMaybeLocalTypeSignature.put());
  if (FAILED(hr)) return false;
  hr = spDebugHostSymbols->CreateTypeSignature(L"v8::internal::Handle<*>", nullptr,
                                          spHandleTypeSignature.put());
  if (FAILED(hr)) return false;
  hr = spDebugHostSymbols->CreateTypeSignature(L"v8::internal::MaybeHandle<*>", nullptr,
                                          spMaybeHandleTypeSignature.put());
  if (FAILED(hr)) return false;

  // Add the 'Value' property to the parent model.
  auto localValueProperty{winrt::make<V8LocalValueProperty>()};
  winrt::com_ptr<IModelObject> spLocalValuePropertyModel;
  hr = CreateProperty(spDataModelManager.get(), localValueProperty.get(),
                      spLocalValuePropertyModel.put());
  hr = spLocalDataModel->SetKey(L"Value", spLocalValuePropertyModel.get(),
                                nullptr);
  // Register the DataModel as the viewer for the type signature
  hr = spDataModelManager->RegisterModelForTypeSignature(
      spLocalTypeSignature.get(), spLocalDataModel.get());
  hr = spDataModelManager->RegisterModelForTypeSignature(
      spMaybeLocalTypeSignature.get(), spLocalDataModel.get());
  hr = spDataModelManager->RegisterModelForTypeSignature(
      spHandleTypeSignature.get(), spLocalDataModel.get());
  hr = spDataModelManager->RegisterModelForTypeSignature(
      spMaybeHandleTypeSignature.get(), spLocalDataModel.get());

  // Register the @$currisolate function alias.
  auto currIsolateFunction{winrt::make<CurrIsolateAlias>()};

  VARIANT vtCurrIsolateFunction;
  vtCurrIsolateFunction.vt = VT_UNKNOWN;
  vtCurrIsolateFunction.punkVal =
      static_cast<IModelMethod*>(currIsolateFunction.get());

  hr = spDataModelManager->CreateIntrinsicObject(
      ObjectMethod, &vtCurrIsolateFunction, spCurrIsolateModel.put());
  hr = spDebugHostExtensibility->CreateFunctionAlias(pcurIsolate,
                                                     spCurrIsolateModel.get());

  return !FAILED(hr);
}

Extension::~Extension() {
  _RPTF0(_CRT_WARN, "Entered Extension::~Extension\n");
  spDebugHostExtensibility->DestroyFunctionAlias(pcurIsolate);

  for (const auto& registered : registered_handler_types) {
    if (registered.second != nullptr) {
      spDataModelManager->UnregisterModelForTypeSignature(
          spObjectDataModel.get(), registered.second.get());
    }
  }
  spDataModelManager->UnregisterModelForTypeSignature(
      spLocalDataModel.get(), spLocalTypeSignature.get());
  spDataModelManager->UnregisterModelForTypeSignature(
      spLocalDataModel.get(), spMaybeLocalTypeSignature.get());
  spDataModelManager->UnregisterModelForTypeSignature(
      spLocalDataModel.get(), spHandleTypeSignature.get());
  spDataModelManager->UnregisterModelForTypeSignature(
      spLocalDataModel.get(), spMaybeHandleTypeSignature.get());
}
