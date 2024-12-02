/**
 * @file WebSocket.cpp
 * 
 * This module contains the implementation of the WebSocket::WebSocket class
 * 
 * © 2024 by Hatem Nabli
 */

#include <stdint.h>
#include <vector>
#include <WebSocket/WebSocket.hpp>
namespace {

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
}
namespace WebSocket {
    /**
     * This contains the private properties of a WebSocket instance.
     */
    struct WebSocket::Impl
    {
        /* Properties */
        /**
         * This is the connection to use to send and receive frames.
         */
        std::shared_ptr< Http::Connection > connection;

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
         * This is the 
         */
        std::vector< uint8_t > reassemblyBuffer;
        /* Methods */

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
        void SendFrame(
            bool fin,
            uint8_t opcode,
            const std::string& payload
        ) {
            std::vector< uint8_t > frame;
            frame.push_back((fin ? FIN : 0) + opcode);
            const uint8_t mask = ((role == WebSocket::Role::Client) ? MASK : 0);
            if (payload.length() < 126) {
                frame.push_back((uint8_t)payload.length() + mask);
            } else if (payload.length() < 65536) {
                frame.push_back(0x7E + mask);
                frame.push_back((uint8_t)(payload.length() >> 8));
                frame.push_back((uint8_t)(payload.length() & 0xFF));
            } else {
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
            if (mask == 0) {
                (void)frame.insert(
                    frame.end(),
                    payload.begin(),
                    payload.end()
                );
            } else {
                // TODO: need to pick one at randome from a source of entropy.
                uint8_t maskingKey[4] = {0xDE, 0XAD, 0XBE, 0XEF};
                for (size_t i = 0; i < sizeof(maskingKey); ++i) {
                    frame.push_back(maskingKey[i]);
                }
                for (size_t i = 0; i < payload.length(); i++) {
                    frame.push_back(payload[i] ^ maskingKey[i % 4]);
                }
            }
            connection->SendData(frame);
        }
        /**
         * This method is called whenever the WebSocket has reassembled
         * a complete frame received from the remote peer.
         */
        void ReceiveFrame(
            size_t headerLength,
            size_t payloadLength
        ) {
            const uint8_t opcode = (reassemblyBuffer[0] & 0x0F);
            std::string data;
            if (role == Role::Server) {
                data.resize(payloadLength);
                for (size_t i = 0; i < payloadLength; ++i) {
                    data[i] = (
                        reassemblyBuffer[headerLength + i]
                        ^ reassemblyBuffer[headerLength - 4 + (i % 4)]
                    );
                }
            } else {
                (void)data.assign(
                    reassemblyBuffer.begin() + headerLength,
                    reassemblyBuffer.begin() + headerLength + payloadLength                    
                );
            }
            switch (opcode)
            {
                case OPCODE_PING: {
                    if (pingDelegate != nullptr) {
                        pingDelegate(data);
                    }
                    SendFrame(true, OPCODE_PONG, data);
                } break;

                case OPCODE_PONG: {
                    if (pongDelegate != nullptr) {
                        pongDelegate(data);
                    }
                } break;

                case OPCODE_TEXT: {
                    if (textDelegate != nullptr) {
                        textDelegate(data);
                    }
                } break;

                case OPCODE_BINARY: {
                    if (binaryDelegate != nullptr) {
                        binaryDelegate(data);
                    }
                } break;
            }
        }

        /**
         * This method is called whenever the WebSocket receives data from 
         * the remote peer.
         * 
         * @param[in] data
         *      This is the data received from the remote peer.
         */
        void ReceiveData(
            const std::vector< uint8_t >& data
        ){
            (void)reassemblyBuffer.insert(
                reassemblyBuffer.end(),
                data.begin(),
                data.end()
            );
            for(;;) {
                if (reassemblyBuffer.size() < 2) {
                    return;
                }
                const auto lengthFirstOctet = (reassemblyBuffer[1] & ~MASK);
                size_t headerLength, payloadLength;
                if (lengthFirstOctet == 0x7E) {
                    headerLength = 4; 
                    if (reassemblyBuffer.size() < headerLength) {
                         return;
                    }        
                    payloadLength = (
                        ((size_t)reassemblyBuffer[2] << 8)
                        + (size_t)reassemblyBuffer[3]
                    );
                } else if (lengthFirstOctet == 0x7F) {
                    headerLength = 10; 
                    if (reassemblyBuffer.size() < headerLength) {
                        return;
                    }
                    payloadLength = (
                        ((size_t)reassemblyBuffer[2] << 56)
                        + ((size_t)reassemblyBuffer[3] << 48)
                        + ((size_t)reassemblyBuffer[4] << 40)
                        + ((size_t)reassemblyBuffer[5] << 32)
                        + ((size_t)reassemblyBuffer[6] << 24)
                        + ((size_t)reassemblyBuffer[7] << 16)
                        + ((size_t)reassemblyBuffer[8] << 8)
                        + (size_t)reassemblyBuffer[9]
                    );
                } else {
                    headerLength = 2;
                    payloadLength = (size_t)lengthFirstOctet;
                }
                if (role == Role::Server) {
                    headerLength += 4;
                }
                if (reassemblyBuffer.size() < headerLength + payloadLength) {
                    return;
                }
                ReceiveFrame(headerLength, payloadLength);
                (void)reassemblyBuffer.erase(
                    reassemblyBuffer.begin(),
                    reassemblyBuffer.begin() + headerLength + payloadLength
                );
            } 
        }
    };

    WebSocket::~WebSocket() = default;

    WebSocket::WebSocket(): impl_(new Impl) {

    }

    void WebSocket::Open(
        std::shared_ptr< Http::Connection > connection,
        Role role
    ) {
        impl_->connection = connection;
        impl_->role = role;
        impl_->connection->SetDataReceivedDelegate(
            [this](
                const std::vector< uint8_t >& data
            ){
                impl_->ReceiveData(data);
            }
        );
    }
    
    void WebSocket::Ping(const std::string& data) {
        if (data.length() > MAX_CONTROL_FRAME_DATA_LENGTH) {
            return;
        }
        impl_->SendFrame(true, OPCODE_PING, data);
    }

    void WebSocket::Pong(const std::string& data) {
        if (data.length() > MAX_CONTROL_FRAME_DATA_LENGTH) {
            return;
        }
        impl_->SendFrame(true, OPCODE_PONG, data);
    }

    void WebSocket::SendText(const std::string& text) {
        impl_->SendFrame(true, OPCODE_TEXT, text);
    }

    void WebSocket::SendBinary(const std::string& binary) {
        impl_->SendFrame(true, OPCODE_BINARY, binary);
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

}
