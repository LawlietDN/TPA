#pragma once

#include <string>
#include <fstream>
#include <cstdint>

class SQLiteStore;
class StopManager;

class ReplayEngine
{
public:
    static void run(std::string const& filename, SQLiteStore& db, StopManager& stops);

private:
    static bool readChunkHeader(std::ifstream& file,std::uint64_t& timestamp, std::uint32_t& size);
    static void syncRealtime(std::uint64_t timestamp, std::uint64_t& replayStart, std::uint64_t realStart);
    static void processChunk(std::string& data, std::uint64_t timestamp, SQLiteStore& db, StopManager& stops);
};
