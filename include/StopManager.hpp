#pragma once
#include <string>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <sstream>

class StopManager
{
private:
    std::unordered_map<std::string, std::string> stopNames;

public:
    StopManager(std::string const& filepath)
    {
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            std::cerr << "WARNING: Could not open stops.txt. Station names will be IDs." << std::endl;
            return;
        }

        std::string line;
        std::getline(file, line); 

        while (std::getline(file, line))
        {
            if (line.empty()) continue;
            
            std::stringstream ss(line);
            std::string segment;
            std::vector<std::string> row;
            
            while (std::getline(ss, segment, ','))
            {
                row.push_back(segment);
            }

            if (row.size() >= 2)
            {
                std::string id = row[0];
                std::string name = row[1];
                stopNames[id] = name;
            }
        }
        std::cout << "Loaded " << stopNames.size() << " stations." << std::endl;
    }

    std::string getName(std::string const& stopId)
    {
        if (stopNames.count(stopId)) return stopNames[stopId];
        if (stopId.length() > 1)
        {
            std::string parent = stopId.substr(0, stopId.length() - 1);
            if (stopNames.count(parent)) return stopNames[parent];
        }

        return stopId; 
    }
};
