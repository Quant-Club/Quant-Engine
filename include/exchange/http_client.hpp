#pragma once

#include <string>
#include <map>
#include <memory>
#include <future>
#include "common/logger.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/strand.hpp>

namespace quant_hub {
namespace exchange {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE
};

class HttpClient {
public:
    HttpClient(const std::string& host, const std::string& port, bool useSSL = true)
        : host_(host)
        , port_(port)
        , useSSL_(useSSL)
        , ioc_()
        , resolver_(net::make_strand(ioc_))
        , ssl_ctx_(ssl::context::tlsv12_client)
    {
        ssl_ctx_.set_verify_mode(ssl::verify_peer);
        ssl_ctx_.set_default_verify_paths();
    }

    template<typename RequestBody, typename ResponseBody>
    http::response<ResponseBody> request(
        HttpMethod method,
        const std::string& target,
        const RequestBody& body,
        const std::map<std::string, std::string>& headers = {})
    {
        try {
            auto const results = resolver_.resolve(host_, port_);

            if (useSSL_) {
                beast::ssl_stream<beast::tcp_stream> stream(ioc_, ssl_ctx_);
                
                if(!SSL_set_tlsext_host_name(stream.native_handle(), host_.c_str())) {
                    throw beast::system_error(
                        beast::error_code(
                            static_cast<int>(::ERR_get_error()),
                            net::error::get_ssl_category()),
                        "Failed to set SNI Hostname");
                }

                beast::get_lowest_layer(stream).connect(results);
                stream.handshake(ssl::stream_base::client);

                return performRequest<RequestBody, ResponseBody>(stream, method, target, body, headers);
            } else {
                beast::tcp_stream stream(ioc_);
                stream.connect(results);

                return performRequest<RequestBody, ResponseBody>(stream, method, target, body, headers);
            }
        }
        catch(std::exception const& e) {
            LOG_ERROR("HTTP request failed: ", e.what());
            throw;
        }
    }

private:
    template<typename Stream, typename RequestBody, typename ResponseBody>
    http::response<ResponseBody> performRequest(
        Stream& stream,
        HttpMethod method,
        const std::string& target,
        const RequestBody& body,
        const std::map<std::string, std::string>& headers)
    {
        http::request<http::string_body> req{
            methodToVerb(method),
            target,
            11  // HTTP/1.1
        };

        req.set(http::field::host, host_);
        req.set(http::field::user_agent, "QuantHub/1.0");
        
        for (const auto& [key, value] : headers) {
            req.set(key, value);
        }

        if constexpr (!std::is_void_v<RequestBody>) {
            req.body() = body;
            req.prepare_payload();
        }

        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<ResponseBody> res;
        http::read(stream, buffer, res);

        return res;
    }

    static http::verb methodToVerb(HttpMethod method) {
        switch (method) {
            case HttpMethod::GET:    return http::verb::get;
            case HttpMethod::POST:   return http::verb::post;
            case HttpMethod::PUT:    return http::verb::put;
            case HttpMethod::DELETE: return http::verb::delete_;
            default: throw std::runtime_error("Unsupported HTTP method");
        }
    }

    std::string host_;
    std::string port_;
    bool useSSL_;
    net::io_context ioc_;
    tcp::resolver resolver_;
    ssl::context ssl_ctx_;
};

} // namespace exchange
} // namespace quant_hub
