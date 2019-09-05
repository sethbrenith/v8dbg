#pragma once

#include <crtdbg.h>
#include <string>
#include <vector>
#include "../utilities.h"
#include "extension.h"
#include "v8.h"

int GetIsolateKey(winrt::com_ptr<IDebugHostContext>& sp_ctx);
HRESULT GetCurrentIsolate(winrt::com_ptr<IModelObject>& sp_result);

struct CurrIsolateAlias : winrt::implements<CurrIsolateAlias, IModelMethod> {
  HRESULT __stdcall Call(IModelObject* pContextObject, ULONG64 arg_count,
                         _In_reads_(arg_count) IModelObject** pp_arguments,
                         IModelObject** pp_result,
                         IKeyStore** pp_metadata) noexcept override;
};
