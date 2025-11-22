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
#include <Dashboard.hpp>

boost::asio::awaitable<void> runPollingLoop(MtaClient& client, boost::asio::io_context& io, SQLiteStore& db, StopManager& stops, std::vector<FeedEndpoint> const& feeds)
{
    boost::asio::steady_timer timer(io);
    std::cout << "[System] Running initial database cleanup..." << std::endl;
    db.pruneOldData(7);
    auto lastPruneTime = std::chrono::steady_clock::now();

    for (;;)
    {
        std::cout << "\n[T=" << std::time(nullptr) << "] --- SYSTEM SWEEP START ---" << std::endl;
        int totalProcessed = 0;

        for (const auto& feed : feeds)
        {
            try
            {
                std::string data = co_await client.fetch(feed.url);

                std::vector<TrainSnapshot> snapshots =
                    Parser::extractSnapshots(data, stops);

                if (!snapshots.empty())
                {
                    db.insertMany(snapshots);
                    totalProcessed += snapshots.size();

                    std::cout << "   | " << feed.name 
                              << ": " << snapshots.size() 
                              << " trains." << std::endl;
                }

            }
            catch (std::exception const& e)
            {
                std::cerr << "Error fetching " << feed.name
                          << ": " << e.what() << std::endl;
            }
        }

        std::cout << "   -> TOTAL: " << totalProcessed << " trains tracked." << std::endl;

        auto now = std::chrono::steady_clock::now();
        auto secondsSincePrune = std::chrono::duration_cast<std::chrono::seconds>(now - lastPruneTime).count();
        
        if (secondsSincePrune > 3600)
        {
            std::cout << "   [Maintenance] Pruning data older than 7 days..." << std::endl;
            db.pruneOldData(7);
            lastPruneTime = now; 
        }

        timer.expires_after(std::chrono::seconds(30));
        co_await timer.async_wait(boost::asio::use_awaitable);
    }
}

int main()
{
    try
    {
        boost::asio::io_context io;
        ConfigurationManager config;

        StopManager stops("stops.txt");
        auto terminals = Parser::detectTerminals("stop_times.txt", stops);
        stops.loadTerminals(terminals);

        SQLiteStore db("mtaHistory.db");

        std::cout << "API Key found.\nSystem Initialized\n";

     std::thread serverThread([&db, &stops]()
     {

    try
    {
        boost::asio::io_context ioc;
        boost::asio::ip::tcp::acceptor acceptor(ioc, {boost::asio::ip::tcp::v4(), 8080});

        std::cout << "   -> Dashboard active at http://localhost:8080\n";

        auto handle_client = [&](std::shared_ptr<boost::asio::ip::tcp::socket> sock) -> boost::asio::awaitable<void>
        {
            try
            {
                boost::asio::streambuf buffer;

                co_await boost::asio::async_read_until(
                    *sock, buffer, "\r\n\r\n", boost::asio::use_awaitable);

                auto stalls = db.getRecentStalls();
                std::string html = Dashboard::generate(stalls, stops);

                std::string response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: " + std::to_string(html.size()) + "\r\n"
                    "Connection: close\r\n\r\n" +
                    html;

                co_await boost::asio::async_write(
                    *sock,
                    boost::asio::buffer(response),
                    boost::asio::use_awaitable);

                boost::system::error_code ignore;
                sock->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore);
            }
            catch (std::exception const& e)
            {
                std::cerr << "HTTP handler error: " << e.what() << "\n";
            }
        };

        auto accept_loop = [&]() -> boost::asio::awaitable<void>
        {
            for (;;)
            {
                auto sock = std::make_shared<boost::asio::ip::tcp::socket>(co_await boost::asio::this_coro::executor);

                co_await acceptor.async_accept(*sock, boost::asio::use_awaitable);

                std::cout << "HTTP connection accepted\n";
                boost::asio::co_spawn(
                    sock->get_executor(),
                    handle_client(sock),
                    boost::asio::detached);
            }
        };

        boost::asio::co_spawn(ioc, accept_loop(), boost::asio::detached);
        ioc.run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Server Thread Error: " << e.what() << std::endl;
    }
});


        serverThread.detach(); 
        MtaClient client(io, config.getAPIKey());
        const auto& feeds = config.getFeeds();

        boost::asio::co_spawn(
            io,
            runPollingLoop(client, io, db, stops, feeds),
            boost::asio::detached
        );

        io.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Main Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
