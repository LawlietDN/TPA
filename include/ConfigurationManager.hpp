#include <string>
#include <vector>

struct FeedEndpoint
{
    std::string name;
    std::string url;
    // Future: ProtocolType type; (PROTOBUF vs JSON)
};


class ConfigurationManager
{
private:
    std::string apiKey;
    std::vector<FeedEndpoint> activeFeeds;
    static inline const std::vector<std::string> FEED_URLS = {
        "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-ace",  // A, C, E, H
        "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs",      // 1, 2, 3, 4, 5, 6, S
        "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-nqrw", // N, Q, R, W
        "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-bdfm", // B, D, F, M
        "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-l",    // L
        "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-sir",  // Staten Island
        "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-g",    // G
        "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-jz"    // J, Z
    };


public:
    ConfigurationManager();
    static inline const std::string MTA_HOST   = "api-endpoint.mta.info";
    static inline const std::string MTA_PORT   = "443";

    [[nodiscard]] std::string getAPIKey() const noexcept;
    [[nodiscard]] std::vector<FeedEndpoint> const& getFeeds() const noexcept;
};