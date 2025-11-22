#include <string>

class ConfigurationManager
{
private:
    std::string apiKey;

public:
    ConfigurationManager();
    static inline const std::string MTA_HOST   = "api-endpoint.mta.info";
    static inline const std::string MTA_PORT   = "443";
    static inline const std::string MTA_TARGET = "/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-ace";
    [[nodiscard]] std::string getAPIKey() const noexcept;
};