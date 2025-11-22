#pragma once
#include <string>
#include <vector>
#include "sqlite3.h"
#include "Types.hpp"

class SQLiteStore
{
private:
    sqlite3* db;
    sqlite3_stmt* insertStmt;

public:
    SQLiteStore(std::string const& path);
    ~SQLiteStore();

    void insert(const TrainSnapshot& s);
    void insertMany(const std::vector<TrainSnapshot>& snapshots);
};
