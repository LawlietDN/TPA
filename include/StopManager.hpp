#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>

class StopManager
{
private:
    std::unordered_map<std::string, std::string> parentOf;
    std::unordered_map<std::string, std::string> stationNames;
    std::unordered_set<std::string> validStops;
    std::unordered_set<std::string> terminalStations;

public:
    StopManager(std::string const& filepath);
    bool exists(std::string const& stopId) const;
    std::string getParent(std::string const& stopId) const;
    std::string getName(std::string const& stopId) const;
    bool isTerminal(std::string const& stopId) const;
    void loadTerminals(const std::unordered_set<std::string>& terminals);
};
