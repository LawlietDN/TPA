#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "sqlite3.h"
#include "Types.hpp"

class SQLiteStore
{
private:
    sqlite3* db;
    sqlite3_stmt* insertStmt;
    std::mutex mutex;

public:
    SQLiteStore(std::string const& path);
    ~SQLiteStore();

    void insert(TrainSnapshot const& s);
    void insertMany(std::vector<TrainSnapshot> const& snapshots);
    void insertInternal(TrainSnapshot const& s);
    void pruneOldData(int daysToKeep);
    std::vector<TrainSnapshot> getRecentStalls();
};
