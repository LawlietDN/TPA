#include "SQLiteStore.hpp"
#include <iostream>

SQLiteStore::SQLiteStore(std::string const& path)
    : db(nullptr), insertStmt(nullptr)
{
    sqlite3_open(path.c_str(), &db);

    const char* createSql =
        "CREATE TABLE IF NOT EXISTS Snapshots ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "timestamp INTEGER, "
        "tripId TEXT, "
        "routeId TEXT, "
        "trainId TEXT, "
        "direction INTEGER, "
        "isAssigned INTEGER, "
        "stopId TEXT, "
        "currentStatus INTEGER, "
        "delay INTEGER"
        ");";

    sqlite3_exec(db, createSql, nullptr, nullptr, nullptr);

    const char* insertSql =
        "INSERT INTO Snapshots "
        "(timestamp, tripId, routeId, trainId, direction, isAssigned, stopId, currentStatus, delay) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_prepare_v2(db, insertSql, -1, &insertStmt, nullptr);
}

SQLiteStore::~SQLiteStore()
{
    if (insertStmt) sqlite3_finalize(insertStmt);
    if (db) sqlite3_close(db);
}

void SQLiteStore::insert(const TrainSnapshot& s) {
    std::lock_guard<std::mutex> lock(mutex);
    insertInternal(s);
}


void SQLiteStore::insertInternal(TrainSnapshot const& s)
{
    sqlite3_reset(insertStmt);

    sqlite3_bind_int64(insertStmt, 1, static_cast<sqlite3_int64>(s.timestamp));
    sqlite3_bind_text(insertStmt, 2, s.tripId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertStmt, 3, s.routeId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insertStmt, 4, s.trainId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(insertStmt, 5, s.direction);
    sqlite3_bind_int(insertStmt, 6, s.isAssigned ? 1 : 0);
    sqlite3_bind_text(insertStmt, 7, s.stopId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(insertStmt, 8, s.currentStatus);
    sqlite3_bind_int(insertStmt, 9, s.delay);

    sqlite3_step(insertStmt);
}

void SQLiteStore::insertMany(const std::vector<TrainSnapshot>& snapshots)
{
    std::lock_guard<std::mutex> lock(mutex); 
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    for (const TrainSnapshot& s : snapshots)
        insertInternal(s);

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}


std::vector<TrainSnapshot> SQLiteStore::getRecentStalls()
{
    std::lock_guard<std::mutex> lock(mutex);

    std::vector<TrainSnapshot> results;

        const char* sql =
        "SELECT "
        "  tripId, routeId, trainId, direction, stopId, "
        "  MAX(timestamp) - MIN(timestamp) as dwellTimeSeconds, "
        "  MAX(delay) as reportedDelay "
        "FROM Snapshots "
        "WHERE currentStatus = 1 "
        "AND timestamp > (strftime('%s','now') - 1800) "   
        "GROUP BY tripId, stopId "
        "HAVING dwellTimeSeconds > 60 "
        "AND MAX(timestamp) > (strftime('%s','now') - 60) " 
        "ORDER BY dwellTimeSeconds DESC;";



    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return results;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        TrainSnapshot s;

        const unsigned char* tripId = sqlite3_column_text(stmt, 0);
        const unsigned char* routeId = sqlite3_column_text(stmt, 1);
        const unsigned char* trainId = sqlite3_column_text(stmt, 2);
        const unsigned char* stopId = sqlite3_column_text(stmt, 4);

        s.tripId        = tripId  ? reinterpret_cast<const char*>(tripId)  : "";
        s.routeId       = routeId ? reinterpret_cast<const char*>(routeId) : "";
        s.trainId       = trainId ? reinterpret_cast<const char*>(trainId) : "";
        
        s.direction     = sqlite3_column_int(stmt, 3);
        s.stopId        = stopId  ? reinterpret_cast<const char*>(stopId)  : "";
        
        s.dwellTimeSeconds = sqlite3_column_int(stmt, 5); 
        s.delay            = sqlite3_column_int(stmt, 6);
        s.currentStatus = 1; 

        results.push_back(s);
    }

    sqlite3_finalize(stmt);
    return results;


}

void SQLiteStore::pruneOldData(int daysToKeep)
{
    std::lock_guard<std::mutex> lock(mutex);
    long long cutoffSeconds = daysToKeep * 86400LL;
    std::string sql = "DELETE FROM Snapshots WHERE timestamp < (strftime('%s','now') - " + std::to_string(cutoffSeconds) + ");";
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK)
    {
        std::cerr << "WARNING: Prune failed" << '\n';
        sqlite3_free(errMsg);
    }
    
}