/**
 * @file WebSocket.cpp
 *
 * This module contains the implementation of the WebSocket::WebSocket class
 *
 * © 2024 by Hatem Nabli
 */

#include <stdint.h>
#include <Base64/Base64.hpp>
#include <Sha1/Sha1.hpp>
#include <StringUtils/StringUtils.hpp>
#include <SystemUtils/CryptoRandom.hpp>
#include <Utf8/Utf8.hpp>
#include <WebSocket/WebSocket.hpp>
#include <memory>
#include <string>
#include <vector>
namespace
{
    /**
     * This is the the websocket key salt to add to the "Sec-WebSocket-Key" before
     * computing  the SHA-1 hash and Base64 encoding to feorm the Sec-WebSocket-Accept.
     */
    const std::string WEBSOCKET_KEY_SALT = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    /**
     * This is the supported version of the websocket.
     */
    const std::string CURRENTLY_SUPPORTED_WEBSOCKET_VERSION = "13";
    /**
     * This is the length of the websocket key
     */
    constexpr size_t REQUEIRED_KEY_LENGTH = 16;
    /**
     *
     */
    constexpr size_t MAX_CONTROL_FRAME_DATA_LENGTH = 125;
    /**
     * This is the bit to set in the first octet of a WebSocket frame
     * to indicate that the frame is the final one in a message.
     */
    constexpr uint8_t FIN = 0x80;

    /**
     * This is the bit to set in the second octet of a WebSocket frame
     * to indicate that the payload of the frame is masked, and that a
     * masking key is needed.
     */
    constexpr uint8_t MASK = 0x80;

    /**
     * This is the opcode for a ping frame.
     */
    constexpr uint8_t OPCODE_PING = 0x09;

    /**
     * This is the opcode for a pong frame.
     */
    constexpr uint8_t OPCODE_PONG = 0x0A;

    /**
     * This is the opcode for a text frame.
     */
    constexpr uint8_t OPCODE_TEXT = 0x01;

    /**
     * This is the opcode for a binary frame.
     */
    constexpr uint8_t OPCODE_BINARY = 0x02;

    /**
     * This is the opcod for a continuation frame
     */
    constexpr uint8_t OPCODE_CONTINUATION = 0X00;

    /**
     * This is the opcod for a close frame
     */
    constexpr uint8_t OPCODE_CLOSE = 0X08;

    /**
     * This is used to track what kind of message is being sent or
     * received in fragments.
     */
    enum class FragmentedMessageType
    {
        /**
         * sendig/receiving any message.
         */
        None,
        /**
         * sending/receiving text message.
         */
        Text,
        /**
         * sending/receiving binary message.
         */
        Binary
    };
    /**
     * This function computes the value of the "Sec-WebSocket-Accept"
     * HTTP response header that matches the given value of the
     * "Sec-WebSocket-Key" HTTP request headerd.
     *
     * @param[in]
     *      This is the key for which to compute an answer using SHA-1
     *      and Base64
     */
    std::string ComputeValidationKey(const std::string& key) {
        return Base64::EncodeToBase64(Sha1::Sha1Bytes(key + WEBSOCKET_KEY_SALT));
    }
}  // namespace
namespace WebSocket
{
    /**
     * This contains the private properties of a WebSocket instance.
     */
    struct WebSocket::Impl
    {
        /* Properties */
        /**
         * This is a helper object used to generate and publish
         * diagnostic message.
         */
        SystemUtils::DiagnosticsSender diagnosticsSender;
        /**
         * This is the connection to use to send and receive frames.
         */
        std::shared_ptr<Http::Connection> connection;

        /**
         * This flag indicates whether or not the WebSocket has sent a close frame,
         * and is waiting for a one to be received back before closing the WebSocket.
         */
        bool closeWebSocketSent = false;

        /**
         * This flag indicates whether or not the WebSocket has receive a close frame.
         * and is waiting for the user to finish and signal a close, in order to close
         * the webSocket.
         */
        bool closeWebSocketReceived = false;
        /**
         * This is the role throughout the webSocket in the connection.
         */
        WebSocket::Role role;

        /**
         * This is the function to call whenever a pong message
         * is received from the websocket.
         */
        MessageReceivedDelegate pongDelegate;

        /**
         * This is the function to call whenever a ping message
         * is received from the websocket.
         */
        MessageReceivedDelegate pingDelegate;

        /**
         * This is the function to call whenever a text message
         * is received from the websocket.
         */
        MessageReceivedDelegate textDelegate;

        /**
         * This is the function to call whenever a binary message
         * is received from the websocket.
         */
        MessageReceivedDelegate binaryDelegate;

