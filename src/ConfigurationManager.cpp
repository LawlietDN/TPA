#include <cstdlib>
#include <stdexcept>
#include "ConfigurationManager.hpp"

ConfigurationManager::ConfigurationManager()
{
    const char* envAPIKey = std::getenv("MTA_API_KEY");
    if(!envAPIKey) throw std::runtime_error("MTA_API_KEY not set.");
    apiKey = envAPIKey;

    activeFeeds = {
        {"Lines 1-7",   "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs"},
        {"Lines A-E",   "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-ace"},
        {"Lines N-W",   "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-nqrw"},
        {"Lines B-M",   "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-bdfm"},
        {"Line L",      "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-l"},
        {"Line G",      "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-g"},
        {"Lines J/Z",   "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-jz"},
        {"Staten Isl",  "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-sir"}
    };
}

std::string ConfigurationManager::getAPIKey() const noexcept { return apiKey; }
const std::vector<FeedEndpoint>& ConfigurationManager::getFeeds() const noexcept { return activeFeeds; }