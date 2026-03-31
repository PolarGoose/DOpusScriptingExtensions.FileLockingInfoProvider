#pragma once

#include "Shared/Utils/Exceptions.h"
#include "FileLockingInfoProvider.ComDll/FileLockingInfoProvider_i.h"
#include "FileLockingInfoProvider.ComDll/Utils/ComUtils.h"

class ATL_NO_VTABLE CProcessInfo :
  public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>,
  public ATL::CComCoClass<CProcessInfo, &__uuidof(ProcessInfo)>,
  public ATL::IDispatchImpl<IProcessInfo, &IID_IProcessInfo, &LIBID_DOpusScriptingExtensions_FileLockingInfoProvider_Lib, 1, 0> {
public:
  BEGIN_COM_MAP(CProcessInfo)
    COM_INTERFACE_ENTRY(IProcessInfo)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  STDMETHOD(get_Pid)(UINT* val) override try {
    *val = _pid;
    return S_OK;
  } CATCH_ALL_EXCEPTIONS()

  STDMETHOD(get_ExecutablePath)(BSTR* val) override try {
    *val = Copy(_executablePath);
    return S_OK;
  } CATCH_ALL_EXCEPTIONS()

  STDMETHOD(get_DomainName)(BSTR* val) override try {
    *val = Copy(_domainName);
    return S_OK;
  } CATCH_ALL_EXCEPTIONS()

  STDMETHOD(get_UserName)(BSTR* val) override try {
    *val = Copy(_userName);
    return S_OK;
  } CATCH_ALL_EXCEPTIONS()

  void Init(const UINT pid, std::wstring executablePath, std::wstring domainName, std::wstring userName) {
    _pid = pid;
    _executablePath = std::move(executablePath);
    _domainName = std::move(domainName);
    _userName = std::move(userName);
  }

private:
  UINT _pid = 0;
  std::wstring _executablePath;
  std::wstring _domainName;
  std::wstring _userName;
};
