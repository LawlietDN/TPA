#include <sstream>
#include <ctime>
#include <date/tz.h>
#include "Types.hpp"
#include "StopManager.hpp"
#include "VirtualClock.hpp"
#include "Dashboard.hpp"

using namespace date;
using namespace std::chrono;

int Dashboard::computeNowSec()
{
    auto epochNow = VirtualClock::now();
    auto now = system_clock::from_time_t(epochNow);

    auto nyc = date::locate_zone("America/New_York");
    zoned_time nycTime{nyc, now};

    auto local = nycTime.get_local_time();
    auto day = date::floor<date::days>(local);
    date::hh_mm_ss tod{local - day};

    return tod.hours().count() * 3600
         + tod.minutes().count() * 60
         + tod.seconds().count();
}


std::string Dashboard::buildHtmlHead(std::size_t stalledCount)
{
    std::stringstream ss;

    ss << "<html><head><title>NYC Transit Monitor</title>"
       << "<style>"
       << "body { font-family: sans-serif; background: #1a1a1a; color: #ddd; padding: 20px; }"
       << "h1 { color: #e74c3c; border-bottom: 2px solid #444; padding-bottom: 10px; }"
       << "table { width: 100%; border-collapse: collapse; margin-top: 20px; }"
       << "th { text-align: left; background: #333; padding: 10px; border-bottom: 2px solid #555; }"
       << "td { padding: 10px; border-bottom: 1px solid #333; }"
       << "tr:hover { background: #2c2c2c; }"
       << ".alert { color: #e74c3c; font-weight: bold; }"
       << ".severity-low { border-left: 5px solid #f1c40f; }"
       << ".severity-high { border-left: 5px solid #e74c3c; }"
       << ".badge { background: #444; padding: 2px 5px; border-radius: 3px; "
                     "font-size: 0.8em; margin-right:5px;}"
       << "</style>"
       << "<meta charset='UTF-8'>"
       << "<meta http-equiv='refresh' content='30'>"
       << "</head><body>";

    ss << "<h1>Live Report</h1>";
    ss << "<p>Status: " << stalledCount
       << " trains holding > 60s.</p>";

    return ss.str();
}

std::string Dashboard::buildTableHeader()
{
    std::stringstream ss;
    ss << "<table><thead><tr>"
       << "<th>Ln</th>"
       << "<th>Train Identity</th>"
       << "<th>Dir</th>"
       << "<th>Location</th>"
       << "<th>Dwell Time</th>"
       << "<th>Status</th>"
       << "<th>MTA Reported</th>"
       << "<th>sLateness</th>"
       << "</tr></thead><tbody>";
    return ss.str();
}

std::string Dashboard::formatLateness(TrainSnapshot const& t, int nowSec)
{
    std::string noSchedule = "<span style='color:#777'>No Schedule</span>";
    if (t.scheduledArrivalSec <= 0)
        return noSchedule;

    int diffSeconds = nowSec - t.scheduledArrivalSec;

    if (diffSeconds < -43200) diffSeconds += 86400;
    if (diffSeconds > 43200) diffSeconds -= 86400;

    int diffMinutes = diffSeconds / 60;
    char schedBuf[16], nowBuf[16];

    int sH = t.scheduledArrivalSec / 3600;
    int sM = (t.scheduledArrivalSec % 3600) / 60;
    int sS = t.scheduledArrivalSec % 60;
    std::snprintf(schedBuf, sizeof(schedBuf),
                  "%02d:%02d:%02d", sH, sM, sS);

    int nH = nowSec / 3600;
    int nM = (nowSec % 3600) / 60;
    int nS = nowSec % 60;
    std::snprintf(nowBuf, sizeof(nowBuf),
                  "%02d:%02d:%02d", nH, nM, nS);

    std::string color = "#888";
    if (diffMinutes > 5)       color = "#e74c3c";
    else if (diffMinutes < -2) color = "#2ecc71";

    std::stringstream evidence;
    evidence << "<span style='color:" << color
             << "; font-weight:bold; font-size:1.2em'>"
             << (diffMinutes > 0 ? "+" : "") << diffMinutes << "m</span>"
             << "<div style='font-size:0.75em; color:#aaa; margin-top:4px;'>"
             << "Sched: " << schedBuf << "<br>"
             << "Now: " << nowBuf
             << "</div>";

    return evidence.str();
}

std::string Dashboard::buildRow(TrainSnapshot const& t, StopManager& stops, int nowSec)
{
    std::string severityClass =
        (t.dwellTimeSeconds > 300) ? "severity-high" : "severity-low";
    std::string alertText =
        (t.dwellTimeSeconds > 300) ? "LONG STALL" : "SLOW HOLD";
    std::string dirStr =
        (t.direction == 1) ? "N" :
        (t.direction == 3) ? "S" : "?";

    int mins = t.dwellTimeSeconds / 60;
    int secs = t.dwellTimeSeconds % 60;

    std::string latenessStr = formatLateness(t, nowSec);

    std::stringstream ss;
    ss << "<tr class='" << severityClass << "'>"
       << "<td><b style='font-size:1.2em'>" << t.routeId << "</b></td>"
       << "<td>" << t.trainId << "</td>"
       << "<td><span class='badge'>" << dirStr << "</span></td>"
       << "<td>" << stops.getName(t.stopId)
       << " <span style='color:#666; font-size:0.8em'>("
       << t.stopId << ")</span></td>"
       << "<td><b>" << mins << "m " << secs << "s</b></td>"
       << "<td class='" << (t.dwellTimeSeconds > 300 ? "alert" : "") << "'>"
       << alertText << "</td>"
       << "<td>" << (t.delay == 0 ? "0" : std::to_string(t.delay)) << "s</td>"
       << "<td>" << latenessStr << "</td>"
       << "</tr>";

    return ss.str();
}

std::string Dashboard::generate(std::vector<TrainSnapshot> const& stalledTrains, StopManager& stops)
{
    int nowSec = computeNowSec();

    std::stringstream ss;
    ss << buildHtmlHead(stalledTrains.size());
    ss << buildTableHeader();

    for (const auto& t : stalledTrains)
    {
        ss << buildRow(t, stops, nowSec);
    }

    ss << "</tbody></table></body></html>";

    return ss.str();
}
