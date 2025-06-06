/**
 * @file WebSocketTests
 *
 * This module contains tests for the WebSocket::WebSocket class.
 *
 * © 2024 by Hatem Nabli
 */
#include <gtest/gtest.h>
#include <Base64/Base64.hpp>
#include <Http/Client.hpp>
#include <Http/Connection.hpp>
#include <Http/Server.hpp>
#include <Sha1/Sha1.hpp>
#include <StringUtils/StringUtils.hpp>
#include <Uri/Uri.hpp>
#include <WebSocket/WebSocket.hpp>
#include <string>
#include <vector>

namespace
{
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
        bool callingDelegate = false;

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
        ~MockConnection() {}

        MockConnection(const MockConnection&) = delete;
        MockConnection(MockConnection&&) = delete;
        MockConnection& operator=(const MockConnection&) = delete;
        MockConnection& operator=(MockConnection&&) = delete;

        // Methods

        MockConnection() = default;

        // Http::Connection

        virtual std::string GetPeerId() override { return "mock-client"; }

        virtual void SetDataReceivedDelegate(
            DataReceivedDelegate newDataReceivedDelegate) override {
            dataReceivedDelegate = newDataReceivedDelegate;
        }

        virtual void SetConnectionBrokenDelegate(BrokenDelegate newBrokenDelegate) override {
            brokenDelegate = newBrokenDelegate;
        }

        virtual void SendData(const std::vector<uint8_t>& data) override {
            (void)webSocketOutput.insert(webSocketOutput.end(), data.begin(), data.end());
        }

        virtual void Break(bool clean) override { brokenByWebSocket = true; }
    };
}  // namespace

struct WebSocketTests : public ::testing::Test
{
    // Properties
    /**
     * This is the websocket object under test.
     */
    WebSocket::WebSocket ws;
    /**
     * These are the diagnostic messages that have been received
     * from the unit under test.
     */
    std::vector<std::string> diagnosticMessages;
    /**
     * This is the delegate obtained when subscribing
     * to receive diagnostic messages from the unit under test.
     * It's called to terminate the subscription.
     */
    SystemUtils::DiagnosticsSender::UnsubscribeDelegate diagnosticsUnsubscribeDelegate;

    // Methods
    virtual void SetUp() {
        diagnosticsUnsubscribeDelegate = ws.SubscribeToDiagnostics(
            [this](std::string senderName, size_t level, std::string message)
            {
                diagnosticMessages.push_back(StringUtils::sprintf("%s[%zu]: %s", senderName.c_str(),
                                                                  level, message.c_str()));
            },
            0);
    }

    virtual void TearDown() { diagnosticsUnsubscribeDelegate(); }
};

TEST_F(WebSocketTests, WebSocketTests_InitiateOpenAsClient__Test) {
    Http::Server::Request request;
    Http::Client::Response response;
    ws.StartOpenAsClient(request);
    EXPECT_EQ("13", request.headers.GetHeaderValue("Sec-WebSocket-Version"));
    EXPECT_TRUE(request.headers.HasHeader("Sec-WebSocket-Key"));
    const auto key = request.headers.GetHeaderValue("Sec-WebSocket-Key");
    EXPECT_EQ(key, Base64::EncodeToBase64(Base64::DecodeFromBase64(key)));
    EXPECT_EQ("websocket", StringUtils::NormalizeCaseInsensitiveString(
                               request.headers.GetHeaderValue("Upgrade")));
    bool foundUpgradeToken = false;
    for (const auto token : request.headers.GetHeaderMultiValues("Connection"))
    {
        for (const auto tokenWithWiteSpace : StringUtils::Split(token, ','))
        {
            if (StringUtils::NormalizeCaseInsensitiveString(
                    StringUtils::Trim(tokenWithWiteSpace)) == "upgrade")
            { foundUpgradeToken = true; }
        }
    }
    EXPECT_TRUE(foundUpgradeToken);
}

