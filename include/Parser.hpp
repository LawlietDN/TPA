#pragma once
#include <string>
#include <vector>
#include "gtfs-realtime.pb.h"
#include "nyct-subway.pb.h"
#include "Types.hpp"
#include "StopManager.hpp"

class Parser
{
public:
    static std::vector<TrainSnapshot> extractSnapshots(std::string const& data, StopManager& stops);
    static std::unordered_set<std::string> detectTerminals(std::string const& stopTimesPath, StopManager& stops);

};
