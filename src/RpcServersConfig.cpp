#include "../include/RpcServersConfig.h"
#include "../externals/json/single_include/nlohmann/json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

RpcServersConfig::RpcServersConfig(const std::map<std::string, std::map<std::string, std::string>>& rpcServersMap) 
    : rpcServersMap(rpcServersMap) {}

std::map<std::string, std::string> RpcServersConfig::getRpcInfo(const std::string& interfaceUuid, int funcOpnum)
{
    auto it = rpcServersMap.find(interfaceUuid);
    if (it == rpcServersMap.end())
    {
        return {};
    }

    auto rpcServer = it->second;
    std::map<std::string, std::string> rpcInfo;
    rpcInfo["FileName"] = rpcServer["FileName"];

    if (!rpcServer["ServiceDisplayName"].empty())
    {
        rpcInfo["ServiceDisplayName"] = rpcServer["ServiceDisplayName"];
    }

    if (!rpcServer["ServiceName"].empty())
    {
        rpcInfo["ServiceName"] = rpcServer["ServiceName"];
    }

    if (funcOpnum >= 0 && funcOpnum < std::stoi(rpcServer["ProcedureCount"]))
    {
        rpcInfo["ProcedureName"] = rpcServer["Procedures"][funcOpnum]["Name"];
    }

    return rpcInfo;
}

RpcServersConfig RpcServersConfig::load(const std::string& filePath)
{
    std::ifstream rpcServersFile(filePath);
    if (!rpcServersFile.is_open())
    {
        throw std::runtime_error("Could not open file: " + filePath);
    }

    json root;
    rpcServersFile >> root;

    std::map<std::string, std::map<std::string, std::string>> rpcServersMap;

    for (const auto& rpcServer : root)
    {
        std::string uuidKey = "{" + rpcServer["InterfaceUuid"].get<std::string>() + "}";
        rpcServersMap[uuidKey] = {
            {"FileName", rpcServer["FileName"].get<std::string>()},
            {"ServiceDisplayName", rpcServer["ServiceDisplayName"].get<std::string>()},
            {"ServiceName", rpcServer["ServiceName"].get<std::string>()},
            {"ProcedureCount", std::to_string(rpcServer["Procedures"].size())}
        };
    }

    return RpcServersConfig(rpcServersMap);
}