TEST_F(WebSocketTests, WebSocketTests_ConnectionBreaksAbnormally__Test) {
    const auto connection = std::make_shared<MockConnection>();
    Http::Server::Request request;
    Http::Client::Response response;
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](unsigned int code,
                                                         const std::string& reason)
        {
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        });
    connection->brokenDelegate(true);
    ASSERT_TRUE(connection->brokenByWebSocket);
    ASSERT_TRUE(closeReceived);
    EXPECT_EQ(1006, codeReceived);
    EXPECT_EQ("connection broken by peer", reasonReceived);
}

TEST_F(WebSocketTests, WebSocketTests_CompleteOpenAsClient__Test) {
    Http::Server::Request request;
    Http::Client::Response response;
    ws.StartOpenAsClient(request);
    response.statusCode = 101;
    response.headers.SetHeader("Connection", "upgrade");
    response.headers.SetHeader("Upgrade", "websocket");
    response.headers.SetHeader(
        "Sec-WebSocket-Accept",
        Base64::EncodeToBase64(Sha1::Sha1Bytes(request.headers.GetHeaderValue("Sec-WebSocket-Key") +
                                               "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
    auto connection = std::make_shared<MockConnection>();
    ASSERT_TRUE(ws.CompleteOpenAsClient(connection, response));
    const std::string data = "Hello!";
    ws.Ping(data);
    ASSERT_EQ(6 + data.length(), connection->webSocketOutput.length());
    ASSERT_EQ("\x89\x86", connection->webSocketOutput.substr(0, 2));
    for (size_t i = 0; i < data.length(); ++i)
    {
        ASSERT_EQ(data[i] ^ connection->webSocketOutput[2 + (i % 4)],
                  connection->webSocketOutput[data.length() + i]);
    }
}

TEST_F(WebSocketTests, WebSocketTests_FailCompleteOpenAsClientDueToMissingUpgrade__Test) {
    Http::Server::Request request;
    Http::Client::Response response;
    ws.StartOpenAsClient(request);
    response.statusCode = 101;
    response.headers.SetHeader("Connection", "upgrade");
    response.headers.SetHeader(
        "Sec-WebSocket-Accept",
        Base64::EncodeToBase64(Sha1::Sha1(request.headers.GetHeaderValue("Sec-WebSocket-Key") +
                                          "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
    auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.CompleteOpenAsClient(connection, response));
}

TEST_F(WebSocketTests, WebSocketTests_FailCompleteOpenAsClientDueToWrongUpgrade__Test) {
    Http::Server::Request request;
    Http::Client::Response response;
    ws.StartOpenAsClient(request);
    response.statusCode = 101;
    response.headers.SetHeader("Connection", "upgrade");
    response.headers.SetHeader("Upgrade", "foobar");
    response.headers.SetHeader(
        "Sec-WebSocket-Accept",
        Base64::EncodeToBase64(Sha1::Sha1(request.headers.GetHeaderValue("Sec-WebSocket-Key") +
                                          "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
    auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.CompleteOpenAsClient(connection, response));
}

TEST_F(WebSocketTests, WebSocketTests_FailCompleteOpenAsClientDueToMissingConnection__Test) {
    Http::Server::Request request;
    Http::Client::Response response;
    ws.StartOpenAsClient(request);
    response.statusCode = 101;
    response.headers.SetHeader("Upgrade", "websocket");
    response.headers.SetHeader(
        "Sec-WebSocket-Accept",
        Base64::EncodeToBase64(Sha1::Sha1(request.headers.GetHeaderValue("Sec-WebSocket-Key") +
                                          "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
    auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.CompleteOpenAsClient(connection, response));
}

TEST_F(WebSocketTests, WebSocketTests_FailCompleteOpenAsClientDueToWrongConnection__Test) {
    Http::Server::Request request;
    Http::Client::Response response;
    ws.StartOpenAsClient(request);
    response.statusCode = 101;
    response.headers.SetHeader("Connection", "foobar");
    response.headers.SetHeader("Upgrade", "websocket");
    response.headers.SetHeader(
        "Sec-WebSocket-Accept",
        Base64::EncodeToBase64(Sha1::Sha1(request.headers.GetHeaderValue("Sec-WebSocket-Key") +
                                          "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
    auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.CompleteOpenAsClient(connection, response));
}

TEST_F(WebSocketTests, WebSocketTests_FailCompleteOpenAsClientDueToWrongAccept__Test) {
    Http::Server::Request request;
    Http::Client::Response response;
    ws.StartOpenAsClient(request);
    response.statusCode = 101;
    response.headers.SetHeader("Connection", "upgrade");
    response.headers.SetHeader("Upgrade", "websocket");
    response.headers.SetHeader(
        "Sec-WebSocket-Accept",
        Base64::EncodeToBase64(Sha1::Sha1(request.headers.GetHeaderValue("Sec-WebSocket-Key") +
                                          "258EAFA5-E914-47DA-95CB-C5AB0DC85B11")));
    auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.CompleteOpenAsClient(connection, response));
}

TEST_F(WebSocketTests, WebSocketTests_FailCompleteOpenAsClientDueToMissingAccept__Test) {
    Http::Server::Request request;
    Http::Client::Response response;
    ws.StartOpenAsClient(request);
    response.statusCode = 101;
    response.headers.SetHeader("Connection", "upgrade");
    response.headers.SetHeader("Upgrade", "websocket");
    auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.CompleteOpenAsClient(connection, response));
}

TEST_F(WebSocketTests, WebSocketTests_FailCompleteOpenAsClientDueToBlancExtension__Test) {
    Http::Server::Request request;
    Http::Client::Response response;
    ws.StartOpenAsClient(request);
    response.statusCode = 101;
    response.headers.SetHeader("Connection", "upgrade");
    response.headers.SetHeader("Upgrade", "websocket");
    response.headers.SetHeader(
        "Sec-WebSocket-Accept",
        Base64::EncodeToBase64(Sha1::Sha1Bytes(request.headers.GetHeaderValue("Sec-WebSocket-Key") +
                                               "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
    response.headers.SetHeader("Sec-WebSocket-Extension", "");
    auto connection = std::make_shared<MockConnection>();
    ASSERT_TRUE(ws.CompleteOpenAsClient(connection, response));
}

TEST_F(WebSocketTests, WebSocketTests_FailCompleteOpenAsClientDueToBlancProtocol__Test) {
    Http::Server::Request request;
    Http::Client::Response response;
    ws.StartOpenAsClient(request);
    response.statusCode = 101;
    response.headers.SetHeader("Connection", "upgrade");
    response.headers.SetHeader("Upgrade", "websocket");
    response.headers.SetHeader(
        "Sec-WebSocket-Accept",
        Base64::EncodeToBase64(Sha1::Sha1Bytes(request.headers.GetHeaderValue("Sec-WebSocket-Key") +
                                               "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
    response.headers.SetHeader("Sec-WebSocket-Protocol", "");
    auto connection = std::make_shared<MockConnection>();
    ASSERT_TRUE(ws.CompleteOpenAsClient(connection, response));
}

TEST_F(WebSocketTests, WebSocketTests_CompleteOpenAsServer__Test) {
    Http::Server::Request request;
    request.headers.SetHeader("Sec-WebSocket-Version", "13");
    request.headers.SetHeader("Connection", "upgrade");
    request.headers.SetHeader("Upgrade", "websocket");
    const std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
    request.headers.SetHeader("Sec-webSocket-key", key);
    Http::Client::Response response;
    auto connection = std::make_shared<MockConnection>();
    ASSERT_TRUE(ws.OpenAsServer(connection, request, response, ""));
    EXPECT_EQ(101, response.statusCode);
    EXPECT_EQ("Switching Protocols", response.status);
    EXPECT_EQ("websocket", StringUtils::NormalizeCaseInsensitiveString(
                               response.headers.GetHeaderValue("Upgrade")));
    bool foundUpgradeToken = false;
    for (const auto token : response.headers.GetHeaderTokens("Connection"))
    {
        if (token == "upgrade")
        {
            foundUpgradeToken = true;
            break;
        }
    }
    EXPECT_TRUE(foundUpgradeToken);
    EXPECT_EQ(response.headers.GetHeaderValue("Sec-WebSocket-Accept"),
              "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    ws.Ping("Hello");
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x89\x05Hello", connection->webSocketOutput);
}

TEST_F(WebSocketTests, WebSocketTests_CompleteOpenAsServerWithTrailer__Test) {
    Http::Server::Request request;
    request.headers.SetHeader("Sec-WebSocket-Version", "13");
    request.headers.SetHeader("Connection", "upgrade");
    request.headers.SetHeader("Upgrade", "websocket");
    const std::string key = Base64::EncodeToBase64("abcdefghijklmnop");
    request.headers.SetHeader("Sec-webSocket-key", key);
    Http::Client::Response response;
    auto connection = std::make_shared<MockConnection>();
    std::vector<std::string> pongs;
    ws.SetPongDelegate([&pongs](const std::string& data) { pongs.push_back(data); });
    ASSERT_TRUE(ws.OpenAsServer(connection, request, response, "\x8A"));
    EXPECT_TRUE(pongs.empty());
    connection->dataReceivedDelegate({0x80, 0x12, 0x34, 0x56, 0X76});
    ASSERT_EQ((std::vector<std::string>{
                  "",
              }),
              pongs);
}

TEST_F(WebSocketTests, WebSocketTests_SendPingNormaly__Test) {
    auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.Ping("Hello");
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x89\x05Hello", connection->webSocketOutput);
}

TEST_F(WebSocketTests, WebSocketTests_SendPingTooMuchData__Test) {
    auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.Ping(std::string(126, 'x'));
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("", connection->webSocketOutput);
}

TEST_F(WebSocketTests, WebSocketTests_SendPingMaxAllowedData__Test) {
    auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.Ping(std::string(125, 'x'));
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x89\x7D" + std::string(125, 'x'), connection->webSocketOutput);
}

TEST_F(WebSocketTests, WebSocketTests_ReceivePing__Test) {
    auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    std::vector<std::string> pings;
    ws.SetPingDelegate([&pings](const std::string& data) { pings.push_back(data); });

    const std::string frame = "\x89\x06World!";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ((std::vector<std::string>{"World!"}), pings);
    ASSERT_LE(12, connection->webSocketOutput.length());
    ASSERT_EQ("\x8A\x86", connection->webSocketOutput.substr(0, 2));
    for (size_t i = 0; i < 6; ++i)
    {
        ASSERT_EQ(frame[2 + i] ^ connection->webSocketOutput[2 + (i % 4)],
                  connection->webSocketOutput[6 + i]);
    }
}

TEST_F(WebSocketTests, WebSocketTests_SendPongNormaly__Test) {
    auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.Pong("Hello");
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x8A\x05Hello", connection->webSocketOutput);
}

TEST_F(WebSocketTests, WebSocketTests_SendPongTooMuchData__Test) {
    auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.Pong(std::string(126, 'x'));
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("", connection->webSocketOutput);
}

TEST_F(WebSocketTests, WebSocketTests_SendPongMaxAllowedData__Test) {
    auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.Pong(std::string(125, 'x'));
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x8A\x7D" + std::string(125, 'x'), connection->webSocketOutput);
}

TEST_F(WebSocketTests, WebSocketTests_ReceivePong__Test) {
    auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    std::vector<std::string> pongs;
    ws.SetPongDelegate([&pongs](const std::string& data) { pongs.push_back(data); });
    const std::string frame = "\x8A\x06World!";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ((std::vector<std::string>{
                  "World!",
              }),
              pongs);
}

TEST_F(WebSocketTests, WebSocketTests_SendText__Test) {
    const auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.SendText("Hello, World!");
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x81\x0DHello, World!", connection->webSocketOutput);
}

TEST_F(WebSocketTests, WebSocketTests_ReceiveText__Test) {
    auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    std::vector<std::string> text;
    ws.SetTextDelegate([&text](const std::string& data) { text.push_back(data); });
    const std::string frame = "\x81\x0DHello, World!";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ((std::vector<std::string>{"Hello, World!"}), text);
}

TEST_F(WebSocketTests, WebSocketTests_SendBinary__Test) {
    const auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    ws.SendBinary("Hello, World!");
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ("\x82\x0DHello, World!", connection->webSocketOutput);
}

TEST_F(WebSocketTests, WebSocketTests_ReceiveBinary__Test) {
    auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    std::vector<std::string> binary;
    ws.SetBinaryDelegate([&binary](const std::string& data) { binary.push_back(data); });
    const std::string frame = "\x82\x0DHello, World!";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ((std::vector<std::string>{"Hello, World!"}), binary);
}

TEST_F(WebSocketTests, WebSocketTests_SendMasked__Test) {
    const auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    const std::string data = "Hello, World!";
    ws.SendText(data);
    ASSERT_EQ(19, connection->webSocketOutput.length());
    ASSERT_EQ("\x81\x8D", connection->webSocketOutput.substr(0, 2));
    for (size_t i = 0; i < 13; ++i)
    {
        ASSERT_EQ(data[i] ^ connection->webSocketOutput[2 + (i % 4)],
                  connection->webSocketOutput[6 + i]);
    }
}

TEST_F(WebSocketTests, WebSocketTests_ReceiveMasked__Test) {
    auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    std::vector<std::string> text;
    ws.SetTextDelegate([&text](const std::string& data) { text.push_back(data); });
    const char maskingKey[4] = {0x12, 0X13, 0X14, 0X17};
    const std::string data = "Hello, world!";
    std::string frame = "\x81\x8D";
    frame += std::string(maskingKey, 4);
    for (size_t i = 0; i < data.length(); ++i)
    { frame += data[i] ^ maskingKey[i % 4]; }
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ((std::vector<std::string>{data}), text);
}

TEST_F(WebSocketTests, WebSocketTests_SendFragmentedText__Test) {
    const auto connection = std::make_shared<MockConnection>();
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

TEST_F(WebSocketTests, WebSocketTests_SendFragmentedBinary__Test) {
    const auto connection = std::make_shared<MockConnection>();
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

TEST_F(WebSocketTests, WebSocketTests_ReceivedFragmentedText__Test) {
    auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    std::vector<std::string> text;
    ws.SetTextDelegate([&text](const std::string& data) { text.push_back(data); });
    const std::vector<std::string> frames{std::string("\x01\06", 2) + "Hello,",
                                          std::string("\x00\x06", 2) + " World",
                                          "\x80\x01"
                                          "!"};
    for (const auto& frame : frames)
    { connection->dataReceivedDelegate({frame.begin(), frame.end()}); }
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ((std::vector<std::string>{"Hello, World!"}), text);
}

TEST_F(WebSocketTests, WebSocketTests_InitiateCloseNoStatusReturned__Test) {
    const auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](unsigned int code,
                                                         const std::string& reason)
        {
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        });
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

TEST_F(WebSocketTests, WebSocketTests_InitiateCloseStatusReturned__Test) {
    const auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](unsigned int code,
                                                         const std::string& reason)
        {
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        });
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
    const std::string unmaskedPayload =
        "\x03\xe8"
        "Bye";
    const char mask[4] = {0x12, 0x32, 0x31, 0x60};
    frame += std::string(mask, 4);
    for (size_t i = 0; i < unmaskedPayload.length(); ++i)
    { frame += unmaskedPayload[i] ^ mask[i % 4]; }
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_TRUE(connection->brokenByWebSocket);
    ASSERT_TRUE(closeReceived);
    EXPECT_EQ(1000, codeReceived);
    EXPECT_EQ("Bye", reasonReceived);
}

TEST_F(WebSocketTests, WebSocketTests_ReceiveCloseFrame__Test) {
    const auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](unsigned int code,
                                                         const std::string& reason)
        {
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        });
    const std::string frame = "\x88\x80XXXX";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_FALSE(connection->brokenByWebSocket);
    ASSERT_TRUE(closeReceived);
    EXPECT_EQ(1005, codeReceived);
    EXPECT_EQ("", reasonReceived);
    ASSERT_EQ((std::vector<std::string>{
                  "WebSocket::WebSocket[1]: Connection to mock-client closed by peer",
              }),
              diagnosticMessages);
    diagnosticMessages.clear();
    ws.Ping();
    ASSERT_EQ(std::string("\x89\x00", 2), connection->webSocketOutput);
    connection->webSocketOutput.clear();
    ws.Close(1000, "Goodbye!");
    ASSERT_EQ("\x88\x0A\x03\xe8Goodbye!", connection->webSocketOutput);
    ASSERT_TRUE(connection->brokenByWebSocket);
    ASSERT_EQ((std::vector<std::string>{
                  "WebSocket::WebSocket[1]: Connection to mock-client closed (Goodbye!)",
              }),
              diagnosticMessages);
    diagnosticMessages.clear();
}

TEST_F(WebSocketTests, WebSocketTests_ViolationReservedBits__Test) {
    const auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](unsigned int code,
                                                         const std::string& reason)
        {
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        });
    const std::string frame = "\x99\x80XXXX";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_EQ("\x88\x13\x03\xeareserved bits set", connection->webSocketOutput);
    ASSERT_TRUE(connection->brokenByWebSocket);
    ASSERT_TRUE(closeReceived);
    EXPECT_EQ(1002, codeReceived);
    EXPECT_EQ("reserved bits set", reasonReceived);
}

TEST_F(WebSocketTests, WebSocketTests_ViolationUnexpectedContinuation__Test) {
    const auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](unsigned int code,
                                                         const std::string& reason)
        {
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        });
    std::string frame = "\x80\x80XXXX";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_EQ("\x88\x1F\x03\xeaunexpected continuation frame", connection->webSocketOutput);
    ASSERT_TRUE(connection->brokenByWebSocket);
    ASSERT_TRUE(closeReceived);
    EXPECT_EQ(1002, codeReceived);
    EXPECT_EQ("unexpected continuation frame", reasonReceived);
}

TEST_F(WebSocketTests, WebSocketTests_ViolationNewFrameDuringFragmentedFrame__Test) {
    const auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](unsigned int code,
                                                         const std::string& reason)
        {
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        });
    std::string frame = "\x01\x80XXXX";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_TRUE(connection->webSocketOutput.empty());
    frame = "\02\x80XXXX";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_EQ("\x88\x19\x03\xealast message incomplete", connection->webSocketOutput);
    ASSERT_TRUE(connection->brokenByWebSocket);
    ASSERT_TRUE(closeReceived);
    EXPECT_EQ(1002, codeReceived);
    EXPECT_EQ("last message incomplete", reasonReceived);
}

TEST_F(WebSocketTests, WebSocketTests_ViolationUnknownOpcode__Test) {
    const auto connection = std::make_shared<MockConnection>();
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](unsigned int code,
                                                         const std::string& reason)
        {
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        });
    std::string frame = "\x83\x80XXXX";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_EQ("\x88\x10\x03\xeaunknown opcode", connection->webSocketOutput);
    ASSERT_TRUE(connection->brokenByWebSocket);
    ASSERT_TRUE(closeReceived);
    EXPECT_EQ(1002, codeReceived);
    EXPECT_EQ("unknown opcode", reasonReceived);
}

TEST_F(WebSocketTests, FailCompleteOpenAsServerNotGetMethod) {
    Http::Server::Request request;
    request.method = "POST";
    request.headers.SetHeader("Connection", "upgrade");
    request.headers.SetHeader("Update", "websocket");
    request.headers.SetHeader("Sec-WebSocket-Version", "13");
    const std::string key = Base64::EncodeToBase64("abcdefghijklmnop");
    request.headers.SetHeader("Sec-WebSocket-Key", key);
    Http::Client::Response response;
    const auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.OpenAsServer(connection, request, response, ""));
}

TEST_F(WebSocketTests, FailCompleteOpenAsServerWrongUpgrade) {
    Http::Server::Request request;
    request.method = "GET";
    request.headers.SetHeader("Connection", "upgrade");
    request.headers.SetHeader("Upgrade", "foobar");
    request.headers.SetHeader("Sec-WebSocket-Version", "13");
    const std::string key = Base64::EncodeToBase64("abcdefghijklmnop");
    request.headers.SetHeader("Sec-WebSocket-Key", key);
    Http::Client::Response response;
    const auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.OpenAsServer(connection, request, response, ""));
}

TEST_F(WebSocketTests, FailCompleteOpenAsServerMissUpgrade) {
    Http::Server::Request request;
    request.method = "GET";
    request.headers.SetHeader("Connection", "upgrade");
    request.headers.SetHeader("Sec-WebSocket-Version", "13");
    const std::string key = Base64::EncodeToBase64("abcdefghijklmnop");
    request.headers.SetHeader("Sec-WebSocket-Key", key);
    Http::Client::Response response;
    const auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.OpenAsServer(connection, request, response, ""));
}

TEST_F(WebSocketTests, FailCompleteOpenAsServerMissConnection) {
    Http::Server::Request request;
    request.method = "GET";
    request.headers.SetHeader("Upgrade", "websocket");
    request.headers.SetHeader("Sec-WebSocket-Version", "13");
    const std::string key = Base64::EncodeToBase64("abcdefghijklmnop");
    request.headers.SetHeader("Sec-WebSocket-Key", key);
    Http::Client::Response response;
    const auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.OpenAsServer(connection, request, response, ""));
}

TEST_F(WebSocketTests, FailCompleteOpenAsServerWrongConnection) {
    Http::Server::Request request;
    request.method = "GET";
    request.headers.SetHeader("Connection", "foobar");
    request.headers.SetHeader("Upgrade", "websocket");
    request.headers.SetHeader("Sec-WebSocket-Version", "13");
    const std::string key = Base64::EncodeToBase64("abcdefghijklmnop");
    request.headers.SetHeader("Sec-WebSocket-Key", key);
    Http::Client::Response response;
    const auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.OpenAsServer(connection, request, response, ""));
}

