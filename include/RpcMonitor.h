#ifndef RPCMONITOR_H
#define RPCMONITOR_H

#include "../include/RpcServersConfig.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

/// @brief RpcEvent struct to store RPC event information \struct RpcEvent
struct RpcEvent
{
    int ProcessId;
    int ThreadId;
    uint64_t Timestamp;
    std::string InterfaceUuid;
    int ProcedureNum;
    std::string Endpoint;
    std::string Protocol;
    std::string FileName;
    std::string ServiceDisplayName;
    std::string ServiceName;
    std::string ProcedureName;
};

/// @brief RpcMonitor class to monitor RPC events \class RpcMonitor
class RpcMonitor
{
public:
    RpcMonitor(const RpcServersConfig& config);
    
    /*!
     * @brief Start the RPC monitor
     */
    void start();
    
    /*!
     * @brief Stop the RPC monitor
     */
    void stop();
    
    /*!
     * @brief Get the RPC events captured by the monitor
     * @return std::vector<RpcEvent> The RPC events
     */
    std::vector<RpcEvent> getEvents() const;

private:
    RpcServersConfig rpcServersConfig;
    std::vector<RpcEvent> collectedEvents;
    std::mutex lock;

    /*!
     * @brief Check if the event is an RPC client call
     * @param eventCode The event code
     * @return bool True if it's an RPC client call, false otherwise
     */
    bool isRpcClientCall(int eventCode);
    
    /*!
     * @brief Parse the RPC event from the event data
     * @param event The event data
     * @return RpcEvent The parsed RPC event
     */
    RpcEvent parseRpcEvent(const std::map<std::string, std::string>& event);

    /*!
     * @brief ETW callback function to process the events
     * @param event The event data
     */
    void etwCallback(const std::map<std::string, std::string>& event);
};

#endif // RPCMONITOR_H