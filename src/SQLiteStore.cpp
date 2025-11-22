#include "SQLiteStore.hpp"

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

void SQLiteStore::insert(const TrainSnapshot& s)
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
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    for (const TrainSnapshot& s : snapshots)
        insert(s);

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}
