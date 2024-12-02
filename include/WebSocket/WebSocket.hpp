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
#include <functional>
#include <Http/Connection.hpp>

namespace WebSocket {
    class WebSocket
    {
        /* types */
    public:
        /**
         * This identifies which role the local endpoint originally
         * had when the WebSocket was established.
         */
        enum class Role {
            /**
             * In this role, the data for message sent must be masked,
             * and all messages received must not be masked.
             */
            Client,

            /**
             * In this role, the data for messages sent must not be masked,
             * and all messages received must be masked.
             */
            Server
        };

        /**
         * This is the type of function used to publish messages received
         * by the WebSocket.
         * 
         * @param[in] data
         *      This is the payload data of the received message
         */
        typedef std::function< void(const std::string& data) > MessageReceivedDelegate;

    public:
        /* data */

        // LifeCycle managment
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

        /**
         * This method open the WebSocket connection in a given role.
         * 
         * @param[in] connection
         *      This is the connection to use to send and receive frames.
         * 
         * @param[in] role
         *      This is the role throughout the WebSocket in the connection.
         */
        void Open(
            std::shared_ptr< Http::Connection > connection,
            Role role
        );
        /**
         * This method sends a ping message over the WebSocket.
         * 
         * @param[in] data
         *      This is the optional payload to include with the ping message
         */
        void Ping(const std::string& data = "");
        /**
         * This method sends a pong message over the WebSocket.
         * 
         * @param[in] data
         *      This is the optional payload to include with the pong message
         */
        void Pong(const std::string& data = "");
        /**
         * This method sets the function to call whanever a ping message
         * is received from the websocket.
         * 
         * @param[in] pingDelegate
         *      This is the function to call whenever a ping message
         *      is received from the websocket.
         *      
         */
        void SetPingDelegate(MessageReceivedDelegate pingDelegate);
        /**
         * This method sets the function to call whanever a pong message
         * is received from the websocket
         * 
         * @param[in] pongDelegate
         *      This is the function to call whenever a pong message
         *      is received from the websocket.
         */
        void SetPongDelegate(MessageReceivedDelegate pongDelegate);
        /**
         * This method sets the function to call whanever a text message
         * is received from the websocket
         * 
         * @param[in] textDelegate
         *      This is the function to call whenever a text message
         *      is received from the websocket.
         */
        void SetTextDelegate(MessageReceivedDelegate textDelegate);
        /**
         * This method sets the function to call whanever a text message
         * is received from the websocket
         * 
         * @param[in] textDelegate
         *      This is the function to call whenever a text message
         *      is received from the websocket.
         */
        void SetBinaryDelegate(MessageReceivedDelegate binaryDelegate);
        /**
         * This method send a text message over the webSocket.
         * 
         * @param[in] text
         *      This is the text message to send over the webSocket
         */
        void SendText(const std::string& text);
        /**
         * This method send a binary message over the webSocket.
         * 
         * @param[in] binary
         *      This is the binary message to send over the webSocket
         */
        void SendBinary(const std::string& binary);
    private:
        /**
         * 
         */
        struct Impl;
        /**
         * 
         */
        std::unique_ptr< Impl > impl_;
    };
}


#endif /* WEBSOCKET_HPP */