        /**
         * This is the function to call whenever a binary message
         * is received from the websocket.
         */
        CloseReceivedDelegate closeReceivedDelegate;

        /**
         * This flag indicates whether or not the webSocket is in the midst
         * of sending a fragmented message.
         */
        FragmentedMessageType sending = FragmentedMessageType::None;

        /**
         * This flag indicates whether or not the webSocket is in the midst
         * of receiving a fragmented  message.
         */
        FragmentedMessageType receiving = FragmentedMessageType::None;

        /**
         * This is the buffer that reassemble received data into a frame.
         */
        std::vector<uint8_t> reassemblyBuffer;

        /**
         * This is the buffer that reassemble fragmented frames into a message.
         */
        std::string reassemblyFragmentedBuffer;

        /**
         * This is the cryptoRandom key of the WebSocket.
         */
        SystemUtils::CryptoRandom randomKey;

        /**
         * This is the randomly-generated key used to form the Sec-WebSocket-Key
         * header sent in the HTTP request of the opening handshake, when opening
         * as a client.
         */
        std::string key;

        /* Methods */
        Impl() : diagnosticsSender("webSockets::WebSockets") {}
        /**
         * This method constructs and send a frame over the WebSocket.
         *
         * @param[in] fin
         *      This indicates whether or not to set the FIN bit in the frame.
         *
         * @param[in] opcode
         *      This is the opcode to set in the frame.
         *
         * @param[in] payload
         *      This is the payload to include in the frame.
         */
        void SendFrame(bool fin, uint8_t opcode, const std::string& payload) {
            std::vector<uint8_t> frame;
            frame.push_back((fin ? FIN : 0) + opcode);
            const uint8_t mask = ((role == WebSocket::Role::Client) ? MASK : 0);
            if (payload.length() < 126)
            {
                frame.push_back((uint8_t)payload.length() + mask);
            } else if (payload.length() < 65536)
            {
                frame.push_back(0x7E + mask);
                frame.push_back((uint8_t)(payload.length() >> 8));
                frame.push_back((uint8_t)(payload.length() & 0xFF));
            } else
            {
                frame.push_back(0x7F + mask);
                frame.push_back((uint8_t)(payload.length() >> 56));
                frame.push_back((uint8_t)(payload.length() >> 48) & 0xFF);
                frame.push_back((uint8_t)(payload.length() >> 40) & 0xFF);
                frame.push_back((uint8_t)(payload.length() >> 32) & 0xFF);
                frame.push_back((uint8_t)(payload.length() >> 24) & 0xFF);
                frame.push_back((uint8_t)(payload.length() >> 16) & 0xFF);
                frame.push_back((uint8_t)(payload.length() >> 8) & 0xFF);
                frame.push_back((uint8_t)(payload.length() & 0xFF));
            }
            if (mask == 0)
            {
                (void)frame.insert(frame.end(), payload.begin(), payload.end());
            } else
            {
                // TODO: need to pick one at randome from a source of entropy.
                uint8_t maskingKey[4];
                SystemUtils::CryptoRandom crypto;
                crypto.Generate(maskingKey, sizeof(maskingKey));
                for (size_t i = 0; i < sizeof(maskingKey); ++i)
                { frame.push_back(maskingKey[i]); }
                for (size_t i = 0; i < payload.length(); i++)
                { frame.push_back(payload[i] ^ maskingKey[i % 4]); }
            }
            connection->SendData(frame);
        }
        /**
         * This method is called whenever the WebSocket has reassembled
         * a complete frame received from the remote peer.
         */
        void ReceiveFrame(size_t headerLength, size_t payloadLength) {
            const bool fin = ((reassemblyBuffer[0] & FIN) != 0);
            const uint8_t reservedBits = ((reassemblyBuffer[0] >> 4) & 0x07);
            if (reservedBits != 0)
            {
                Close(1002, "reserved bits set", true);
                return;
            }
            const uint8_t opcode = (reassemblyBuffer[0] & 0x0F);
            std::string data;
            if (role == Role::Server)
            {
                data.resize(payloadLength);
                for (size_t i = 0; i < payloadLength; ++i)
                {
                    data[i] = (reassemblyBuffer[headerLength + i] ^
                               reassemblyBuffer[headerLength - 4 + (i % 4)]);
                }
            } else
            {
                (void)data.assign(reassemblyBuffer.begin() + headerLength,
                                  reassemblyBuffer.begin() + headerLength + payloadLength);
            }
            switch (opcode)
            {
            case OPCODE_PING: {
                if (pingDelegate != nullptr)
                { pingDelegate(data); }
                SendFrame(true, OPCODE_PONG, data);
            }
            break;

            case OPCODE_PONG: {
                if (pongDelegate != nullptr)
                { pongDelegate(data); }
            }
            break;

            case OPCODE_CLOSE: {
                unsigned int statusCode = 1005;
                std::string reason;
                bool fail = false;
                if (data.length() >= 2)
                {
                    statusCode = ((((unsigned int)data[0] << 8) & 0xFF00) +
                                  ((unsigned int)data[1] & 0x00FF));
                    reason = data.substr(2);
                    Utf8::Utf8 utf8;
                    if (!utf8.IsValidEncoding(reason))
                    {
                        fail = true;
                        Close(1007, "invalid UTF-8 encoding in close reason", fail);
                    }
                }
                if (!fail)
                { OnCloseReceipt(statusCode, reason); }
            }
            break;

            case OPCODE_CONTINUATION: {
                reassemblyFragmentedBuffer += data;
                switch (receiving)
                {
                case FragmentedMessageType::Text:
                    if (fin)
                    { OnTextMessage(reassemblyFragmentedBuffer); }
                    break;
                case FragmentedMessageType::Binary:
                    if (fin)
                    {
                        if (binaryDelegate != nullptr)
                        { binaryDelegate(reassemblyFragmentedBuffer); }
                    }
                    break;
                default:
                    reassemblyFragmentedBuffer.clear();
                    Close(1002, "unexpected continuation frame", true);
                    break;
                }
                if (fin)
                {
                    receiving = FragmentedMessageType::None;
                    reassemblyFragmentedBuffer.clear();
                }
            }
            break;

            case OPCODE_TEXT: {
                if (receiving == FragmentedMessageType::None)
                {
                    if (fin)
                    {
                        OnTextMessage(data);
                    } else
                    {
                        receiving = FragmentedMessageType::Text;
                        reassemblyFragmentedBuffer = data;
                    }
                } else
                { Close(1002, "last message incomplete", true); }
            }
            break;

            case OPCODE_BINARY: {
                if (receiving == FragmentedMessageType::None)
                {
                    if (fin)
                    {
                        if (binaryDelegate != nullptr)
                        { binaryDelegate(data); }
                    } else
                    {
                        receiving = FragmentedMessageType::Binary;
                        reassemblyFragmentedBuffer = data;
                    }
                } else
                { Close(1002, "last message incomplete", true); }
            }
            break;
            default: {
                Close(1002, "unknown opcode", true);
            }
            }
        }

