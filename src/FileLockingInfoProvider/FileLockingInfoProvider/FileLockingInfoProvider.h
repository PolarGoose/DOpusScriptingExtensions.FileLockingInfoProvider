#pragma once

#include "FileLockingInfoProvider/FileLockingInfoProvider/ProcessInfo.h"
#include "FileLockingInfoProvider/FileLockingInfoProvider/LockedFilesDatabase.h"

class ATL_NO_VTABLE CFileLockingInfoProvider :
  public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>,
  public ATL::CComCoClass<CFileLockingInfoProvider, &CLSID_FileLockingInfoProvider>,
  public ATL::IDispatchImpl<IFileLockingInfoProvider, &IID_IFileLockingInfoProvider, &LIBID_DOpusScriptingExtensions_FileLockingInfoProvider_Lib, /*wMajor =*/ 1, /*wMinor =*/ 0>
{
public:
  DECLARE_REGISTRY_RESOURCEID(IDR_FileLockingInfoProvider)
  BEGIN_COM_MAP(CFileLockingInfoProvider)
    COM_INTERFACE_ENTRY(IFileLockingInfoProvider)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT FinalConstruct() try {
    _lockedFilesDatabase = LockedFilesDatabase::GetInstance();
    return S_OK;
  } CATCH_ALL_EXCEPTIONS()

  STDMETHOD(GetLockingProcesses)(BSTR fileFullName, SAFEARRAY** processInfos) override try {
    auto lockingProcesses = _lockedFilesDatabase->GetLockingProcesses(fileFullName);

    ATL::CComSafeArray<VARIANT> processInfosSafeArray;
    THROW_IF_FAILED_MSG(
      processInfosSafeArray.Create(static_cast<ULONG>(lockingProcesses.size())),
      L"Failed to allocate a SafeArray");

    for (LONG i = 0; i < static_cast<LONG>(lockingProcesses.size()); ++i) {
      ATL::CComVariant var;
      var.vt = VT_DISPATCH;
      var.pdispVal = lockingProcesses[i].Detach();
      processInfosSafeArray.SetAt(i, var);
    }

    *processInfos = processInfosSafeArray.Detach();
    return S_OK;
  } CATCH_ALL_EXCEPTIONS()

private:
  LockedFilesDatabase* _lockedFilesDatabase;
};

OBJECT_ENTRY_AUTO(__uuidof(FileLockingInfoProvider), CFileLockingInfoProvider)
