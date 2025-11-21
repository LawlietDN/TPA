#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>

class MtaClient: public std::enable_shared_from_this<MtaClient>
{
private:
    boost::asio::io_context& ioContext;
    boost::asio::ssl::context& sslContext;
    std::string_view const apiKey;

public:
    MtaClient(boost::asio::io_context& ioc, boost::asio::ssl::context sslctx, std::string_view& const key)
    :   ioContext(ioc),
        sslContext(sslctx),
        apiKey(key)
        {}

};