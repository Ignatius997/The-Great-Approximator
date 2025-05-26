#ifndef TGA_MSG_CMB_H
#define TGA_MSG_CMB_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <utility>
#include <optional>

#include "Message.h"
#include "io.h"
#include "Rational.h"
#include "utils.h"

namespace tga {
namespace msg {

using tga::utils::ReceiveInfo;
using tga::rat::Rational;
using MsgPtr = std::unique_ptr<Message>;

// ==== Establishing Connection Messages ====

/**
 * @brief Class representing HELLO message sent by the client to the server.
 * This message contains the player ID.
 */
class HelloMessage : public Message {
private:
    const std::string player_id;

public:
    explicit HelloMessage(const std::string& id) : player_id(id) {}

    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo &rinfo);

    std::string messageType() const override final { return "HELLO"; }

    std::string messageContent() const override { return player_id; }
};

/**
 * @brief Class representing COEFF message sent by the server to the client.
 * This message contains the coefficients of the polynomial.
 */
class CoeffMessage : public Message {
private:
    /** Coefficients of the polynomial. coeffs.size() == N + 1. */
    const std::vector<Rational> coeffs;

public:
    CoeffMessage(std::vector<Rational> coeffs) : Message(), coeffs(std::move(coeffs)) {}

    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo &rinfo);

    std::string messageType() const override final { return "COEFF"; }

    std::string messageContent() const override {
        return tga::io::file::read_coeffs();
    }
};

// ==== Game Messages ====

/**
 * @brief Class representing STATE message sent by the server to the client.
 * This message contains the approximation of the function.
 */
class StateMessage : public Message {
private:
    // TODO Is this correct? What if we change this approx?
    // NOTE Change K to the actual size of the approximation
    const std::vector<Rational> &approx; // Approximation of the function. approx.size() == K.

public:
    StateMessage(const std::vector<Rational> &approx) : approx(approx) {}

    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo &rinfo);

    std::string messageType() const override final { return "STATE"; }

    std::string messageContent() const override {
        std::string content;

        for (const auto& coeff : approx) {
            content += static_cast<std::string>(coeff) + " ";
        }

        if (!content.empty()) {
            content.pop_back(); // Remove the last space
        }
        
        return content;
    }
};

/**
 * @brief Point-Value-Message. Abstract base class for messages that contain a point and a value.
 */
class PVMessage : public Message {
private:
    const size_t point;
    const Rational value;

public:
    PVMessage(size_t p, Rational v) : point(p), value(std::move(v)) {}
    static std::optional<std::pair<size_t, Rational>> deserialize_helper(
                                                    const std::string& msg_body,
                                                    ReceiveInfo& rinfo);
    
    std::string messageContent() const override final {
        return std::to_string(point) + " " + static_cast<std::string>(value);
    }
};

/**
 * @brief Class representing PUT message sent by the client to the server.
 * This message contains the point and the value to be put.
 */
class PutMessage : public PVMessage {
public:
    using PVMessage::PVMessage; // Inherit constructor
    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo& rinfo);

    std::string messageType() const override final { return "PUT"; }
};

/**
 * @brief Class representing BAD_PUT message sent by the server to the client.
 * This message indicates that the PUT message was not accepted.
 */
class BadPutMessage : public PVMessage {
public:
    using PVMessage::PVMessage; // Inherit constructor
    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo& rinfo);

    std::string messageType() const override final { return "BAD_PUT"; }
};

/**
 * @brief Class representing PENALTY message sent by the server to the client.
 * This message indicates that the client has been penalized.
 */
class PenaltyMessage : public PVMessage {
public:
    using PVMessage::PVMessage; // Inherit constructor

    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo &rinfo);
    std::string messageType() const override final { return "PENALTY"; }
};

// ==== Endgame Messages ====

/**
 * @brief Class representing END message sent by the server to the client.
 * This message indicates that the game has ended.
 */
class ScoringMessage : public Message {
private:
    const std::map<std::string, Rational> &scores;

public:
    ScoringMessage(std::map<std::string, Rational> scores) : scores(std::move(scores)) {}

    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo &rinfo);

    std::string messageType() const override final {
        return "SCORING";
    }

    // NOTE Check, if the order of players is correct.
    std::string messageContent() const override {
        std::string content;

        for (const auto& [player, score] : scores) {
            content += player + " " + static_cast<std::string>(score) + " ";
        }

        if (!content.empty()) {
            content.pop_back(); // Remove the last space
        }

        return content;
    }
};

MsgPtr deserialize_message(const char *buffer, const size_t len_received, ReceiveInfo &rinfo);

} // namespace msg
} // namespace tga

#endif // TGA_MSG_CMB_H