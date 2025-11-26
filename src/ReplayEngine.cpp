#include "ReplayEngine.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <ctime>
#include "Parser.hpp"
#include "SQLiteStore.hpp"
#include "VirtualClock.hpp"
#include "StopManager.hpp"

bool ReplayEngine::readChunkHeader(std::ifstream& file, std::uint64_t& timestamp, std::uint32_t& size)
{
    file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
    file.read(reinterpret_cast<char*>(&size), sizeof(size));

    if (file.eof() || !file.good())
        return false;

    return true;
}

void ReplayEngine::syncRealtime(std::uint64_t timestamp, std::uint64_t& replayStart, std::uint64_t realStart)
{
    if (replayStart == 0)
    {
        replayStart = timestamp;
    }

    std::uint64_t recordingDelta = timestamp - replayStart;
    std::uint64_t realDelta      = static_cast<std::uint64_t>(std::time(nullptr)) - realStart;

    if (recordingDelta > realDelta)
    {
        std::uint64_t waitSeconds = recordingDelta - realDelta;
        if (waitSeconds > 1)
        {
            std::cout << "[REPLAY] Syncing... sleeping for "
                      << waitSeconds << "s" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(waitSeconds));
    }
}

void ReplayEngine::processChunk(std::string& data, std::uint64_t timestamp, SQLiteStore& db, StopManager& stops)
{
    std::cout << "[REPLAY] Ingesting Snapshot (Recorded T="
              << timestamp << ")" << std::endl;

    VirtualClock::set(static_cast<std::time_t>(timestamp));

    auto snapshots = Parser::extractSnapshots(data, stops);
    if (!snapshots.empty())
    {
        std::uint64_t now = static_cast<std::uint64_t>(std::time(nullptr));
        for (auto& s : snapshots)
        {
            s.timestamp = now;
        }
        db.insertMany(snapshots);
    }
}

void ReplayEngine::run(std::string const& filename, SQLiteStore& db, StopManager& stops)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Failed to open replay file: " << filename << std::endl;
        return;
    }

    std::cout << ">>> STARTING REPLAY MODE (1:1 SPEED) <<<" << std::endl;

    std::uint64_t replayStart = 0;
    std::uint64_t realStart   = static_cast<std::uint64_t>(std::time(nullptr));

    while (file.peek() != EOF)
    {
        std::uint64_t timestamp = 0;
        std::uint32_t size      = 0;

        if (!readChunkHeader(file, timestamp, size))
            break;

        syncRealtime(timestamp, replayStart, realStart);

        std::string data(size, '\0');
        file.read(&data[0], size);

        processChunk(data, timestamp, db, stops);
    }

    VirtualClock::disable();
    std::cout << ">>> REPLAY COMPLETE <<<" << std::endl;
}