TEST_F(WebSocketTests, FailCompleteOpenAsServerWrongVersion) {
    Http::Server::Request request;
    request.method = "GET";
    request.headers.SetHeader("Connection", "upgrade");
    request.headers.SetHeader("Upgrade", "websocket");
    request.headers.SetHeader("Sec-WebSocket-Version", "12");
    const std::string key = Base64::EncodeToBase64("abcdefghijklmnop");
    request.headers.SetHeader("Sec-WebSocket-Key", key);
    Http::Client::Response response;
    const auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.OpenAsServer(connection, request, response, ""));
}

TEST_F(WebSocketTests, FailCompleteOpenAsServerMissingVersion) {
    Http::Server::Request request;
    request.method = "GET";
    request.headers.SetHeader("Connection", "upgrade");
    request.headers.SetHeader("Upgrade", "websocket");
    const std::string key = Base64::EncodeToBase64("abcdefghijklmnop");
    request.headers.SetHeader("Sec-WebSocket-Key", key);
    Http::Client::Response response;
    const auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.OpenAsServer(connection, request, response, ""));
}

TEST_F(WebSocketTests, FailCompleteOpenAsServerWrongKey) {
    Http::Server::Request request;
    request.method = "GET";
    request.headers.SetHeader("Connection", "upgrade");
    request.headers.SetHeader("Upgrade", "websocket");
    request.headers.SetHeader("Sec-WebSocket-Version", "13");
    const std::string key = Base64::EncodeToBase64("abcdefghijklmno");
    request.headers.SetHeader("Sec-WebSocket-Key", key);
    Http::Client::Response response;
    const auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.OpenAsServer(connection, request, response, ""));
}

