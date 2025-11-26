#include "Parser.hpp"
#include "StopManager.hpp"
#include <fstream>
std::vector<TrainSnapshot> Parser::extractSnapshots(std::string const& data, StopManager& stops)
{
    if (data.empty() || data[0] == '<')
        return {};

    transit_realtime::FeedMessage feed;
    if (!feed.ParseFromString(data))
        return {};

    uint64_t ts = feed.header().timestamp();
    std::unordered_map<std::string, TrainSnapshot> mergeMap;

    for (const auto& entity : feed.entity())
    {
        std::string tripId;

        if (entity.has_trip_update())
            tripId = entity.trip_update().trip().trip_id();
        else if (entity.has_vehicle())
            tripId = entity.vehicle().trip().trip_id();
        else
            continue;

        TrainSnapshot& snap = mergeMap[tripId];
        snap.tripId    = tripId;
        snap.timestamp = ts;

        if (entity.has_trip_update())
        {
            const auto& tu = entity.trip_update();
            snap.routeId = tu.trip().route_id();

            for (int i = 0; i < tu.stop_time_update_size(); ++i)
            {
                const auto& st = tu.stop_time_update(i);
                if (st.has_arrival() && st.arrival().has_delay())
                {
                    snap.delay = st.arrival().delay();
                    break;
                }
                if (st.has_departure() && st.departure().has_delay())
                {
                    snap.delay = st.departure().delay();
                    break;
                }
            }

            if (tu.trip().HasExtension(transit_realtime::nyct_trip_descriptor))
            {
                const auto& ext =
                    tu.trip().GetExtension(transit_realtime::nyct_trip_descriptor);
                snap.trainId    = ext.train_id();
                snap.direction  = ext.direction();
                snap.isAssigned = ext.is_assigned();
            }
        }

        if (entity.has_vehicle())
        {
            const auto& v = entity.vehicle();
            snap.routeId       = v.trip().route_id();
            snap.stopId        = v.stop_id();         
            snap.currentStatus = v.current_status();

            if (v.trip().HasExtension(transit_realtime::nyct_trip_descriptor))
            {
                const auto& ext =
                    v.trip().GetExtension(transit_realtime::nyct_trip_descriptor);
                snap.trainId    = ext.train_id();
                snap.direction  = ext.direction();
                snap.isAssigned = ext.is_assigned();
            }
        }
    }

    std::vector<TrainSnapshot> out;
    out.reserve(mergeMap.size());

    for (auto& kv : mergeMap)
    {
        TrainSnapshot& s = kv.second;

        if (s.stopId.empty())          continue;
        if (!stops.exists(s.stopId))   continue;
        if (stops.isTerminal(s.stopId)) continue;

        out.push_back(std::move(s));
    }

    return out;
}


std::unordered_set<std::string> Parser::detectTerminals(std::string const& stopTimesPath, StopManager& stops)
{
    std::unordered_map<std::string, int> terminalCount;
    std::ifstream f(stopTimesPath);
    if (!f.is_open())
    {
        std::cerr << "Failed to open " << stopTimesPath << "\n";
        return {};
    }


    std::string line;
    std::getline(f, line); 

    std::string prevTrip;
    std::string firstParent;
    std::string lastParent;

    while (std::getline(f, line))
    {
        std::stringstream ss(line);

        std::string trip_id, stop_id, arrival, depart, seqStr;
        std::getline(ss, trip_id, ',');
        std::getline(ss, stop_id, ',');
        std::getline(ss, arrival, ',');
        std::getline(ss, depart, ',');
        std::getline(ss, seqStr, ',');

        if (!stops.exists(stop_id))
            continue;

        std::string parent = stops.getParent(stop_id);

        if (trip_id != prevTrip)
        {
            if (!prevTrip.empty() && !firstParent.empty() && !lastParent.empty()) {
                terminalCount[firstParent]++;
                terminalCount[lastParent]++;
            }

            prevTrip = trip_id;
            firstParent = parent;
            lastParent = parent;
        }
        else {
            lastParent = parent;
        }
    }

    if (!firstParent.empty() && !lastParent.empty()) {
        terminalCount[firstParent]++;
        terminalCount[lastParent]++;
    }

    std::unordered_set<std::string> terminals;

    for (auto& p : terminalCount)
    {
        if (p.second >= 100)
        {
            terminals.insert(p.first);
        }
    }

    return terminals;
}

