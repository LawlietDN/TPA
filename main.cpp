#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include "ConfigurationManager.hpp"

int main() 
{
    try
    {
        ConfigurationManager config;
        std::cout << ConfigurationManager::MTA_PORT << '\n';
        std::cout << config.getAPIKey() << '\n';
    }
    catch (std::exception const& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
