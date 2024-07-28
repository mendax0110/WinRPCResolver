#include "../include/ResolveWindows.h"

#if defined(_WIN32) || defined(_WIN64)

/**
 * @brief Entry point for the Windows RPC scan, searches for known endpoints in DLLs and queries them for RPC interfaces and stats.
 *
 * @param resolver -> Reference to the ResolveWindows object
 * @param server -> RPC server to query
 * @param protocol -> RPC protocol to use
 * @return int (if any endpoints were found)
 */
static int searchProtocol(ResolveWindows& resolver, RPC_WSTR server, RPC_WSTR protocol)
{
    RPC_WSTR szStringBinding = nullptr;
    RPC_BINDING_HANDLE hRpc;
    RPC_EP_INQ_HANDLE hInq;
    int numFound = 0;

    if (!resolver.ComposeStringBinding(protocol, server, &szStringBinding))
    {
        std::wcerr << L"Failed to compose string binding" << std::endl;
        return numFound;
    }

    if (!resolver.ConvertToBindingHandle(szStringBinding, &hRpc))
    {
        std::wcerr << L"Failed to convert to binding handle" << std::endl;
        resolver.FreeResources(&szStringBinding);
        return numFound;
    }

    if (!resolver.BeginEndpointInquiry(hRpc, &hInq))
    {
        std::wcerr << L"Failed to begin endpoint inquiry" << std::endl;
        resolver.FreeBindingHandle(&hRpc);
        resolver.FreeResources(&szStringBinding);
        return numFound;
    }

    RPC_IF_ID ifId;
    RPC_BINDING_HANDLE hEnumBind;
    UUID uuid;
    RPC_WSTR szAnnot;

    while (resolver.EndpointInquiryNext(hInq, &ifId, &hEnumBind, &uuid, &szAnnot))
    {
        numFound++;
        resolver.PrintUuidAndAnnotation(&ifId, uuid, szAnnot);

        if (resolver._verbosity >= 1)
        {
            RPC_BINDING_HANDLE hIfidsBind;
            if (resolver.ParseBindingHandle(hEnumBind, server, &hIfidsBind))
            {
                RPC_IF_ID_VECTOR* pVector;
                if (resolver.InquireInterfaceIDs(hIfidsBind, &pVector))
                {
                    resolver.PrintInterfaces(pVector);
                    resolver.FreeResources((RPC_WSTR*)&pVector);
                }
                resolver.FreeBindingHandle(&hIfidsBind);
            }
            else
            {
                std::wcerr << L"Failed to parse binding handle" << std::endl;
            }
        }

        if (resolver._verbosity >= 2)
        {
            RPC_WSTR princName;
            if (resolver.InquireServerPrincipalName(hEnumBind, &princName))
            {
                resolver.PrintPrincipalName(princName);
                resolver.FreeResources(&princName);
            }
            else
            {
                std::wcerr << L"Failed to inquire server principal name" << std::endl;
            }
        }

        RPC_STATS_VECTOR* pStats;
        if (resolver.InquireStats(hEnumBind, &pStats))
        {
            resolver.PrintStats(pStats);
            resolver.FreeResources((RPC_WSTR*)&pStats);
        }
        else
        {
            std::wcerr << L"Failed to inquire stats" << std::endl;
        }

        resolver.FreeBindingHandle(&hEnumBind);
    }

    resolver.FreeBindingHandle(&hRpc);
    resolver.FreeResources(&szStringBinding);

    return numFound;
}

/**
 * @brief The known protocols to search for
 *
 */
RPC_WSTR protocols[] = {
    (RPC_WSTR)L"ncacn_ip_tcp",
    (RPC_WSTR)L"ncacn_np",
    (RPC_WSTR)L"ncacn_nb_tcp",
    (RPC_WSTR)L"ncacn_http",
    (RPC_WSTR)L"ncalrpc"
};

/**
 * @brief The main entry point of the Windows RPC scan
 *
 * @param argc -> Number of arguments
 * @param argv -> Arguments
 * @return int (0 if successful, 1 if failed to find any endpoints)
 */
int main(int argc, char* argv[])
{
    int verbosity = 0;
    if (argc > 1)
    {
        verbosity = std::stoi(argv[1]);
    }

    ResolveWindows resolver(verbosity);

    std::unordered_map<std::wstring, std::wstring> knownEndpoints;

    // List of directories to scan
    std::vector<std::wstring> directories = {
        L"C:\\Windows\\System32",
        L"C:\\Windows\\SysWOW64",
        L"C:\\Program Files",
        L"C:\\Program Files (x86)",
        L"C:\\Windows\\System32\\drivers",
        L"C:\\Windows\\System32\\wbem",
        L"C:\\Windows\\SysWOW64\\drivers",
        L"C:\\Windows\\SysWOW64\\wbem",
        L"C:\\Program Files\\Common Files",
        L"C:\\Program Files (x86)\\Common Files"
    };

    size_t maxDllNum = 0;

    for (const auto& directory : directories)
    {
        std::wcout << L"Scanning directory: " << directory << std::endl;
        try
        {
            resolver.ScanDirectoryForDlls(directory, knownEndpoints);
            maxDllNum += knownEndpoints.size();
            if (maxDllNum > 100)
            {
                resolver.SaveEndpointsToFile(knownEndpoints, L"endpoints.txt");
                break;
            }
        }
        catch (const std::exception& e)
        {
            std::wcerr << L"Failed to scan directory " << directory << L": " << e.what() << std::endl;
        }
    }

    for (const auto& [dllPath, endpoint] : knownEndpoints)
    {
        std::wcout << L"Endpoint: " << endpoint << L" in " << dllPath << std::endl;
        for (RPC_WSTR protocol : protocols)
        {
            int numFound = searchProtocol(resolver, (RPC_WSTR)(endpoint.c_str()), protocol);
            if (numFound > 0)
            {
                std::wcout << L"Found " << numFound << L" endpoints using protocol " << protocol << std::endl;
                RPC_IF_ID_VECTOR* pVector;

                if (resolver.InquireInterfaceIDs(nullptr, &pVector))
                {
                    resolver.PrintInterfaces(pVector);
                    resolver.FreeResources((RPC_WSTR*)&pVector);
                }
                RPC_WSTR princName;
                if (resolver.InquireServerPrincipalName(nullptr, &princName))
                {
                    resolver.PrintPrincipalName(princName);
                    resolver.FreeResources(&princName);
                }
                else
                {
                    std::wcerr << L"Failed to inquire server principal name" << std::endl;
                }
                RPC_STATS_VECTOR* pStats;
                if (resolver.InquireStats(nullptr, &pStats))
                {
                    resolver.PrintStats(pStats);
                    resolver.FreeResources((RPC_WSTR*)&pStats);
                }
            }
        }
    }

    resolver.Usage(L"rpcscan");

    return 0;
}

#endif
