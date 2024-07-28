#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <unordered_map>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <comdef.h>
#include <rpc.h>
#include <rpcdce.h>
#include <rpcndr.h>
#include <winnt.h>

/// @brief ResolveWindows class \namespace ResolveWindows
class ResolveWindows
{
public:
    ResolveWindows(int verbosity);
    ~ResolveWindows();

    HMODULE LoadLibraryExWO(LPCWSTR dllPath);

    std::wstring GuidToString(const GUID& guid);
    std::wstring ExtractGuidFromDll(const std::wstring& dllPath);
    void ScanDirectoryForDlls(const std::filesystem::path& directory, std::unordered_map<std::wstring, std::wstring>& knownEndpoints);

    bool ComposeStringBinding(RPC_WSTR protocol, RPC_WSTR server, RPC_WSTR* szStringBinding);
    bool ConvertToBindingHandle(RPC_WSTR szStringBinding, void** hRpc);
    bool BeginEndpointInquiry(RPC_BINDING_HANDLE hRpc, RPC_EP_INQ_HANDLE* hInq);
    bool EndpointInquiryNext(RPC_EP_INQ_HANDLE hInq, RPC_IF_ID* ifId, void** hEnumBind, UUID* uuid, RPC_WSTR* szAnnot);
    bool ParseBindingHandle(RPC_BINDING_HANDLE hEnumBind, RPC_WSTR server, void** hIfidsBind);
    bool InquireInterfaceIDs(RPC_BINDING_HANDLE hIfidsBind, RPC_IF_ID_VECTOR** pVector);
    bool InquireServerPrincipalName(RPC_BINDING_HANDLE hEnumBind, RPC_WSTR* princName);
    bool InquireStats(RPC_BINDING_HANDLE hEnumBind, RPC_STATS_VECTOR** pStats);

    void FreeResources(RPC_WSTR* str);
    void FreeBindingHandle(RPC_BINDING_HANDLE* hRpc);

    void PrintUuidAndAnnotation(RPC_IF_ID* Ifid, UUID uuid, RPC_WSTR szAnnot);
    void PrintInterfaces(RPC_IF_ID_VECTOR* pVector);
    void PrintPrincipalName(RPC_WSTR princName);
    void PrintStats(RPC_STATS_VECTOR* pStats);
    void Usage(wchar_t* programName);
    void SaveEndpointsToFile(const std::unordered_map<std::wstring, std::wstring>& knownEndpoints, const std::filesystem::path& filePath);

    int _verbosity;

private:
    
};

#endif