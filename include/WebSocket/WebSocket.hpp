#ifndef WEBSOCKET_HPP
#define WEBSOCKET_HPP

/**
 * @file WebSocket.hpp
 * 
 * This module represent the declaration of the WebSocket::WebSocket Class.
 * 
 * © 2024 by Hatem Nabli
 */
#include <memory>

namespace WebSocket {
    class WebSocket
    {
    public:
        /* data */

        // LifeCycle managment
    public:
        ~WebSocket();
        WebSocket(const WebSocket&) = delete;
        WebSocket(WebSocket&&) = delete;
        WebSocket& operator=(const WebSocket&) = delete;
        WebSocket& operator=(WebSocket&&) = delete;
        
        // Public Methods
    public:
        /**
         * This is the default constructor.
         */
        WebSocket();

    private:
        /**
         * 
         */
        struct Impl
        /**
         * 
         */
        std::unique_ptr< Impl > _impl;
    };
}


#endif /* WEBSOCKET_HPP */