TEST_F(WebSocketTests, FailCompleteOpenAsServerMissingKey) {
    Http::Server::Request request;
    request.method = "GET";
    request.headers.SetHeader("Connection", "upgrade");
    request.headers.SetHeader("Upgrade", "websocket");
    request.headers.SetHeader("Sec-WebSocket-Version", "13");
    Http::Client::Response response;
    const auto connection = std::make_shared<MockConnection>();
    ASSERT_FALSE(ws.OpenAsServer(connection, request, response, ""));
}

TEST_F(WebSocketTests, WebSocketTests_BadUtf8InText__Test) {
    const auto connection = std::make_shared<MockConnection>();
    Http::Server::Request request;
    Http::Client::Response response;
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](unsigned int code,
                                                         const std::string& reason)
        {
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        });
    const char maskingKey[4] = {0x12, 0X13, 0X14, 0X17};
    const std::string data = "\xc0\xaf";  // overly long encoding of '/'
    std::string frame = "\x81\x82";
    frame += std::string(maskingKey, 4);
    for (size_t i = 0; i < data.length(); ++i)
    { frame += data[i] ^ maskingKey[i % 4]; }
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_EQ("\x88\x2A\x03\xeftext message with invalid UTF-8 encoding",
              connection->webSocketOutput);
    ASSERT_TRUE(connection->brokenByWebSocket);
    ASSERT_TRUE(closeReceived);
    EXPECT_EQ(1007, codeReceived);
    EXPECT_EQ("text message with invalid UTF-8 encoding", reasonReceived);
}

