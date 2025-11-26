#include <ctime>
#include <fstream>
#include <sstream>
#include <iostream>
#include "SQLiteStore.hpp"

SQLiteStore::SQLiteStore(std::string const& path)
    : db(nullptr), insertStmt(nullptr)
{
    int rc = sqlite3_open(path.c_str(), &db);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to open SQLite DB: " << sqlite3_errmsg(db) << "\n";
    }

    const char* createSql =
        "CREATE TABLE IF NOT EXISTS Snapshots ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "  timestamp INTEGER, "
        "  tripId TEXT, "
        "  routeId TEXT, "
        "  trainId TEXT, "
        "  direction INTEGER, "
        "  isAssigned INTEGER, "
        "  stopId TEXT, "
        "  currentStatus INTEGER, "
        "  delay INTEGER"
        ");"
        "CREATE TABLE IF NOT EXISTS StationMetrics ("
        "  stationId TEXT, "
        "  date TEXT, "
        "  totalStalls INTEGER, "
        "  avgDwellTime REAL, "
        "  maxDwellTime INTEGER, "
        "  PRIMARY KEY (stationId, date)"
        ");";

    char* errMsg = nullptr;
    rc = sqlite3_exec(db, createSql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to create tables: "
                  << (errMsg ? errMsg : "unknown error") << "\n";
        if (errMsg) sqlite3_free(errMsg);
    }

    const char* insertSql =
        "INSERT INTO Snapshots "
        "(timestamp, tripId, routeId, trainId, direction, isAssigned, stopId, currentStatus, delay) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

    rc = sqlite3_prepare_v2(db, insertSql, -1, &insertStmt, nullptr);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to prepare insert statement: "
                  << sqlite3_errmsg(db) << "\n";
        insertStmt = nullptr;
    }
}

SQLiteStore::~SQLiteStore()
{
    if (insertStmt) sqlite3_finalize(insertStmt);
    if (db) sqlite3_close(db);
}

void SQLiteStore::insert(const TrainSnapshot& s)
{
    std::lock_guard<std::mutex> lock(mutex);
    insertInternal(s);
}

void SQLiteStore::insertInternal(TrainSnapshot const& s)
{
    if (!insertStmt)
        return; 

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

    int rc = sqlite3_step(insertStmt);
    if (rc != SQLITE_DONE)
    {
        std::cerr << "SQLite insert failed: " << sqlite3_errmsg(db) << "\n";
    }
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
        "  S.tripId, S.routeId, S.trainId, S.direction, S.stopId, "
        "  MAX(S.timestamp) - MIN(S.timestamp) as dwellTimeSeconds, "
        "  MAX(S.delay) as reportedDelay, "
        "  SCH.arrival_sec "
        "FROM Snapshots S "
        "LEFT JOIN StaticSchedule SCH ON SCH.trip_id LIKE '%' || S.tripId || '%' " 
        "  AND S.stopId = SCH.stop_id " 
        "WHERE S.currentStatus = 1 "
        "AND S.stopId NOT LIKE '701%' "
        "AND S.stopId NOT LIKE 'D43%' "
        "AND S.stopId NOT LIKE 'G05%' "
        "AND S.stopId NOT LIKE '207%' "
        "AND S.stopId NOT LIKE 'A65%' "
        "AND S.timestamp > (strftime('%s','now') - 1800) " 
        "GROUP BY S.tripId, S.stopId "
        "HAVING dwellTimeSeconds > 60 " 
        "AND MAX(S.timestamp) > (strftime('%s','now') - 60) " 
        "ORDER BY dwellTimeSeconds DESC;";

    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to prepare getRecentStalls: "
                  << sqlite3_errmsg(db) << "\n";
        return results;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        TrainSnapshot s;

        const unsigned char* tripId  = sqlite3_column_text(stmt, 0);
        const unsigned char* routeId = sqlite3_column_text(stmt, 1);
        const unsigned char* trainId = sqlite3_column_text(stmt, 2);
        const unsigned char* stopId  = sqlite3_column_text(stmt, 4);

        s.tripId  = tripId  ? reinterpret_cast<const char*>(tripId)  : "";
        s.routeId = routeId ? reinterpret_cast<const char*>(routeId) : "";
        s.trainId = trainId ? reinterpret_cast<const char*>(trainId) : "";
        s.direction = sqlite3_column_int(stmt, 3);
        s.stopId    = stopId ? reinterpret_cast<const char*>(stopId) : "";

        s.dwellTimeSeconds = sqlite3_column_int(stmt, 5);
        s.delay            = sqlite3_column_int(stmt, 6);

        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL)
            s.scheduledArrivalSec = sqlite3_column_int(stmt, 7);
        else
            s.scheduledArrivalSec = 0;  

        s.currentStatus = 1;

        results.push_back(std::move(s));
    }

    if (rc != SQLITE_DONE)
    {
        std::cerr << "Error stepping getRecentStalls: "
                  << sqlite3_errmsg(db) << "\n";
    }

    sqlite3_finalize(stmt);
    return results;
}


