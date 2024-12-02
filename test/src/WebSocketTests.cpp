/**
 * @file WebSocketTests
 * 
 * This module contains tests for the WebSocket::WebSocket class.
 * 
 * © 2024 by Hatem Nabli
 */
#include <vector>
#include <string>
#include <gtest/gtest.h>
#include <Http/Connection.hpp>
#include <WebSocket/WebSocket.hpp>


namespace {

    /**
     * This is a fake client connection used to test the WebSocket.
     */
    struct MockConnection : public Http::Connection
    {
        // Properies

        /**
         * This indicates whether or not the mock connection is received
         * from the remote peer.
         */
        bool callingDelegate =  false;

        /**
         * This is the delegate to call in order to simulate data coming into 
         * the WebSocket from the remote peer.
         */
        DataReceivedDelegate dataReceivedDelegate;

        /**
         * This is the delegate to call whenever connection has been broken.
         */
        BrokenDelegate brokenDelegate;

        // /**
        //  * This holds onto a copy of all data sent by the WebSocket
        //  * to the remote peer.
        //  */
        // std::vector< uint8_t > dataReceived;

        /**
         * This holds out a copy of all data sent by the WebSocket
         * to the remote peer.
         */
        std::string webSocketOutput;

        /**
         * This flag is set if the remote close the connection
         */
        bool brokenByWebSocket = false;


        // Lifecycle management
        ~MockConnection() {
        }

        MockConnection(const MockConnection&) = delete;
        MockConnection(MockConnection&&) = delete;
        MockConnection& operator=(const MockConnection&) = delete;
        MockConnection& operator=(MockConnection&&) = delete;

        // Methods

        MockConnection() = default;


        // Http::Connection

        virtual std::string GetPeerId() override {
            return "mock-client";
        }

        virtual void SetDataReceivedDelegate(DataReceivedDelegate newDataReceivedDelegate) override {
            dataReceivedDelegate = newDataReceivedDelegate;
        }

        virtual void SetConnectionBrokenDelegate(BrokenDelegate newBrokenDelegate) override {
            brokenDelegate = newBrokenDelegate;
        }

        virtual void SendData(const std::vector< uint8_t >& data) override {
            (void)webSocketOutput.insert(
                webSocketOutput.end(),
                data.begin(),
                data.end()
            );
        }

        virtual void Break(bool clean) override {
            brokenByWebSocket = true;
        }
    };
}

TEST(WebSocketTests, WebSocketTests_InitiateOpen__Test) {
    //TODO 
}

TEST(WebSocketTests, WebSocketTests_CompleteOpen__Test) {
    //TODO 
}

TEST(WebSocketTests, WebSocketTests_SendPingNormaly__Test) {
    WebSocket::WebSocket ws;
    auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.Ping("Hello");   
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x89\x05Hello", connection->webSocketOutput);
}

TEST(WebSocketTests, WebSocketTests_SendPingTooMuchData__Test) {
    WebSocket::WebSocket ws;
    auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.Ping(std::string(126, 'x'));   
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("", connection->webSocketOutput);
}

TEST(WebSocketTests, WebSocketTests_SendPingMaxAllowedData__Test) {
    WebSocket::WebSocket ws;
    auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.Ping(std::string(125, 'x'));   
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x89\x7D" + std::string(125, 'x'), connection->webSocketOutput);
}

TEST(WebSocketTests, WebSocketTests_ReceivePing__Test) {
    WebSocket::WebSocket ws;
    auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    std::vector< std::string > pings;
    ws.SetPingDelegate(
        [&pings](
            const std::string& data
        )
        {
            pings.push_back(data);
        }
    );

    const std::string frame = "\x89\x06World!";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ(
        (std::vector< std::string >{
            "World!"
        }),
        pings
    );
    ASSERT_LE(12, connection->webSocketOutput.length());
    ASSERT_EQ("\x8A\x86", connection->webSocketOutput.substr(0, 2));
    for (size_t i = 0; i < 6; ++i) {
        ASSERT_EQ(
            frame[2 + i] ^ connection->webSocketOutput[2 + (i % 4)],
            connection->webSocketOutput[6 + i]
        );
    }
}

