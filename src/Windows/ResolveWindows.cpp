#include "../../include/ResolveWindows.h"

#if defined(_WIN32) || defined(_WIN64)

ResolveWindows::ResolveWindows(int verbosity)
    : _verbosity(verbosity)
{
}

ResolveWindows::~ResolveWindows()
{
}

std::wstring ResolveWindows::GuidToString(const GUID& guid)
{
    wchar_t buffer[64] = { 0 };
    swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), 
            L"{%08lX-%04X-%04X-%04X-%012llX}",
        guid.Data1, guid.Data2, guid.Data3,
        (guid.Data4[0] << 8) | guid.Data4[1],
        static_cast<uint16_t>(guid.Data4[2] << 40) | static_cast<uint16_t>(guid.Data4[3] << 32) | 
        static_cast<uint16_t>(guid.Data4[4] << 24) | static_cast<uint16_t>(guid.Data4[5] << 16) | 
        static_cast<uint16_t>(guid.Data4[6] << 8) | static_cast<uint16_t>(guid.Data4[7]));

    return std::wstring(buffer);
}

std::wstring ResolveWindows::ExtractGuidFromDll(const std::wstring& dllPath)
{
    HMODULE hmodule = LoadLibraryEx(dllPath.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!hmodule)
    {
        std::wcout << L"Failed to load library: " << dllPath << std::endl;
        return L"";
    }

    HRSRC hResInfo = FindResource(hmodule, MAKEINTRESOURCE(1), RT_RCDATA);
    if (!hResInfo)
    {
        std::wcout << L"Failed to find resource in library: " << dllPath << std::endl;
        FreeLibrary(hmodule);
        return L"";
    }

    HGLOBAL hResData = LoadResource(hmodule, hResInfo);
    if (!hResData)
    {
        std::wcout << L"Failed to load resource in library: " << dllPath << std::endl;
        FreeLibrary(hmodule);
        return L"";
    }

    void* pResData = LockResource(hResData);
    if (!pResData)
    {
        std::wcout << L"Failed to lock resource in library: " << dllPath << std::endl;
        FreeLibrary(hmodule);
        return L"";
    }

    GUID* pGuid = reinterpret_cast<GUID*>(pResData);
    std::wstring guidString = GuidToString(*pGuid);
    FreeLibrary(hmodule);

    return guidString;
}

void ResolveWindows::ScanDirectoryForDlls(const std::filesystem::path& directory, std::unordered_map<std::wstring, std::wstring>& knownEndpoints)
{
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.is_directory())
        {
            ScanDirectoryForDlls(entry.path(), knownEndpoints);
        }
        else if (entry.is_regular_file())
        {
            if (entry.path().extension() == L".dll")
            {
                std::wstring guid = ExtractGuidFromDll(entry.path().wstring());
                if (!guid.empty())
                {
                    knownEndpoints[guid] = entry.path().wstring();
                }
            }
        }
    }
}

bool ResolveWindows::ComposeStringBinding(RPC_WSTR protocol, RPC_WSTR server, RPC_WSTR* szStringBinding)
{
    return RpcStringBindingCompose(NULL, protocol, server, NULL, NULL, szStringBinding) == RPC_S_OK;
}

bool ResolveWindows::ConvertToBindingHandle(RPC_WSTR szStringBinding, RPC_BINDING_HANDLE* hRpc)
{
    return RpcBindingFromStringBinding(szStringBinding, hRpc) == RPC_S_OK;
}

bool ResolveWindows::BeginEndpointInquiry(RPC_BINDING_HANDLE hRpc, RPC_EP_INQ_HANDLE* hInq)
{
    return RpcMgmtEpEltInqBegin(hRpc, RPC_C_EP_ALL_ELTS, NULL, 0, NULL, hInq) == RPC_S_OK;
}

bool ResolveWindows::EndpointInquiryNext(RPC_EP_INQ_HANDLE hInq, RPC_IF_ID* ifId, RPC_BINDING_HANDLE* hEnumBind, UUID* uuid, RPC_WSTR* szAnnot)
{
    return RpcMgmtEpEltInqNext(hInq, ifId, hEnumBind, uuid, szAnnot) == RPC_S_OK;
}

bool ResolveWindows::ParseBindingHandle(RPC_BINDING_HANDLE hEnumBind, RPC_WSTR server, RPC_BINDING_HANDLE* hIfidsBind)
{
    RPC_WSTR strBinding = NULL, strObj = NULL, strProtseq = NULL, strNetaddr = NULL, strEndpoint = NULL, strNetoptions = NULL;
    if (RpcBindingToStringBinding(hEnumBind, &strBinding) != RPC_S_OK)
    {
        return false;
    }
    if (wcsstr((LPCWSTR)strBinding, L"ncalrpc") != NULL)
    {
        RpcStringFree(&strBinding);
        return false;
    }
    if (RpcStringBindingParse(strBinding, &strObj, &strProtseq, &strNetaddr, &strEndpoint, &strNetoptions) != RPC_S_OK)
    {
        RpcStringFree(&strBinding);
        return false;
    }
    RpcStringFree(&strBinding);
    bool success = RpcStringBindingCompose(strObj, strProtseq, wcscmp(L"ncacn_nb_tcp", (LPCWSTR)strProtseq) == 0 ? strNetaddr : server, strEndpoint, strNetoptions, &strBinding) == RPC_S_OK &&
                    RpcBindingFromStringBinding(strBinding, hIfidsBind) == RPC_S_OK;

    RpcStringFree(&strObj);
    RpcStringFree(&strProtseq);
    RpcStringFree(&strNetaddr);
    RpcStringFree(&strEndpoint);
    RpcStringFree(&strNetoptions);
    RpcStringFree(&strBinding);

    return success;
}