TEST_F(WebSocketTests, WebSocketTests_Utf8InFragmentedText__Test) {
    const auto connection = std::make_shared<MockConnection>();
    Http::Server::Request request;
    Http::Client::Response response;
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    std::vector<std::string> text;
    ws.SetTextDelegate([&text](const std::string& data) { text.push_back(data); });
    std::string frame = "\x01\x02\xF0\xA3";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    EXPECT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ(std::vector<std::string>{}, text);
    frame = "\x80\x02\x8E\xB4";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    EXPECT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ(std::vector<std::string>{"𣎴"}, text);
}

TEST_F(WebSocketTests, WebSocketTests_BadUtf8InFragmentedText__Test) {
    const auto connection = std::make_shared<MockConnection>();
    Http::Server::Request request;
    Http::Client::Response response;
    ws.Open(connection, WebSocket::WebSocket::Role::Client);
    std::vector<std::string> text;
    ws.SetTextDelegate([&text](const std::string& data) { text.push_back(data); });
    std::string frame = "\x01\x02\xF0\xA3";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    EXPECT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ(std::vector<std::string>{}, text);
    frame = "\x80\x02\x8E";
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    EXPECT_FALSE(connection->brokenByWebSocket);
    ASSERT_EQ(std::vector<std::string>{}, text);
}

