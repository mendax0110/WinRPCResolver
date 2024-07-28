#include "../../include/ResolveWindows.h"
#include <rpcdce.h>
#include <fstream>

#pragma comment(lib, "Rpcrt4.lib")

#if defined(_WIN32) || defined(_WIN64)

ResolveWindows::ResolveWindows(int verbosity)
    : _verbosity(verbosity)
{
}

ResolveWindows::~ResolveWindows()
{
}

/**
 * @brief Loads a library without resolving dependencies
 * 
 * @param dllPath -> path to the library
 * @return HMODULE (handle to the library)
 */
HMODULE ResolveWindows::LoadLibraryExWO(LPCWSTR dllPath)
{
    HMODULE hmodule = ::LoadLibraryExW(dllPath, NULL, RPCFLG_ACCESSIBILITY_BIT1);
    if (!hmodule)
    {
        std::wcout << L"Failed to load library: " << dllPath << L", Error: " << GetLastError() << std::endl;
    }
    return hmodule;
}

/**
 * @brief Converts the GUID to a string
 * 
 * @param guid -> GUID to convert
 * @return std::wstring (GUID as a string)
 */
std::wstring ResolveWindows::GuidToString(const GUID& guid)
{
    wchar_t buffer[64] = { 0 };


    swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]),
        L"{%08lX-%04X-%04X-%04X-%012llX}",
        guid.Data1, guid.Data2, guid.Data3,
        (guid.Data4[0] << 8) | guid.Data4[1],
        static_cast<uint64_t>(guid.Data4[2]) << 40 |
        static_cast<uint64_t>(guid.Data4[3]) << 32 |
        static_cast<uint64_t>(guid.Data4[4]) << 24 |
        static_cast<uint64_t>(guid.Data4[5]) << 16 |
        static_cast<uint64_t>(guid.Data4[6]) << 8 |
        static_cast<uint64_t>(guid.Data4[7]));

    return std::wstring(buffer);
}

/**
 * @brief Extracts the GUID from a DLL
 * 
 * @param dllPath -> path to the DLL
 * @return std::wstring (GUID as a string)
 */
std::wstring ResolveWindows::ExtractGuidFromDll(const std::wstring& dllPath)
{
    HMODULE hmodule = LoadLibraryExWO(dllPath.c_str());
    if (!hmodule)
    {
        std::wcout << L"Failed to load library: " << dllPath << std::endl;
        return L"";
    }

    HRSRC hResInfo = FindResource(hmodule, MAKEINTRESOURCE(1), RT_RCDATA);
    if (!hResInfo)
    {
        std::wcout << L"Failed to find resource in library: " << dllPath << L", Error: " << GetLastError() << std::endl;
        FreeLibrary(hmodule);
        return L"";
    }

    HGLOBAL hResData = LoadResource(hmodule, hResInfo);
    if (!hResData)
    {
        std::wcout << L"Failed to load resource in library: " << dllPath << L", Error: " << GetLastError() << std::endl;
        FreeLibrary(hmodule);
        return L"";
    }

    void* pResData = LockResource(hResData);
    if (!pResData)
    {
        std::wcout << L"Failed to lock resource in library: " << dllPath << L", Error: " << GetLastError() << std::endl;
        FreeLibrary(hmodule);
        return L"";
    }

    GUID* pGuid = reinterpret_cast<GUID*>(pResData);
    std::wstring guidString = GuidToString(*pGuid);
    FreeLibrary(hmodule);

    return guidString;
}

/**
 * @brief Scans the given directory for DLLs
 * 
 * @param directory -> directory to scan
 * @param knownEndpoints -> map of known endpoints
 */
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
        else if (entry.is_symlink())
        {
			// ignore symlinks
		}
        else if (entry.is_other())
        {
			std::wcout << L"Unknown file type: " << entry.path() << std::endl;
		}
        else if (entry.is_block_file())
        {
            std::wcout << L"Block file: " << entry.path() << std::endl;
        }
    }
}

/**
 * @brief Composes a string binding
 * 
 * @param protocol -> protocol that the binding uses
 * @param server -> server that the binding uses
 * @param szStringBinding -> string bindings
 * @return true (if successful)
 * @return false (if failed)
 */
bool ResolveWindows::ComposeStringBinding(RPC_WSTR protocol, RPC_WSTR server, RPC_WSTR* szStringBinding)
{
    return RpcStringBindingComposeW(NULL, protocol, server, NULL, NULL, szStringBinding) == RPC_S_OK;
}

/**
 * @brief Converts a string binding to a binding handle
 * 
 * @param szStringBinding -> string binding
 * @param hRpc -> binding handle
 * @return true (if successful)
 * @return false (if failed)
 */
