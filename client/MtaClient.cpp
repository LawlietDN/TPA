#include <iostream>
#include "ConfigurationManager.hpp"
#include "MtaClient.hpp"

MtaClient::MtaClient(boost::asio::io_context& ioc, std::string key)
        : ioContext(ioc)
        , sslContext(boost::asio::ssl::context::tlsv12_client)
        , apiKey(std::move(key))
    {
        sslContext.set_options(
            boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::single_dh_use
        );

        sslContext.set_default_verify_paths();
        sslContext.set_verify_mode(boost::asio::ssl::verify_none);
    }


void MtaClient::configureTlsStream(boost::beast::ssl_stream<boost::beast::tcp_stream>& stream)
{
    if (!SSL_set_tlsext_host_name(stream.native_handle(), ConfigurationManager::MTA_HOST.c_str()))
    {
        throw boost::beast::system_error(boost::system::error_code(static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()),"Failed to set SNI");
    }
    stream.set_verify_callback(boost::asio::ssl::host_name_verification(ConfigurationManager::MTA_HOST));
}

boost::asio::awaitable<boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>> MtaClient::resolve(boost::asio::ip::tcp::resolver& resolver, boost::beast::ssl_stream<boost::beast::tcp_stream>& stream)
{
    boost::asio::ip::tcp::resolver::results_type results = co_await resolver.async_resolve(ConfigurationManager::MTA_HOST, ConfigurationManager::MTA_PORT,boost::asio::use_awaitable);
    co_return results;
}   

boost::asio::awaitable<void> MtaClient::connect(boost::asio::ip::tcp::resolver::results_type results, boost::beast::ssl_stream<boost::beast::tcp_stream>& stream)
{
    co_await boost::beast::get_lowest_layer(stream).async_connect(results, boost::asio::use_awaitable);

    co_await stream.async_handshake(boost::asio::ssl::stream_base::client, boost::asio::use_awaitable
    );  
    co_return;
}

boost::beast::http::request<boost::beast::http::string_body> MtaClient::buildGetRequest(std::string const& target) const
{
    boost::beast::http::request<boost::beast::http::string_body> request(boost::beast::http::verb::get, target, 11);
    request.set(boost::beast::http::field::host, ConfigurationManager::MTA_HOST);
    request.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    request.set("X-API-Key", this->apiKey);

    return request;

}


boost::asio::awaitable<void> MtaClient::sendRequest(boost::beast::ssl_stream<boost::beast::tcp_stream>& stream, boost::beast::http::request<boost::beast::http::string_body> const& request)
{
    co_await boost::beast::http::async_write(stream, request, boost::asio::use_awaitable);
}

boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> MtaClient::readResponse(boost::beast::ssl_stream<boost::beast::tcp_stream>& stream)
{
    boost::beast::http::response<boost::beast::http::string_body> response;
    boost::beast::flat_buffer buffer;
    co_await boost::beast::http::async_read(stream, buffer, response, boost::asio::use_awaitable);
    co_return response;
}   


boost::asio::awaitable<std::string> MtaClient::fetch(std::string target)
{
    auto executor = co_await boost::asio::this_coro::executor;
    boost::asio::ip::tcp::resolver resolver(ioContext);
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream(executor, sslContext);

    configureTlsStream(stream);
    boost::asio::ip::tcp::resolver::results_type results =  co_await resolve(resolver, stream);
    co_await connect(results, stream);
    boost::beast::http::request<boost::beast::http::string_body> request = buildGetRequest(target);
    co_await sendRequest(stream, request);
    boost::beast::http::response<boost::beast::http::string_body> response = co_await this-> readResponse(stream);
    co_return response.body();
}



boost::asio::awaitable<void> MtaClient::shutdownStream(boost::beast::ssl_stream<boost::beast::tcp_stream>& stream)
{
    boost::system::error_code ec;
    co_await stream.async_shutdown(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    co_return;
}
