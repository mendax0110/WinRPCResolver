#include "../include/ResolveWindows.h"

#if defined(_WIN32) || defined(_WIN64)

int searchProtocol(ResolveWindows& resolver, RPC_WSTR server, RPC_WSTR protocol)
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
        resolver.PrintUuidAndAnnotation(ifId, uuid, szAnnot);

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

RPC_WSTR protocols[] = { L"ncacn_ip_tcp", L"ncacn_np", L"ncacn_nb_tcp", L"ncacn_http", L"ncalrpc" };

int main(int argc, char* argv[])
{
    int verbosity = 0;
    if (argc > 1)
    {
        verbosity = std::stoi(argv[1]);
    }

    ResolveWindows resolver(verbosity);

    std::unordered_map<std::wstring, std::wstring> knownEndpoints;
    resolver.ScanDirectoryForDlls(L"C:\\Windows\\System32", knownEndpoints);

    for (const auto& [dllPath, endpoint] : knownEndpoints)
    {
        std::wcout << L"Endpoint: " << endpoint << L" in " << dllPath << std::endl;
        for (RPC_WSTR protocol : protocols)
        {
            int numFound = searchProtocol(resolver, endpoint.c_str(), protocol);
            if (numFound > 0)
            {
                std::wcout << L"Found " << numFound << L" endpoints using protocol " << protocol << std::endl;
            }
        }
    }

    resolver.Usage(L"rpcscan");

    return 0;
}

#endif