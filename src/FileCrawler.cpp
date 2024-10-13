#include "../include/FileCrawler.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <rpc.h>
#include <rpcdcep.h>
#include <tchar.h>
#include <winsvc.h>

#pragma comment(lib, "rpcrt4.lib")

FileCrawler::FileCrawler(const std::string& rootDir) : m_rootDir(rootDir) {}

std::vector<std::string> FileCrawler::findFiles(const std::vector<std::string>& extensions)
{
    std::vector<std::string> foundFiles;
    searchDirectory(m_rootDir, extensions, foundFiles);
    return foundFiles;
}

void FileCrawler::searchDirectory(const std::string& dir, const std::vector<std::string>& extensions, std::vector<std::string>& foundFiles)
{
    WIN32_FIND_DATAA findFileData;
    HANDLE hFind;
    std::string searchPath = dir + "\\*";
    hFind = FindFirstFileA(searchPath.c_str(), &findFileData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        std::cerr << "Error accessing directory: " << dir << ". Error Code: " << error << " (" << getErrorMessage(error) << ")" << std::endl;
        return;
    }

    do
    {
        const std::string fileName = findFileData.cFileName;
        const std::string fullPath = dir + "\\" + fileName;

        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (fileName == "." || fileName == "..")
            {
                continue;
            }
            searchDirectory(fullPath, extensions, foundFiles);
        }
        else
        {
            for (const auto& ext : extensions)
            {
                if (endsWith(fileName, ext) && isRpcRelatedFile(fullPath))
                {
                    foundFiles.push_back(fullPath);
                    break;
                }
            }
        }
    } while (FindNextFileA(hFind, &findFileData) != 0);

    FindClose(hFind);
}

std::string FileCrawler::getErrorMessage(DWORD errorCode)
{
    LPVOID lpMsgBuf;
    DWORD bufLen = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);
    if (bufLen)
    {
        LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
        std::string result(lpMsgStr, lpMsgStr + bufLen);
        LocalFree(lpMsgBuf);
        return result;
    }
    return std::string();
}

bool FileCrawler::endsWith(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

bool FileCrawler::isRpcRelatedFile(const std::string& filePath)
{
    RPC_STATUS status;
    RPC_IF_ID_VECTOR* if_id_vector = nullptr;

    status = RpcMgmtInqIfIds(NULL, &if_id_vector);
    if (status == RPC_S_OK && if_id_vector != nullptr)
    {
        for (unsigned long i = 0; i < if_id_vector->Count; i++)
        {
            RPC_CSTR uuidString;
            status = UuidToStringA(&if_id_vector->IfId[i]->Uuid, &uuidString);
            if (status == RPC_S_OK)
            {
                std::string rpcInterfaceName = reinterpret_cast<char*>(uuidString);
                RpcStringFreeA(&uuidString);

                if (filePath.find(rpcInterfaceName) != std::string::npos)
                {
                    RpcIfIdVectorFree(&if_id_vector);
                    return true;
                }
            }
        }
    }

    RpcIfIdVectorFree(&if_id_vector);
    return checkExecutableServicePath(filePath);
}

bool FileCrawler::checkExecutableServicePath(const std::string& filePath)
{
    SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scManager)
    {
        std::cerr << "Failed to open service manager. Error: " << GetLastError() << std::endl;
        return false;
    }

    DWORD bytesNeeded = 0;
    DWORD serviceCount = 0;

    EnumServicesStatusEx(scManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &bytesNeeded, &serviceCount, NULL, NULL);

    std::vector<BYTE> buffer(bytesNeeded);
    ENUM_SERVICE_STATUS_PROCESS* services = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESS*>(buffer.data());

    if (EnumServicesStatusEx(scManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, 
    (LPBYTE)services, bytesNeeded, &bytesNeeded, &serviceCount, NULL, NULL))
    {
        for (DWORD i = 0; i < serviceCount; ++i)
        {
            LPQUERY_SERVICE_CONFIG lpsc = NULL;
            DWORD dwBytesNeeded = 0;
            SC_HANDLE hService = OpenService(scManager, services[i].lpServiceName, SERVICE_QUERY_CONFIG);
            if (hService) 
            {
                if (!QueryServiceConfig(hService, NULL, 0, &dwBytesNeeded) && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                {
                    lpsc = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LPTR, dwBytesNeeded);
                    if (lpsc && QueryServiceConfig(hService, lpsc, dwBytesNeeded, &dwBytesNeeded))
                    {
                        auto imagePath = lpsc->lpBinaryPathName;
                        if (filePath == std::string(imagePath, imagePath + _tcslen(imagePath)))
                        {
                            LocalFree(lpsc);
                            CloseServiceHandle(hService);
                            CloseServiceHandle(scManager);
                            return true;
                        }
                        
                    }
                    LocalFree(lpsc);
                }
                CloseServiceHandle(hService);
            }
        }
    }
    else
    {
        std::cerr << "Failed to enumerate services. Error: " << GetLastError() << std::endl;
    }

    CloseServiceHandle(scManager);
    return false;
}
