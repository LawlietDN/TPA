#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include "ConfigurationManager.hpp"
#include "MtaClient.hpp"
#include "gtfs-realtime.pb.h"
#include "nyct-subway.pb.h"

void parse(std::string& data)
{
    if(data.size() > 0 && data[0] == '<')
    {
        std::cerr << "WARNING: Response looks like HTML/XML, not Protobuf." << std::endl;
        std::cout << data << std::endl;
    }
    transit_realtime::FeedMessage feed;
    if (!feed.ParseFromString(data))
    {
        std::cerr << "CRITICAL: Failed to parse Protocol Buffer." << std::endl;
        return;
    }

    std::cout << "--------------------------------------------------" << std::endl;
    std::cout << "SYSTEM TIME: " << feed.header().timestamp() << std::endl;
    std::cout << "ENTITIES DETECTED: " << feed.entity_size() << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    int count = 0;
    for (const auto& entity : feed.entity())
    {
        if (count++ >= 5) break;

        if (entity.has_trip_update())
        {
            const auto& tu = entity.trip_update();
            std::cout << "[TRIP] ID: " << tu.trip().trip_id() << " | Route: " << tu.trip().route_id() << std::endl;
            if (tu.trip().HasExtension(transit_realtime::nyct_trip_descriptor))
            {
                const auto& ext = tu.trip().GetExtension(transit_realtime::nyct_trip_descriptor);

                std::cout << "Train ID: " << ext.train_id() << '\n';
                std::cout << "Assigned: " << ext.is_assigned() << '\n';
                std::cout << "Direction: " << ext.direction() << '\n';
}

        }
    }
}

int main() 
{
    try
    {
        boost::asio::io_context io;
        ConfigurationManager config;

        std::cout << "Target: " << ConfigurationManager::MTA_PORT << '\n';
        std::cout << "API Key found." << '\n';

        MtaClient client(io, config.getAPIKey());

        boost::asio::co_spawn(io, 
            [&client]() -> boost::asio::awaitable<void>
            {
                try {
                    std::string data = co_await client.fetch();
                    parse(data);        
        
                }
                catch (std::exception const& e)
                {
                    std::cerr << "Async Error: " << e.what() << std::endl;
                }
            }, 
            boost::asio::detached
        );

        io.run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Main Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}