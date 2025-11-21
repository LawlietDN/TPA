#include <cstdlib>
#include <iostream>
#include "../include/configurationmanager.hpp"

ConfigurationManager::ConfigurationManager()
{
    const char* envAPIKey = std::getenv("MTA_API_KEY");
    if(!envAPIKey)
    {
        throw std::runtime_error("MTA_API_KEY not set in environment."); 
    }
    apiKey = envAPIKey;
}


std::string_view ConfigurationManager::getAPIKey() const noexcept
{
    return apiKey;
}