bool ResolveWindows::ConvertToBindingHandle(RPC_WSTR szStringBinding, void** hRpc)
{
    return RpcBindingFromStringBindingW(szStringBinding, hRpc) == RPC_S_OK;
}

/**
 * @brief The RPC runtime begins an inquiry to enumerate the endpoints that are registered with the endpoint map manager on the server specified in the binding handle.
 * 
 * @param hRpc -> binding handle
 * @param hInq -> inquiry handle
 * @return true (if successful)
 * @return false (if failed)
 */
bool ResolveWindows::BeginEndpointInquiry(RPC_BINDING_HANDLE hRpc, RPC_EP_INQ_HANDLE* hInq)
{
    return RpcMgmtEpEltInqBegin(hRpc, RPC_C_EP_ALL_ELTS, NULL, 0, NULL, hInq) == RPC_S_OK;
}

/**
 * @brief The RPC runtime returns the next element in the enumeration of endpoints.
 * 
 * @param hInq -> inquiry handle
 * @param ifId -> interface ID
 * @param hEnumBind -> enumeration binding handle
 * @param uuid -> UUID
 * @param szAnnot -> annotation
 * @return true (if successful)
 * @return false (if failed)
 */
bool ResolveWindows::EndpointInquiryNext(RPC_EP_INQ_HANDLE hInq, RPC_IF_ID* ifId, void** hEnumBind, UUID* uuid, RPC_WSTR* szAnnot)
{
    return RpcMgmtEpEltInqNextW(hInq, ifId, hEnumBind, uuid, szAnnot) == RPC_S_OK;
}

/**
 * @brief Parses the binding handle
 * 
 * @param hEnumBind -> enumeration binding handle
 * @param server -> server
 * @param hIfidsBind -> interface IDs binding handle
 * @return true (if successful)
 * @return false (if failed)
 */
bool ResolveWindows::ParseBindingHandle(RPC_BINDING_HANDLE hEnumBind, RPC_WSTR server, void** hIfidsBind)
{
    if (!hEnumBind || !hIfidsBind || !server)
    {
		return false;
	}

    if (wcscmp(L"ncacn_nb_tcp", (LPCWSTR)server) == 0)
    {
		return RpcBindingCopy(hEnumBind, hIfidsBind) == RPC_S_OK;
	}

    if (wcscmp(L"ncacn_ip_tcp", (LPCWSTR)server) == 0)
    {
        return RpcBindingCopy(hEnumBind, hIfidsBind) == RPC_S_OK;
    }



    RPC_WSTR strBinding = NULL;
    RPC_WSTR strObj = NULL;
    RPC_WSTR strProtseq = NULL;
    RPC_WSTR strNetaddr = NULL;
    RPC_WSTR strEndpoint = NULL;
    RPC_WSTR strNetoptions = NULL;

    if (RpcBindingToStringBindingW(hEnumBind, &strBinding) != RPC_S_OK)
    {
        return false;
    }

    if (RpcStringBindingParseW(strBinding, &strObj, &strProtseq, &strNetaddr, &strEndpoint, &strNetoptions) != RPC_S_OK)
    {
        RpcStringFreeW(&strBinding);
        return false;
    }

    RpcStringFreeW(&strBinding);

    bool success = RpcStringBindingComposeW(strObj, strProtseq, wcscmp(L"ncacn_nb_tcp", (LPCWSTR)strProtseq) == 0 ? strNetaddr : server, strEndpoint, strNetoptions, &strBinding) == RPC_S_OK &&
                   RpcBindingFromStringBindingW(strBinding, hIfidsBind) == RPC_S_OK;

    RpcStringFreeW(&strObj);
    RpcStringFreeW(&strProtseq);
    RpcStringFreeW(&strNetaddr);
    RpcStringFreeW(&strEndpoint);
    RpcStringFreeW(&strNetoptions);
    RpcStringFreeW(&strBinding);

    return success;
}

/**
 * @brief Inquires the interface IDs
 * 
 * @param hIfidsBind -> interface IDs binding handle
 * @param pVector -> pointer to the interface ID vector
 * @return true (if successful)
 * @return false (if failed)
 */
bool ResolveWindows::InquireInterfaceIDs(RPC_BINDING_HANDLE hIfidsBind, RPC_IF_ID_VECTOR** pVector)
{
    return RpcMgmtInqIfIds(hIfidsBind, pVector) == RPC_S_OK;
}

/**
 * @brief Inquires the server principal name
 * 
 * @param hEnumBind -> enumeration binding handle
 * @param princName -> pointer to the principal name
 * @return true (if successful)
 * @return false (if failed)
 */
bool ResolveWindows::InquireServerPrincipalName(RPC_BINDING_HANDLE hEnumBind, RPC_WSTR* princName)
{
    return RpcMgmtInqServerPrincNameW(hEnumBind, RPC_C_AUTHN_WINNT, princName) == RPC_S_OK;
}

