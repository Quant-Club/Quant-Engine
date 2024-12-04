#pragma once

#include <string>
#include <functional>
#include <memory>
#include <queue>
#include <thread>
#include "common/logger.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>

namespace quant_hub {
namespace exchange {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

class WebSocketClient {
public:
    using MessageHandler = std::function<void(const std::string&)>;
    using ErrorHandler = std::function<void(const std::string&)>;
    
    WebSocketClient(const std::string& host, 
                   const std::string& port,
                   const std::string& target,
                   bool useSSL = true)
        : host_(host)
        , port_(port)
        , target_(target)
        , useSSL_(useSSL)
        , ioc_()
        , resolver_(net::make_strand(ioc_))
        , ssl_ctx_(ssl::context::tlsv12_client)
        , ws_(net::make_strand(ioc_))
        , ssl_ws_(net::make_strand(ioc_), ssl_ctx_)
        , running_(false)
    {
        ssl_ctx_.set_verify_mode(ssl::verify_peer);
        ssl_ctx_.set_default_verify_paths();
    }

    ~WebSocketClient() {
        stop();
    }

    void connect(const MessageHandler& onMessage,
                const ErrorHandler& onError = nullptr,
                const std::map<std::string, std::string>& headers = {}) 
    {
        messageHandler_ = onMessage;
        errorHandler_ = onError;

        try {
            auto const results = resolver_.resolve(host_, port_);

            if (useSSL_) {
                if(!SSL_set_tlsext_host_name(ssl_ws_.next_layer().native_handle(), 
                                           host_.c_str())) {
                    throw beast::system_error(
                        beast::error_code(
                            static_cast<int>(::ERR_get_error()),
                            net::error::get_ssl_category()),
                        "Failed to set SNI Hostname");
                }

                beast::get_lowest_layer(ssl_ws_).connect(results);
                ssl_ws_.next_layer().handshake(ssl::stream_base::client);
                
                ssl_ws_.handshake(host_, target_);
                
                for (const auto& [key, value] : headers) {
                    ssl_ws_.set_option(websocket::stream_base::decorator(
                        [key, value](websocket::request_type& req) {
                            req.set(key, value);
                        }));
                }

                running_ = true;
                readLoop(true);
            } else {
                beast::get_lowest_layer(ws_).connect(results);
                ws_.handshake(host_, target_);
                
                for (const auto& [key, value] : headers) {
                    ws_.set_option(websocket::stream_base::decorator(
                        [key, value](websocket::request_type& req) {
                            req.set(key, value);
                        }));
                }

                running_ = true;
                readLoop(false);
            }

            ioThread_ = std::thread([this]() { ioc_.run(); });
        }
        catch(std::exception const& e) {
            LOG_ERROR("WebSocket connection failed: ", e.what());
            if (errorHandler_) {
                errorHandler_(e.what());
            }
            throw;
        }
    }

    void send(const std::string& message) {
        if (!running_) return;

        net::post(ioc_, [this, message]() {
            bool write_in_progress = !writeQueue_.empty();
            writeQueue_.push(message);
            
            if (!write_in_progress) {
                if (useSSL_) {
                    doWrite(true);
                } else {
                    doWrite(false);
                }
            }
        });
    }

    void stop() {
        if (!running_) return;
        running_ = false;

        if (useSSL_) {
            ssl_ws_.close(websocket::close_code::normal);
        } else {
            ws_.close(websocket::close_code::normal);
        }

        ioc_.stop();
        if (ioThread_.joinable()) {
            ioThread_.join();
        }
    }

private:
    void readLoop(bool useSSL) {
        auto buffer = std::make_shared<beast::flat_buffer>();
        
        auto readHandler = [this, buffer, useSSL]
            (beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                if (errorHandler_) {
                    errorHandler_(ec.message());
                }
                return;
            }

            std::string message = beast::buffers_to_string(buffer->data());
            if (messageHandler_) {
                messageHandler_(message);
            }

            buffer->clear();
            if (running_) {
                if (useSSL) {
                    ssl_ws_.async_read(*buffer,
                        beast::bind_front_handler(&WebSocketClient::readLoop,
                                                shared_from_this(),
                                                useSSL));
                } else {
                    ws_.async_read(*buffer,
                        beast::bind_front_handler(&WebSocketClient::readLoop,
                                                shared_from_this(),
                                                useSSL));
                }
            }
        };

        if (useSSL) {
            ssl_ws_.async_read(*buffer, readHandler);
        } else {
            ws_.async_read(*buffer, readHandler);
        }
    }

    void doWrite(bool useSSL) {
        auto message = std::make_shared<std::string>(std::move(writeQueue_.front()));
        writeQueue_.pop();

        auto writeHandler = [this, useSSL]
            (beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                if (errorHandler_) {
                    errorHandler_(ec.message());
                }
                return;
            }

            if (!writeQueue_.empty()) {
                doWrite(useSSL);
            }
        };

        if (useSSL) {
            ssl_ws_.async_write(net::buffer(*message), writeHandler);
        } else {
            ws_.async_write(net::buffer(*message), writeHandler);
        }
    }

    std::string host_;
    std::string port_;
    std::string target_;
    bool useSSL_;
    
    net::io_context ioc_;
    tcp::resolver resolver_;
    ssl::context ssl_ctx_;
    
    websocket::stream<beast::tcp_stream> ws_;
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ssl_ws_;
    
    std::atomic<bool> running_;
    std::thread ioThread_;
    
    std::queue<std::string> writeQueue_;
    MessageHandler messageHandler_;
    ErrorHandler errorHandler_;
};

} // namespace exchange
} // namespace quant_hub