        /**
         * This method is called whenever the WebSocket receives data from
         * the remote peer.
         *
         * @param[in] data
         *      This is the data received from the remote peer.
         */
        void ReceiveData(const std::vector<uint8_t>& data) {
            (void)reassemblyBuffer.insert(reassemblyBuffer.end(), data.begin(), data.end());
            for (;;)
            {
                if (reassemblyBuffer.size() < 2)
                { return; }
                const auto lengthFirstOctet = (reassemblyBuffer[1] & ~MASK);
                size_t headerLength, payloadLength;
                if (lengthFirstOctet == 0x7E)
                {
                    headerLength = 4;
                    if (reassemblyBuffer.size() < headerLength)
                    { return; }
                    payloadLength =
                        (((size_t)reassemblyBuffer[2] << 8) + (size_t)reassemblyBuffer[3]);
                } else if (lengthFirstOctet == 0x7F)
                {
                    headerLength = 10;
                    if (reassemblyBuffer.size() < headerLength)
                    { return; }
                    payloadLength =
                        (((size_t)reassemblyBuffer[2] << 56) + ((size_t)reassemblyBuffer[3] << 48) +
                         ((size_t)reassemblyBuffer[4] << 40) + ((size_t)reassemblyBuffer[5] << 32) +
                         ((size_t)reassemblyBuffer[6] << 24) + ((size_t)reassemblyBuffer[7] << 16) +
                         ((size_t)reassemblyBuffer[8] << 8) + (size_t)reassemblyBuffer[9]);
                } else
                {
                    headerLength = 2;
                    payloadLength = (size_t)lengthFirstOctet;
                }
                if (role == Role::Server)
                { headerLength += 4; }
                if (reassemblyBuffer.size() < headerLength + payloadLength)
                { return; }
                ReceiveFrame(headerLength, payloadLength);
                (void)reassemblyBuffer.erase(
                    reassemblyBuffer.begin(),
                    reassemblyBuffer.begin() + headerLength + payloadLength);
            }
        }
        /**
         * This method initiates the closing of the WebSocket sending a close frame
         * with the given parameters.
         *
         * @param[in] statusCode
         *      This is the status code to send in the frame.
         * @param[in] reason
         *      This is the reason to send in the frame.
         * @param[in] fail
         *      This is the indication of wather or not to fail the connection,
         *      closing the connection and reports the closing, rather than waiting
         *      for the receipt of a close frame from the remote peer.
         */
        void Close(unsigned int statusCode, const std::string& reason, bool fail = false) {
            if (closeWebSocketSent)
            { return; }
            closeWebSocketSent = true;
            if (statusCode == 1006)
            {
                OnCloseReceipt(statusCode, reason);
            } else
            {
                std::string data;
                if (statusCode != 1005)
                {
                    data.push_back((uint8_t)(statusCode >> 8));
                    data.push_back((uint8_t)(statusCode & 0xFF));
                    data += reason;
                }
                SendFrame(true, OPCODE_CLOSE, data);
                if (fail)
                {
                    OnCloseReceipt(statusCode, reason);
                } else if (closeWebSocketReceived)
                { connection->Break(true); }
            }
        }
        /**
         * This method responds to the WebSocket being closed.
         *
         * @param[in] statusCode
         *      This is the status code of the closure.
         * @param[in] reason
         *      This is the reason of the closure.
         */
        void OnCloseReceipt(unsigned int statusCode, const std::string& reason) {
            const auto closeWasSent = closeWebSocketSent;
            closeWebSocketReceived = true;
            if (closeReceivedDelegate != nullptr)
            { closeReceivedDelegate(statusCode, reason); }
            if (closeWasSent)
            { connection->Break(false); }
        }
        /**
         * This method is called if the connection is broken by the remote peer.
         */
        void ConnectionBroken() {
            Close(1006, "connection broken by peer", true);
            diagnosticsSender.SendDiagnosticInformationFormatted(
                1, StringUtils::sprintf("Connection to %s Broken by peer", connection->GetPeerId())
                       .c_str());
        }
        /**
         * This method is called if a text message has been received in the
         * reassemblyBuffer.
         */
        void OnTextMessage(const std::string& message) {
            Utf8::Utf8 utf8;
            if (utf8.IsValidEncoding(message))
            {
                if (textDelegate != nullptr)
                { textDelegate(message); }
            } else
            { Close(1007, "text message with invalid UTF-8 encoding", true); }
        }
    };

