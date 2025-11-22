#include <string>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
class MtaClient
{
private:
    boost::asio::io_context& ioContext;
    boost::asio::ssl::context sslContext;
    std::string apiKey;
    void configureTlsStream(boost::beast::ssl_stream<boost::beast::tcp_stream>& stream);
    boost::asio::awaitable<boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>> resolve(boost::asio::ip::tcp::resolver& resolver, boost::beast::ssl_stream<boost::beast::tcp_stream>& stream);
    boost::asio::awaitable<void> connect(boost::asio::ip::tcp::resolver::results_type results, boost::beast::ssl_stream<boost::beast::tcp_stream>& stream);
    boost::beast::http::request<boost::beast::http::string_body> buildGetRequest() const;
    boost::asio::awaitable<void> sendRequest(boost::beast::ssl_stream<boost::beast::tcp_stream>& stream, boost::beast::http::request<boost::beast::http::string_body> const& request);
    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> readResponse(boost::beast::ssl_stream<boost::beast::tcp_stream>& stream);
    boost::asio::awaitable<void> shutdownStream(boost::beast::ssl_stream<boost::beast::tcp_stream>& stream);


public:
    MtaClient(boost::asio::io_context& ioc, std::string key);
    boost::asio::awaitable<std::string> fetch();
};