TEST_F(WebSocketTests, WebSocketTests_InvalidUtf8InReason__Test) {
    const auto connection = std::make_shared<MockConnection>();
    Http::Server::Request request;
    Http::Client::Response response;
    ws.Open(connection, WebSocket::WebSocket::Role::Server);
    unsigned int codeReceived;
    std::string reasonReceived;
    bool closeReceived = false;
    ws.SetCloseDelegate(
        [&codeReceived, &reasonReceived, &closeReceived](unsigned int code,
                                                         const std::string& reason)
        {
            codeReceived = code;
            reasonReceived = reason;
            closeReceived = true;
        });
    const char maskingKey[4] = {0x12, 0X13, 0X14, 0X17};
    const std::string data = "\x03\xe8\xc0\xaf";  // overly long encoding of '/'
    std::string frame = "\x88\x84";
    frame += std::string(maskingKey, 4);
    for (size_t i = 0; i < data.length(); ++i)
    { frame += data[i] ^ maskingKey[i % 4]; }
    connection->dataReceivedDelegate({frame.begin(), frame.end()});
    ASSERT_EQ("\x88\x28\x03\xefinvalid UTF-8 encoding in close reason",
              connection->webSocketOutput);
    ASSERT_TRUE(connection->brokenByWebSocket);
    ASSERT_TRUE(closeReceived);
    EXPECT_EQ(1007, codeReceived);
    EXPECT_EQ("invalid UTF-8 encoding in close reason", reasonReceived);
}

TEST_F(WebSocketTests, WebSocketTests_CompleteOpenAsServerTockenCapitalized__Test) {
    Http::Server::Request request;
    request.headers.SetHeader("Sec-WebSocket-Version", "13");
    request.headers.SetHeader("Connection", "Upgrade");
    request.headers.SetHeader("Upgrade", "websocket");
    const std::string key = Base64::EncodeToBase64("abcdefghijklmnop");
    request.headers.SetHeader("Sec-webSocket-key", key);
    Http::Client::Response response;
    auto connection = std::make_shared<MockConnection>();
    ASSERT_TRUE(ws.OpenAsServer(connection, request, response, ""));
}
