#ifndef RPCSERVERSCONFIG_H
#define RPCSERVERSCONFIG_H

#include <string>
#include <vector>
#include <map>
#include <mutex>

/// @brief RpcEvent struct to store RPC event information \class RpcEvent
class RpcServersConfig
{
public:
    RpcServersConfig(const std::map<std::string, std::map<std::string, std::string>>& rpcServersMap);
    
    /*!
     * @brief Get the RPC information based on the interface UUID and function opnum
     * @param interfaceUuid The interface UUID
     * @param funcOpnum The function opnum
     * @return std::map<std::string, std::string> The RPC information
     */
    std::map<std::string, std::string> getRpcInfo(const std::string& interfaceUuid, int funcOpnum);
    
    /*!
     * @brief Load the RPC servers configuration from a file
     * @param filePath The file path
     * @return RpcServersConfig The RPC servers configuration
     */
    static RpcServersConfig load(const std::string& filePath);

private:
    std::map<std::string, std::map<std::string, std::string>> rpcServersMap;
};

#endif // RPCSERVERSCONFIG_H