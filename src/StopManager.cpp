#include "StopManager.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

StopManager::StopManager(std::string const& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        std::cerr << "ERROR: Could not open " << filepath << " — StopManager unusable!\n";
        return;
    }

    std::string line;
    std::getline(file, line); 

    while (std::getline(file, line))
    {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string stop_id, stop_name, lat, lon, location_type, parent_station;

        std::getline(ss, stop_id, ',');
        std::getline(ss, stop_name, ',');
        std::getline(ss, lat, ',');
        std::getline(ss, lon, ',');
        std::getline(ss, location_type, ',');
        std::getline(ss, parent_station, ',');

        validStops.insert(stop_id);

        bool isParent = (!location_type.empty() && location_type == "1");

        if (isParent)
        {
            parentOf[stop_id]     = stop_id;
            stationNames[stop_id] = stop_name;
        }
        else
        {
            if (!parent_station.empty())
            {
                parentOf[stop_id] = parent_station;

                if (!stationNames.count(parent_station))
                    stationNames[parent_station] = stop_name;
            }
            else
            {
                std::string inferred = stop_id.substr(0, stop_id.size() - 1);
                parentOf[stop_id]    = inferred;
                stationNames[inferred] = stop_name;
            }
        }
    }

    std::cout << "Loaded " << validStops.size()
              << " stops (" << stationNames.size()
              << " parent stations)\n";
}

bool StopManager::exists(std::string const& stopId) const
{
    return validStops.count(stopId) > 0;
}

std::string StopManager::getParent(std::string const& stopId) const
{
    auto it = parentOf.find(stopId);
    if (it != parentOf.end())
        return it->second;

    if (stopId.size() > 1)
        return stopId.substr(0, stopId.size() - 1);

    return stopId;
}

std::string StopManager::getName(std::string const& stopId) const
{
    std::string parent = getParent(stopId);

    auto it = stationNames.find(parent);
    if (it != stationNames.end())
        return it->second;

    return parent;
}

bool StopManager::isTerminal(std::string const& stopId) const
{
    std::string parent = getParent(stopId);
    return terminalStations.count(parent) > 0;
}

void StopManager::loadTerminals(const std::unordered_set<std::string>& terminals)
{
    terminalStations = terminals;
    std::cout << "Loaded " << terminalStations.size()
              << " terminal stations.\n";
}