    WebSocket::~WebSocket() noexcept = default;
    WebSocket::WebSocket(WebSocket&&) noexcept = default;
    WebSocket& WebSocket::operator=(WebSocket&&) noexcept = default;

    WebSocket::WebSocket() : impl_(new Impl) {}

    void WebSocket::Open(std::shared_ptr<Http::Connection> connection, Role role) {
        impl_->connection = connection;
        impl_->role = role;
        impl_->connection->SetDataReceivedDelegate([this](const std::vector<uint8_t>& data)
                                                   { impl_->ReceiveData(data); });
        impl_->connection->SetConnectionBrokenDelegate([this](bool) { impl_->ConnectionBroken(); });
    }

    bool WebSocket::OpenAsServer(std::shared_ptr<Http::Connection> connection,
                                 const Http::Server::Request& request,
                                 Http::Client::Response& response, const std::string& trailer) {
        if (request.headers.GetHeaderValue("Sec-WebSocket-Version") !=
            CURRENTLY_SUPPORTED_WEBSOCKET_VERSION)
        { return false; }
        bool foundUpgradeToken = false;
        for (const auto token : request.headers.GetHeaderTokens("Connection"))
        {
            if (token == "upgrade")
            {
                foundUpgradeToken = true;
                break;
            }
        }
        if (!foundUpgradeToken)
        { return false; }
        if (StringUtils::NormalizeCaseInsensitiveString(
                request.headers.GetHeaderValue("Upgrade")) != "websocket")
        { return false; }
        impl_->key = request.headers.GetHeaderValue("sec-WebSocket-key");

        if (Base64::DecodeFromBase64(impl_->key).length() != REQUEIRED_KEY_LENGTH)
        { return false; }
        auto connectionTockens = request.headers.GetHeaderMultiValues("connection");
        connectionTockens.push_back("upgrade");
        response.statusCode = 101;
        response.status = "Switching Protocols";
        response.headers.SetHeader("Connection", connectionTockens, true);
        response.headers.SetHeader("Upgrade", "websocket");
        response.headers.SetHeader("Sec-WebSocket-Accept", ComputeValidationKey(impl_->key));
        Open(connection, WebSocket::Role::Server);
        if (!trailer.empty())
        { impl_->ReceiveData(std::vector<uint8_t>(trailer.begin(), trailer.end())); }
        return true;
    }

