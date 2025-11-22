#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include "ConfigurationManager.hpp"
#include "MtaClient.hpp"
#include "gtfs-realtime.pb.h"
#include "nyct-subway.pb.h"
#include "Types.hpp"
#include "Parser.hpp"
#include "SQLiteStore.hpp"
#include "StopManager.hpp"

boost::asio::awaitable<void> runPollingLoop(MtaClient& client, boost::asio::io_context& io, SQLiteStore& store, StopManager& stops, auto const& feeds)
{
    boost::asio::steady_timer timer(io);

    for (;;)
    {
        std::cout << "\n[T=" << std::time(nullptr) << "] --- SYSTEM SWEEP START ---" << std::endl;
        int totalProcessed = 0;

        for (const auto& feed : feeds)
        {
            try
            {
                std::string data = co_await client.fetch(feed.url);
                std::vector<TrainSnapshot> snapshots = Parser::extractSnapshots(data, stops);
                if (!snapshots.empty())
                {
                    store.insertMany(snapshots);
                    totalProcessed += snapshots.size();
                    std::cout << "   | " << feed.name << ": " << snapshots.size() << " trains." << std::endl;
                }
                std::cout << "Stored " << snapshots.size() << " snapshots\n";
            }
            catch (const std::exception& e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
            }
            std::cout << "   -> TOTAL: " << totalProcessed << " trains tracked." << std::endl;
            timer.expires_after(std::chrono::seconds(30));
            co_await timer.async_wait(boost::asio::use_awaitable);
        }
    }
}


int main()
{ 
    try
    {
        boost::asio::io_context io;
        ConfigurationManager config;

        StopManager stops("stops.txt");
        SQLiteStore store("mtaHistory.db");

        std::cout << "API Key found.\n";

        MtaClient client(io, config.getAPIKey());
        const auto& feeds = config.getFeeds();
        
        boost::asio::co_spawn(io, runPollingLoop(client, io, store, stops, feeds), boost::asio::detached);
        io.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Main Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
