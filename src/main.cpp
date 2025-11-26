#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdint>
#include <ctime>
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
#include "Dashboard.hpp"
#include "ReplayEngine.hpp"

boost::asio::awaitable<void> runPollingLoop(MtaClient& client, boost::asio::io_context& io, SQLiteStore& db, StopManager& stops, std::vector<FeedEndpoint> const& feeds, bool recordMode)
{
    boost::asio::steady_timer timer(io);

    std::cout << "[System] Running initial database cleanup..." << std::endl;
    db.pruneOldData(7);
    auto lastPruneTime = std::chrono::steady_clock::now();

    static std::ofstream recFile;
    if (recordMode)
    {
        recFile.open("recordings/session.rec", std::ios::binary | std::ios::app);
        std::cout << "[System] Recording activated. Saving to recordings/session.rec" << std::endl;
    }

    for (;;)
    {
        std::cout << "\n[T=" << std::time(nullptr) << "] --- Data Fetch---" << std::endl;
        int totalProcessed = 0;

        for (const auto& feed : feeds)
        {
            try
            {
                std::string data = co_await client.fetch(feed.url);

                if (recordMode && recFile.is_open())
                {
                    uint64_t now = std::time(nullptr);
                    uint32_t size = static_cast<uint32_t>(data.size());
                    recFile.write(reinterpret_cast<const char*>(&now), sizeof(now));
                    recFile.write(reinterpret_cast<const char*>(&size), sizeof(size));
                    recFile.write(data.data(), size);
                    recFile.flush();
                }

                std::vector<TrainSnapshot> snapshots = Parser::extractSnapshots(data, stops);

                if (!snapshots.empty())
                {
                    db.insertMany(snapshots);
                    totalProcessed += static_cast<int>(snapshots.size());

                    std::cout << "   | " << feed.name << ": " << snapshots.size() << " trains." << std::endl;
                }
            }
            catch (std::exception const& e)
            {
                std::cerr << "Error fetching " << feed.name << ": " << e.what() << std::endl;
            }
        }

        std::cout << "   -> TOTAL: " << totalProcessed << " trains tracked." << std::endl;

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastPruneTime).count() > 3600)
        {
            std::cout << "   [Maintenance] Pruning data older than 7 days..." << std::endl;
            db.pruneOldData(7);
            lastPruneTime = now;
        }

        timer.expires_after(std::chrono::seconds(30));
        co_await timer.async_wait(boost::asio::use_awaitable);
    }
}

boost::asio::awaitable<void> handleHttpClient(std::shared_ptr<boost::asio::ip::tcp::socket> socket, SQLiteStore& db, StopManager& stops)
{
    try
    {
        boost::asio::streambuf buffer;

        co_await boost::asio::async_read_until(*socket, buffer, "\r\n\r\n", boost::asio::use_awaitable);

        auto stalls = db.getRecentStalls();
        std::string html = Dashboard::generate(stalls, stops);

        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: " + std::to_string(html.size()) + "\r\n"
            "Connection: close\r\n\r\n" +
            html;

        co_await boost::asio::async_write(*socket, boost::asio::buffer(response), boost::asio::use_awaitable);

        boost::system::error_code ignore;
        socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore);
    }
    catch (boost::system::system_error const& e)
    {
        auto code = e.code();
        if (code == boost::asio::error::operation_aborted ||
            code == boost::asio::error::connection_reset ||
            code == boost::asio::error::connection_aborted ||
            code == boost::asio::error::eof)
        {
            co_return;
        }

        std::cerr << "HTTP handler error: " << e.what() << "\n";
    }
    catch (std::exception const& e)
    {
        std::cerr << "HTTP handler error: " << e.what() << "\n";
    }
}

boost::asio::awaitable<void> httpAcceptLoop(boost::asio::ip::tcp::acceptor& acceptor, SQLiteStore& db, StopManager& stops)
{
    for (;;)
    {
        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(co_await boost::asio::this_coro::executor);

        co_await acceptor.async_accept(*socket, boost::asio::use_awaitable);
        boost::asio::co_spawn(socket->get_executor(), handleHttpClient(socket, db, stops), boost::asio::detached);
    }
}

void runHttpServer(SQLiteStore& db, StopManager& stops)
{
    try
    {
        boost::asio::io_context ioc;
        boost::asio::ip::tcp::acceptor acceptor(ioc, {boost::asio::ip::tcp::v4(), 8080});

        std::cout << "   -> Dashboard active at http://localhost:8080\n";

        boost::asio::co_spawn(ioc, httpAcceptLoop(acceptor, db, stops), boost::asio::detached);

        ioc.run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Server Error: " << e.what() << std::endl;
    }
}

void parseCommandLineArgs(int argc, char* argv[], bool& recordMode, bool& replayMode, std::string& replayFile)
{
    recordMode = false;
    replayMode = false;
    replayFile.clear();

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--record")
        {
            recordMode = true;
        }
        else if (arg == "--replay" && i + 1 < argc)
        {
            replayMode = true;
            replayFile = argv[++i];
        }
        else
        {
            std::cerr << "Warning: ignoring unknown or malformed argument: " << arg << "\n";
        }
    }
}

int main(int argc, char* argv[])
{
    try
    {
        bool replayMode = false;
        bool recordMode = false;
        std::string replayFile;

        parseCommandLineArgs(argc, argv, recordMode, replayMode, replayFile);

        boost::asio::io_context io;
        StopManager stops("data/stops.txt");
        auto terminals = Parser::detectTerminals("data/stop_times.txt", stops);
        stops.loadTerminals(terminals);

        SQLiteStore db("mtaHistory.db");
        db.importStaticSchedule("data/stop_times.txt");

        std::cout << "System Initialized.\n";
        std::thread serverThread([&db, &stops]()
        {
            runHttpServer(db, stops);
        });
        serverThread.detach();

        if (replayMode)
        {
            ReplayEngine::run(replayFile, db, stops);
            std::cout << "Replay Finished. Dashboard is static. Press Enter to exit." << std::endl;
            std::cin.get();
        }
        else
        {
            ConfigurationManager config;
            MtaClient client(io, config.getAPIKey());
            const auto& feeds = config.getFeeds();

            boost::asio::co_spawn(io, runPollingLoop(client, io, db, stops, feeds, recordMode), boost::asio::detached);

            io.run();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Main Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
