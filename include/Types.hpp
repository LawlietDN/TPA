#pragma once
#include <string>
#include <cstdint>

// Represents the state of one train at one specific second.
struct TrainSnapshot
{
    std::string tripId;
    std::string routeId; 
    std::string trainId;     // The physical hardware ID (from NYCT extension)
    int32_t direction;
    uint64_t timestamp;
    bool isAssigned;         // Is it actually running?
    std::string stopId;       // "A12S" (Station Code)
    int32_t currentStatus;    // 0=INCOMING, 1=STOPPED_AT, 2=IN_TRANSIT_TO
    int32_t delay = 0;
    int dwellTimeSeconds = 0; 

    
};