    void WebSocket::StartOpenAsClient(Http::Server::Request& request) {
        char nonce[16];
        request.headers.SetHeader("Sec-WebSocket-Version", CURRENTLY_SUPPORTED_WEBSOCKET_VERSION);
        impl_->randomKey.Generate(nonce, sizeof(nonce));
        impl_->key = Base64::EncodeToBase64(std::string(nonce, sizeof(nonce)));
        request.headers.SetHeader("Sec-WebSocket-Key", impl_->key);
        request.headers.SetHeader("Upgrade", "websocket");
        auto connectionTockens = request.headers.GetHeaderMultiValues("connection");
        connectionTockens.push_back("upgrade");
        request.headers.SetHeader("Connection", connectionTockens, true);
    }

    bool WebSocket::CompleteOpenAsClient(std::shared_ptr<Http::Connection> connection,
                                         const Http::Client::Response& response) {
        if (response.statusCode != 101)
        { return false; }
        bool foundUpgradeToken = false;
        for (const auto token : response.headers.GetHeaderTokens("Connection"))
        {
            if (token == "upgrade")
            {
                foundUpgradeToken = true;
                break;
            }
        }
        if (!foundUpgradeToken)
        { return false; }
        if (StringUtils::NormalizeCaseInsensitiveString(
                response.headers.GetHeaderValue("Upgrade")) != "websocket")
        { return false; }
        if (response.headers.GetHeaderValue("Sec-WebSocket-Accept") !=
            ComputeValidationKey(impl_->key))
        { return false; }
        if (!response.headers.GetHeaderTokens("Sec-WebSocket-Extension").empty())
        { return false; }
        if (!response.headers.GetHeaderTokens("Sec-WebSocket-Protocol").empty())
        { return false; }
        Open(connection, WebSocket::Role::Client);
        return true;
    }

    void WebSocket::Close(unsigned int statusCode, const std::string& reason) {
        impl_->Close(statusCode, reason);
    }

    void WebSocket::Ping(const std::string& data) {
        if (impl_->closeWebSocketSent)
        { return; }
        if (data.length() > MAX_CONTROL_FRAME_DATA_LENGTH)
        { return; }
        impl_->SendFrame(true, OPCODE_PING, data);
    }

    void WebSocket::Pong(const std::string& data) {
        if (impl_->closeWebSocketSent)
        { return; }
        if (data.length() > MAX_CONTROL_FRAME_DATA_LENGTH)
        { return; }
        impl_->SendFrame(true, OPCODE_PONG, data);
    }

    void WebSocket::SendText(const std::string& text, bool lastFragment) {
        if (impl_->closeWebSocketSent)
        { return; }
        if (impl_->sending == FragmentedMessageType::Binary)
        { return; }
        const auto opcode =
            ((impl_->sending == FragmentedMessageType::Text) ? OPCODE_CONTINUATION : OPCODE_TEXT);
        impl_->SendFrame(lastFragment, opcode, text);
        impl_->sending = (lastFragment ? FragmentedMessageType::None : FragmentedMessageType::Text);
    }

    void WebSocket::SendBinary(const std::string& binary, bool lastFragment) {
        if (impl_->closeWebSocketSent)
        { return; }
        if (impl_->sending == FragmentedMessageType::Text)
        { return; }
        const auto opcode = ((impl_->sending == FragmentedMessageType::Binary) ? OPCODE_CONTINUATION
                                                                               : OPCODE_BINARY);
        impl_->SendFrame(lastFragment, opcode, binary);
        impl_->sending =
            (lastFragment ? FragmentedMessageType::None : FragmentedMessageType::Binary);
    }

    void WebSocket::SetPingDelegate(MessageReceivedDelegate pingDelegate) {
        impl_->pingDelegate = pingDelegate;
    }

    void WebSocket::SetPongDelegate(MessageReceivedDelegate pongDelegate) {
        impl_->pongDelegate = pongDelegate;
    }

    void WebSocket::SetTextDelegate(MessageReceivedDelegate textDelegate) {
        impl_->textDelegate = textDelegate;
    }

    void WebSocket::SetBinaryDelegate(MessageReceivedDelegate binaryDelegate) {
        impl_->binaryDelegate = binaryDelegate;
    }

    void WebSocket::SetCloseDelegate(CloseReceivedDelegate closeReceivedDelegate) {
        impl_->closeReceivedDelegate = closeReceivedDelegate;
    }

}  // namespace WebSocket