void SQLiteStore::pruneOldData(int daysToKeep)
{
    std::lock_guard<std::mutex> lock(mutex);

    const long long cutoffSeconds   = static_cast<long long>(daysToKeep) * 86400LL;
    const long long cutoffTimestamp = std::time(nullptr) - cutoffSeconds;

    const char* compressSql =
        "WITH per_stall AS ("
        "  SELECT "
        "    stopId AS stationId, "
        "    date(timestamp, 'unixepoch') AS d, "
        "    (MAX(timestamp) - MIN(timestamp)) AS dwell "
        "  FROM Snapshots "
        "  WHERE timestamp < ? AND currentStatus = 1 "
        "  GROUP BY tripId, stopId, date(timestamp, 'unixepoch') "
        "  HAVING (MAX(timestamp) - MIN(timestamp)) > 60"
        ") "
        "INSERT OR REPLACE INTO StationMetrics "
        "  (stationId, date, totalStalls, avgDwellTime, maxDwellTime) "
        "SELECT "
        "  stationId, "
        "  d AS date, "
        "  COUNT(*) AS totalStalls, "
        "  AVG(dwell) AS avgDwellTime, "
        "  MAX(dwell) AS maxDwellTime "
        "FROM per_stall "
        "GROUP BY stationId, d;";

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, compressSql, -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(cutoffTimestamp));
            int rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE)
            {
                std::cerr << "StationMetrics compression failed: "
                          << sqlite3_errmsg(db) << "\n";
            }
            sqlite3_finalize(stmt);
        }
        else
        {
            std::cerr << "Failed to prepare compressSql: "
                      << sqlite3_errmsg(db) << "\n";
        }
    }

    {
        const char* deleteSql = "DELETE FROM Snapshots WHERE timestamp < ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, deleteSql, -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(cutoffTimestamp));
            int rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE)
            {
                std::cerr << "Prune delete failed: "
                          << sqlite3_errmsg(db) << "\n";
            }
            sqlite3_finalize(stmt);
        }
        else
        {
            std::cerr << "Failed to prepare deleteSql: "
                      << sqlite3_errmsg(db) << "\n";
        }
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}


void SQLiteStore::importStaticSchedule(std::string const& csvPath) {
    std::lock_guard<std::mutex> lock(mutex);

    sqlite3_stmt* checkStmt;
    sqlite3_prepare_v2(db, "SELECT count(*) FROM StaticSchedule", -1, &checkStmt, nullptr);
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        int count = sqlite3_column_int(checkStmt, 0);
        sqlite3_finalize(checkStmt);
        if (count > 0) {
            std::cout << "[System] Static Schedule already loaded (" << count << " rows)." << std::endl;
            return; 
        }
    } else {
        sqlite3_finalize(checkStmt);
    }

    std::cout << "[System] Importing " << csvPath << " (This may take a minute)..." << std::endl;

    const char* createSql = 
        "CREATE TABLE IF NOT EXISTS StaticSchedule ("
        "trip_id TEXT, "
        "stop_id TEXT, "
        "arrival_sec INTEGER, "
        "PRIMARY KEY (trip_id, stop_id));";
    sqlite3_exec(db, createSql, nullptr, nullptr, nullptr);

    std::ifstream file(csvPath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open " << csvPath << std::endl;
        return;
    }

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    sqlite3_stmt* insertStmt;
    sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO StaticSchedule VALUES (?, ?, ?)", -1, &insertStmt, nullptr);

    std::string line;
    std::getline(file, line); 

    int count = 0;
    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string segment;
        std::vector<std::string> row;
        
        while (std::getline(ss, segment, ',')) {
            row.push_back(segment);
        }
        if (row.size() < 4) continue;

        std::string tripId = row[0];
        std::string stopId = row[1]; 
        std::string timeStr = row[2]; 


        int h = 0, m = 0, s = 0;
        if (sscanf(timeStr.c_str(), "%d:%d:%d", &h, &m, &s) == 3) {
            int totalSeconds = (h * 3600) + (m * 60) + s;

            sqlite3_bind_text(insertStmt, 1, tripId.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertStmt, 2, stopId.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insertStmt, 3, totalSeconds);
            
            sqlite3_step(insertStmt);
            sqlite3_reset(insertStmt);
            count++;
        }
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_finalize(insertStmt);
    
    std::cout << "[System] Import Complete. Loaded " << count << " scheduled stops." << std::endl;
}

int SQLiteStore::getScheduledTime(std::string const& tripId, std::string const& stopId) {

    std::string sql = "SELECT arrival_sec FROM StaticSchedule WHERE trip_id = ? AND stop_id = ?";
    sqlite3_stmt* stmt;
    int result = -1;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, tripId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, stopId.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return result;
}