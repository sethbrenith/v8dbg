#pragma once

#if !defined(UNICODE) || !defined(_UNICODE)
#error Unicode not defined
#endif

#include <crtdbg.h>
#include <Windows.h>
#include <DbgEng.h>
#include <DbgModel.h>

#include <winrt/base.h>
#include <string>

// Globals for use throughout the extension. (Populated on load).
extern winrt::com_ptr<IDataModelManager> sp_data_model_manager;
extern winrt::com_ptr<IDebugHost> sp_debug_host;
extern winrt::com_ptr<IDebugControl5> sp_debug_control;

// To be implemented by the custom extension code. (Called on load).
bool CreateExtension();
void DestroyExtension();
