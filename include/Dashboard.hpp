#pragma once
#include <string>
#include <sstream>
#include <vector>
#include "Types.hpp"
#include "StopManager.hpp" 

class Dashboard
{
public:
    static std::string generate(std::vector<TrainSnapshot> const& stalledTrains, StopManager& stops)
    {
        std::stringstream ss;
        ss << "<html><head><title>NYC Transit Monitor</title>"
           << "<style>"
           << "body { font-family: sans-serif; background: #1a1a1a; color: #ddd; padding: 20px; }"
           << "h1 { color: #e74c3c; border-bottom: 2px solid #444; padding-bottom: 10px; }"
           << "table { width: 100%; border-collapse: collapse; margin-top: 20px; }"
           << "th { text-align: left; background: #333; padding: 10px; }"
           << "td { padding: 10px; border-bottom: 1px solid #333; }"
           << "tr:hover { background: #2c2c2c; }"
           << ".alert { color: #e74c3c; font-weight: bold; }"
           << "</style>"
           << "<meta http-equiv='refresh' content='30'>"
           << "</head><body>";

        ss << "<style>"
           << ".severity-low { border-left: 5px solid #f1c40f; }" 
           << ".severity-high { border-left: 5px solid #e74c3c; }"
           << ".badge { background: #444; padding: 2px 5px; border-radius: 3px; font-size: 0.8em; margin-right:5px;}"
           << "</style>";

        ss << "</head><body>";
        ss << "<h1>Live Bottleneck Report</h1>";
        ss << "<p>System Status: " << stalledTrains.size() << " trains holding > 60s.</p>";

        ss << "<table><thead><tr>"
           << "<th>Ln</th><th>Train Identity</th><th>Dir</th><th>Location</th><th>Dwell Time</th><th>Status</th>"
           << "</tr></thead><tbody>";

        for (const auto& t : stalledTrains)
        {
            std::string severityClass = (t.dwellTimeSeconds > 300) ? "severity-high" : "severity-low";
            std::string alertText = (t.dwellTimeSeconds > 300) ? "CRITICAL STALL" : "SLOW HOLD";
            std::string dirStr = (t.direction == 1) ? "N" : (t.direction == 3) ? "S" : "?";

            int mins = t.dwellTimeSeconds / 60;
            int secs = t.dwellTimeSeconds % 60;

        ss << "<tr class='" << severityClass << "'>"
            << "<td><b style='font-size:1.2em'>" << t.routeId << "</b></td>"
            << "<td>" << t.trainId << "</td>"
            << "<td><span class='badge'>" << dirStr << "</span></td>"
            << "<td>" << stops.getName(t.stopId)
            << " <span style='color:#888; font-size:0.8em'>(" << t.stopId << ")</span></td>"
            << "<td><b>" << mins << "m " << secs << "s</b></td>"
            << "<td class='" << (t.dwellTimeSeconds > 300 ? "alert" : "") << "'>" 
            << alertText << "</td>"
            << "<td>" << t.delay << "s</td>"
            << "</tr>";

        }

        ss << "</tbody></table></body></html>";

        return ss.str();
    }
};