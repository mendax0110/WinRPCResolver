#include "../include/RpcMonitor.h"
#include <windows.h>
#include <evntrace.h>
#include <tdh.h>
#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <thread>

RpcMonitor::RpcMonitor(const RpcServersConfig& config) : rpcServersConfig(config) {}

TRACEHANDLE sessionHandle = 0;
VOID WINAPI EtwEventCallback(PEVENT_RECORD eventRecord);
const GUID SystemTraceControlGuid = { 0x9e814c01, 0x5b65, 0x11d0, {0x8f, 0x20, 0x00, 0xaa, 0x00, 0x3e, 0x00, 0x00} };

std::string GuidToString(const GUID& guid)
{
    char buffer[64] = { 0 };
    snprintf(buffer, sizeof(buffer),
        "{%08x-%04x-%04x-%04x-%012llx}",
        guid.Data1, guid.Data2, guid.Data3,
        (guid.Data4[0] << 8) | guid.Data4[1],
        *((uint64_t*)(&guid.Data4[2])));

    return std::string(buffer);
}

void RpcMonitor::start()
{
    std::cout << "Starting RPC session..." << std::endl;
    EVENT_TRACE_PROPERTIES* sessionProperties;
    ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(KERNEL_LOGGER_NAME);
    sessionProperties = (EVENT_TRACE_PROPERTIES*)malloc(bufferSize);
    if (!sessionProperties)
    {
        throw std::runtime_error("Memory allocation failed for ETW session properties.");
    }

    ZeroMemory(sessionProperties, bufferSize);

    sessionProperties->Wnode.BufferSize = bufferSize;
    sessionProperties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    sessionProperties->Wnode.ClientContext = 1;
    sessionProperties->Wnode.Guid = SystemTraceControlGuid;
    sessionProperties->EnableFlags = EVENT_TRACE_FLAG_NETWORK_TCPIP;
    sessionProperties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    sessionProperties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    ULONG status = StartTrace(&sessionHandle, KERNEL_LOGGER_NAME, sessionProperties);
    if (status != ERROR_SUCCESS)
    {
        free(sessionProperties);
        throw std::runtime_error("Failed to start ETW session. Error: " + std::to_string(status));
    }

    GUID RpcProviderGuid = {0x6ad52b32, 0xd609, 0x4be9, {0xae, 0x07, 0xce, 0x8d, 0xae, 0x93, 0x7e, 0x39}};

    TRACE_GUID_REGISTRATION guidReg;
    guidReg.Guid = &RpcProviderGuid;
    guidReg.RegHandle = nullptr;

    EVENT_TRACE_LOGFILE logFile = { 0 };
    logFile.LoggerName = KERNEL_LOGGER_NAME;
    logFile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logFile.EventRecordCallback = EtwEventCallback;

    TRACEHANDLE traceHandle = OpenTrace(&logFile);
    if (traceHandle == INVALID_PROCESSTRACE_HANDLE)
    {
        StopTrace(sessionHandle, KERNEL_LOGGER_NAME, sessionProperties);
        free(sessionProperties);
        throw std::runtime_error("Failed to open ETW trace. Error: " + std::to_string(GetLastError()));
    }

    std::thread traceThread([traceHandle]() {
        TRACEHANDLE handles[1] = { traceHandle };
        ProcessTrace(handles, 1, nullptr, nullptr);
    });

    traceThread.detach();
    free(sessionProperties);
}

void RpcMonitor::stop()
{
    std::cout << "Stopping RPC session..." << std::endl;
    EVENT_TRACE_PROPERTIES* sessionProperties;
    ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(KERNEL_LOGGER_NAME);
    sessionProperties = (EVENT_TRACE_PROPERTIES*)malloc(bufferSize);
    if (!sessionProperties)
    {
        throw std::runtime_error("Memory allocation failed for stopping ETW session.");
    }

    ZeroMemory(sessionProperties, bufferSize);

    sessionProperties->Wnode.BufferSize = bufferSize;
    sessionProperties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    ULONG status = StopTrace(sessionHandle, KERNEL_LOGGER_NAME, sessionProperties);
    if (status != ERROR_SUCCESS)
    {
        free(sessionProperties);
        throw std::runtime_error("Failed to stop ETW session. Error: " + std::to_string(status));
    }

    free(sessionProperties);
}

bool RpcMonitor::isRpcClientCall(int eventCode)
{
    // RPC client calls are event ID 5
    return eventCode == 5;
}

std::vector<RpcEvent> RpcMonitor::getEvents() const
{
    return collectedEvents;
}

VOID WINAPI EtwEventCallback(PEVENT_RECORD eventRecord)
{
    std::cout << "Event received, Event ID: " << eventRecord->EventHeader.EventDescriptor.Id << std::endl;

    if (eventRecord->EventHeader.EventDescriptor.Id == 5)
    {
        std::cout << "RPC Client Call detected" << std::endl;
        auto event = std::map<std::string, std::string>();
        for (ULONG i = 0; i < eventRecord->UserDataLength; i++)
        {
            event["Field" + std::to_string(i)] = std::to_string(((PBYTE)eventRecord->UserData)[i]);
        }
    }
}

void RpcMonitor::etwCallback(const std::map<std::string, std::string>& event)
{
    // TODO: IMPLEMENT THIS!
}
