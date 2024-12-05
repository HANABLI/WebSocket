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

TEST(WebSocketTests, WebSocketTests_SendFragmentedText__Test) {
    WebSocket::WebSocket ws;
    const auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.SendText("Hello,", false);
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x01\x06Hello,", connection->webSocketOutput);
    connection->webSocketOutput.clear();
    ws.SendBinary("X", true);
    ASSERT_TRUE(connection->webSocketOutput.empty());
    ws.Ping();
    ASSERT_EQ(std::string("\x89\x00", 2), connection->webSocketOutput);
    connection->webSocketOutput.clear();
    ws.SendBinary("X", false);
    ASSERT_TRUE(connection->webSocketOutput.empty());
    ws.SendText(" ", false);
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ(std::string("\x00\x01 ", 3), connection->webSocketOutput);
    connection->webSocketOutput.clear();
    ws.SendText("World!", true);
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x80\x06World!", connection->webSocketOutput);
}

TEST(WebSocketTests, WebSocketTests_SendFragmentedBinary__Test) {
    WebSocket::WebSocket ws;
    const auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.SendBinary("Hello,", false);
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x02\x06Hello,", connection->webSocketOutput);
    connection->webSocketOutput.clear();
    ws.SendText("X", true);
    ASSERT_TRUE(connection->webSocketOutput.empty());
    ws.Ping();
    ASSERT_EQ(std::string("\x89\x00", 2), connection->webSocketOutput);
    connection->webSocketOutput.clear();
    ws.SendText("X", false);
    ASSERT_TRUE(connection->webSocketOutput.empty());
    ws.SendBinary(" ", false);
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ(std::string("\x00\x01 ", 3), connection->webSocketOutput);
    connection->webSocketOutput.clear();
    ws.SendBinary("World!", true);
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x80\x06World!", connection->webSocketOutput);
}

TEST(WebSocketTests, WebSocketTests_ReceivedFragmentedText__Test) {
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
    const std::vector< std::string > frames{
        std::string("\x01\06", 2) + "Hello,",
        std::string("\x00\x06", 2) + " World",
        "\x80\x01" "!"
    };
    for (const auto& frame: frames) {
        connection->dataReceivedDelegate({frame.begin(), frame.end()});
    }  
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ(
        (std::vector< std::string >{
            "Hello, World!"
        }),
        text 
    ); 
}

TEST(WebSocketTests, WebSocketTests_InitiateCloseNoStatusReturned__Test) {
    WebSocket::WebSocket ws;
    const auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](
            unsigned int code,
            const std::string& reason
        ){
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        }
    );
    ws.Close(1000, "Goodbye!");
    ASSERT_EQ("\x88\x0A\x03\xe8Goodbye!", connection->webSocketOutput);
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_FALSE(closeReceived);
    const std::string frame = "\x88\x80XXXX";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_TRUE(connection->brokenByWebSocket);
    ASSERT_TRUE(closeReceived);
    EXPECT_EQ(1005, codeReceived);
    EXPECT_EQ("", reasonReceived);
}

TEST(WebSocketTests, WebSocketTests_InitiateCloseStatusReturned__Test) {
    WebSocket::WebSocket ws;
    const auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](
            unsigned int code,
            const std::string& reason
        ){
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        }
    );
    ws.Close(1000, "Goodbye!");
    ASSERT_EQ("\x88\x0A\x03\xe8Goodbye!", connection->webSocketOutput);
    connection->webSocketOutput.clear();
    ws.SendText("tell me why ?");
    ws.SendBinary("Tell me why?");
    ws.Ping();
    ws.Pong();
    ws.Close(1000, "Goodbye One more time!");
    ASSERT_TRUE(connection->webSocketOutput.empty());
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_FALSE(closeReceived);
    std::string frame = "\x88\x85";
    const std::string unmaskedPayload = "\x03\xe8" "Bye";
    const char mask[4] = {0x12, 0x32, 0x31, 0x60};
    frame += std::string(mask, 4);
    for (size_t i = 0; i < unmaskedPayload.length(); ++i) {
        frame += unmaskedPayload[i] ^ mask[i % 4];
    }
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_TRUE(connection->brokenByWebSocket);
    ASSERT_TRUE(closeReceived);
    EXPECT_EQ(1000, codeReceived);
    EXPECT_EQ("Bye", reasonReceived);
}

TEST(WebSocketTests, WebSocketTests_ReceiveCloseFrame__Test) {
        WebSocket::WebSocket ws;
    const auto connection = std::make_shared< MockConnection >();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](
            unsigned int code,
            const std::string& reason
        ){
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        }
    );
    const std::string frame = "\x88\x80XXXX";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_TRUE(closeReceived);
    EXPECT_EQ(1005, codeReceived);
    EXPECT_EQ("", reasonReceived);
    ws.Ping();
    ASSERT_EQ(std::string("\x89\x00", 2), connection->webSocketOutput);
    connection->webSocketOutput.clear();
    ws.Close(1000, "Goodbye!");
    ASSERT_EQ("\x88\x0A\x03\xe8Goodbye!", connection->webSocketOutput);
    ASSERT_TRUE(connection->brokenByWebSocket);
}