bool ResolveWindows::InquireInterfaceIDs(RPC_BINDING_HANDLE hIfidsBind, RPC_IF_ID_VECTOR** pVector)
{
    return RpcMgmtInqIfIds(hIfidsBind, pVector) == RPC_S_OK;
}

bool ResolveWindows::InquireServerPrincipalName(RPC_BINDING_HANDLE hEnumBind, RPC_WSTR* princName)
{
    return RpcMgmtInqServerPrincName(hEnumBind, RPC_C_AUTHN_WINNT, princName) == RPC_S_OK;
}

bool ResolveWindows::InquireStats(RPC_BINDING_HANDLE hEnumBind, RPC_STATS_VECTOR** pStats)
{
    return RpcMgmtInqStats(hEnumBind, pStats) == RPC_S_OK;
}

void ResolveWindows::FreeResources(RPC_WSTR* str)
{
    if (*str)
    {
        RpcStringFree(str);
    }
}

void ResolveWindows::FreeBindingHandle(RPC_BINDING_HANDLE* hRpc)
{
    if (*hRpc)
    {
        RpcBindingFree(hRpc);
    }
}

void ResolveWindows::PrintUuidAndAnnotation(RPC_IF_ID Ifid, UUID uuid, RPC_WSTR szAnnot)
{
    RPC_WSTR str = NULL;
    if (UuidToString(&(Ifid.Uuid), &str) == RPC_S_OK)
    {
        wprintf(L"Ifid: %s version %d.%d\n", str, Ifid.VersMajor, Ifid.VersMinor);
        RpcStringFree(&str);
    }
    if (szAnnot)
    {
        wprintf(L"Annotation: %s\n", szAnnot);
        RpcStringFree(&szAnnot);
    }
    if (UuidToString(&uuid, &str) == RPC_S_OK)
    {
        wprintf(L"Object UUID: %s\n", str);
        RpcStringFree(&str);
    }
}

void ResolveWindows::PrintInterfaces(RPC_IF_ID_VECTOR* pVector)
{
    wprintf(L"Interfaces: %d\n", pVector->Count);
    for (unsigned int i = 0; i < pVector->Count; i++)
    {
        RPC_IF_ID Ifid = pVector->IfId[i];
        wprintf(L"Interface %d: ", i);
        RPC_WSTR str = NULL;
        if (UuidToString(&(Ifid.Uuid), &str) == RPC_S_OK)
        {
            wprintf(L"%s version %d.%d\n", str, Ifid.VersMajor, Ifid.VersMinor);
            if (str)
            {
                RpcStringFree(&str);
            }
        }
    }
}

void ResolveWindows::PrintPrincipalName(RPC_WSTR princName)
{
    wprintf(L"Principal name: %s\n", princName);
    RpcStringFree(&princName);
}

void ResolveWindows::PrintStats(RPC_STATS_VECTOR* pStats)
{
    wprintf(L"Stats: %d\n", pStats->Count);
    for (unsigned int i = 0; i < pStats->Count; i++)
    {
        RPC_STATS_TYPE type = pStats->Stats[i].StatsType;
        wprintf(L"Stats %d: ", i);
        switch (type)
        {
        case RpcStatsCall:
            wprintf(L"Call\n");
            break;
        case RpcStatsCallFail:
            wprintf(L"Call Fail\n");
            break;
        case RpcStatsCallAsync:
            wprintf(L"Call Async\n");
            break;
        case RpcStatsCallFailAsync:
            wprintf(L"Call Fail Async\n");
            break;
        case RpcStatsSend:
            wprintf(L"Send\n");
            break;
        case RpcStatsReceive:
            wprintf(L"Receive\n");
            break;
        case RpcStatsPacket:
            wprintf(L"Packet\n");
            break;
        case RpcStatsPacketRetry:
            wprintf(L"Packet Retry\n");
            break;
        case RpcStatsCallLocal:
            wprintf(L"Call Local\n");
            break;
        case RpcStatsCallFailLocal:
            wprintf(L"Call Fail Local\n");
            break;
        case RpcStatsSendLocal:
            wprintf(L"Send Local\n");
            break;
        case RpcStatsReceiveLocal:
            wprintf(L"Receive Local\n");
            break;
        case RpcStatsPacketLocal:
            wprintf(L"Packet Local\n");
            break;
        case RpcStatsPacketRetryLocal:
            wprintf(L"Packet Retry Local\n");
            break;
        default:
            wprintf(L"Unknown\n");
            break;
        }
    }
}

void ResolveWindows::Usage(wchar_t* programName)
{
    wprintf(L"Usage: %s [-v] [-d directory]\n", programName);
    wprintf(L"  -v: verbose output\n");
    wprintf(L"  -d directory: directory to scan for DLLs\n");
}

#endif