TEST(WebSocketTests, WebSocketTests_SendPongNormaly__Test) {
    WebSocket::WebSocket ws;
    auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.Pong("Hello");   
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x8A\x05Hello", connection->webSocketOutput);
}

TEST(WebSocketTests, WebSocketTests_SendPongTooMuchData__Test) {
    WebSocket::WebSocket ws;
    auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.Pong(std::string(126, 'x'));   
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("", connection->webSocketOutput);
}

TEST(WebSocketTests, WebSocketTests_SendPongMaxAllowedData__Test) {
    WebSocket::WebSocket ws;
    auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.Pong(std::string(125, 'x'));   
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x8A\x7D" + std::string(125, 'x'), connection->webSocketOutput);
}

TEST(WebSocketTests, WebSocketTests_ReceivePong__Test) {
    WebSocket::WebSocket ws;
    auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    std::vector< std::string > pongs;
    ws.SetPongDelegate(
        [&pongs](
            const std::string& data
        )
        {
            pongs.push_back(data);
        }
    );
    const std::string frame = "\x8A\x06World!";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ(
        (std::vector< std::string >{
            "World!",
        }),
        pongs
    );
}

TEST(WebSoketTests, WebSoketTests_SendText__Test) {
    WebSocket::WebSocket ws;
    const auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.SendText("Hello, World!");
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x81\x0DHello, World!", connection->webSocketOutput); 
}

TEST(WebSocketTests, WebSocketTests_ReceiveText__Test) {
    WebSocket::WebSocket ws;
    auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    std::vector< std::string > text;
    ws.SetTextDelegate(
        [&text](
            const std::string& data
        ){
            text.push_back(data);
        }
    );
    const std::string frame = "\x81\x0DHello, World!";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ(
        (std::vector< std::string >{
            "Hello, World!"
        }),
        text
    );
}

TEST(WebSoketTests, WebSoketTests_SendBinary__Test) {
    WebSocket::WebSocket ws;
    const auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.SendBinary("Hello, World!");
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x82\x0DHello, World!", connection->webSocketOutput); 
}

TEST(WebSocketTests, WebSocketTests_ReceiveBinary__Test) {
    WebSocket::WebSocket ws;
    auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    std::vector< std::string > binary;
    ws.SetBinaryDelegate(
        [&binary](
            const std::string& data
        ){
            binary.push_back(data);
        }
    );
    const std::string frame = "\x82\x0DHello, World!";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ(
        (std::vector< std::string >{
            "Hello, World!"
        }),
        binary
    );
}

TEST(WebSocketTests, WebSocketTests_SendMasked__Test) {
    WebSocket::WebSocket ws;
    const auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    const std::string data = "Hello, World!";
    ws.SendText(data);
    ASSERT_EQ(19, connection->webSocketOutput.length());
    ASSERT_EQ("\x81\x8D", connection->webSocketOutput.substr(0, 2));
    for (size_t i = 0; i < 13; ++i) {
        ASSERT_EQ(
            data[i] ^ connection->webSocketOutput[2 + (i % 4)],
            connection->webSocketOutput[6 + i]
        );
    } 
}

TEST(WebSocketTests, WebSocketTests_ReceiveMasked__Test) {
        WebSocket::WebSocket ws;
    auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    std::vector< std::string > text;
    ws.SetTextDelegate(
        [&text](
            const std::string& data
        ){
            text.push_back(data);
        }
    );
    const char maskingKey[4] = {0x12, 0X13, 0X14, 0X17};
    const std::string data = "Hello, world!";
    std::string frame = "\x81\x8D";
    frame += std::string(maskingKey, 4);
    for (size_t i = 0; i < data.length(); ++i) {
        frame += data[i] ^ maskingKey[i % 4];
    }
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ(
        (std::vector< std::string >{
            data
        }),
        text 
    ); 
}

TEST(WebSocketTests, WebSocketTests_SendFragmented__Test) {
    //TODO 
}

TEST(WebSocketTests, WebSocketTests_ReceiveFragmented__Test) {
    //TODO 
}

TEST(WebSocketTests, WebSocketTests_InitiateClose__Test) {
    //TODO 
}

TEST(WebSocketTests, WebSocketTests_CompleteClose__Test) {
    //TODO 
}

