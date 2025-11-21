#include <string>

class ConfigurationManager
{
private:
    std::string apiKey;

public:
    ConfigurationManager();
    static constexpr std::string_view MTA_HOST = "api-endpoint.mta.info";
    static constexpr std::string_view MTA_PORT = "443";
    static constexpr std::string_view MTA_TARGET = "/Dataservice/mtagtfs.svc/nyct/gtfs-ace";
    
    [[nodiscard]] std::string_view getAPIKey() const noexcept;
};