/**
 * @brief Inquires the statistics
 * 
 * @param hEnumBind -> enumeration binding handle
 * @param pStats -> pointer to the statistics vector
 * @return true (if successful)
 * @return false (if failed)
 */
bool ResolveWindows::InquireStats(RPC_BINDING_HANDLE hEnumBind, RPC_STATS_VECTOR** pStats)
{
    return RpcMgmtInqStats(hEnumBind, pStats) == RPC_S_OK;
}

/**
 * @brief The method to free resources (a resource is a pointer to a string)
 * 
 * @param str -> a pointer to the string of resources
 */
void ResolveWindows::FreeResources(RPC_WSTR* str)
{
    if (*str)
    {
        RpcStringFreeW(str);
    }
}

/**
 * @brief The method to free the binding handle (a binding to a handle is a pointer to a binding handle)
 * 
 * @param hRpc -> a pointer to the binding handle
 */
void ResolveWindows::FreeBindingHandle(RPC_BINDING_HANDLE* hRpc)
{
    if (*hRpc)
    {
        RpcBindingFree(hRpc);
    }
}

/**
 * @brief Prints the UUID & the annotation of the interface
 * 
 * @param Ifid -> pointer to the interface ID
 * @param uuid -> UUID
 * @param szAnnot -> annotation
 */
void ResolveWindows::PrintUuidAndAnnotation(RPC_IF_ID* Ifid, UUID uuid, RPC_WSTR szAnnot)
{
    RPC_WSTR str = NULL;
    if (UuidToStringW(&Ifid->Uuid, &str) == RPC_S_OK)
    {
        wprintf(L"Ifid: %s version %d.%d\n", str, Ifid->VersMajor, Ifid->VersMinor);
        RpcStringFreeW(&str);
    }
    if (szAnnot)
    {
        wprintf(L"Annotation: %s\n", szAnnot);
        RpcStringFreeW(&szAnnot);
    }
    if (UuidToStringW(&uuid, &str) == RPC_S_OK)
    {
        wprintf(L"Object UUID: %s\n", str);
        RpcStringFreeW(&str);
    }
}

/**
 * @brief Prints the interfaces of a given pointer to the interface ID vector
 * 
 * @param pVector -> pointer to the interface ID vector
 */
void ResolveWindows::PrintInterfaces(RPC_IF_ID_VECTOR* pVector)
{
    wprintf(L"Interfaces: %d\n", pVector->Count);
    for (unsigned int i = 0; i < pVector->Count; i++)
    {
        RPC_IF_ID* pIfid = pVector->IfId[i];
        wprintf(L"Interface %d: ", i);
        RPC_WSTR str = NULL;
        if (UuidToStringW(&(pIfid->Uuid), &str) == RPC_S_OK)
        {
            wprintf(L"%s version %d.%d\n", str, pIfid->VersMajor, pIfid->VersMinor);
            RpcStringFreeW(&str);
        }
    }
}

/**
 * @brief Prints the statistics of a given pointer to the statistics vector
 * 
 * @param pStats -> pointer to the statistics vector
 */
void ResolveWindows::PrintStats(RPC_STATS_VECTOR* pStats)
{
    wprintf(L"Stats: %d\n", pStats->Count);
    for (unsigned int i = 0; i < pStats->Count; i++)
    {
        wprintf(L"Stats[%d]: %d\n", i, pStats->Stats[i]);
    }
}

/**
 * @brief Prints the principal name
 * 
 * @param princName -> principal name of the server
 */
void ResolveWindows::PrintPrincipalName(RPC_WSTR princName)
{
    wprintf(L"Principal name: %s\n", princName);
}

/**
 * @brief The method to print the usage of the program
 * 
 * @param programName -> pointer to the program name
 */
void ResolveWindows::Usage(wchar_t* programName)
{
    wprintf(L"Usage: %s [-v] [-d directory]\n", programName);
}


void ResolveWindows::SaveEndpointsToFile(const std::unordered_map<std::wstring, std::wstring>& knownEndpoints, const std::filesystem::path& filePath)
{
    std::wofstream file(filePath);

    if (!file.is_open())
    {
        std::wcerr << L"Failed to open file: " << filePath << std::endl;
        return;
    }

    for (const auto& [dllPath, endpoint] : knownEndpoints)
    {
        file << L"Endpoint: " << endpoint << L" in " << dllPath << std::endl;
    }

    file.close();

    if (file.bad())
    {
        std::wcerr << L"Failed to write to file: " << filePath << std::endl;
    }
    else
    {
        std::wcout << L"Successfully saved endpoints to file: " << filePath << std::endl;
    }
}

#endif // defined(_WIN32) || defined(_WIN64)
