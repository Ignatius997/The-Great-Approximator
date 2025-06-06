#ifndef TGA_MSG_CMB_H
#define TGA_MSG_CMB_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <utility>
#include <optional>
#include <algorithm>

#include "Message.h"
#include "io.h"
#include "Rational.h"
#include "netutils.h"

namespace tga {
namespace msg {

using tga::net::ReceiveInfo;
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
    static constexpr const char *msg_type = "HELLO";

    explicit HelloMessage(const std::string& id) : player_id(id) {}
    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo &rinfo);
    std::string message_type() const override final { return msg_type; }
    std::string message_content() const override { return player_id; }
    std::string get_player_id() const { return player_id; }
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
    static constexpr const char *msg_type = "COEFF";

    CoeffMessage(std::vector<Rational> coeffs) : Message(), coeffs(coeffs) {}
    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo &rinfo);
    std::string message_type() const override final { return msg_type; }
    std::string message_content() const override {
        std::string content;

        for (const auto& coeff : coeffs) {
            content += static_cast<std::string>(coeff) + " ";
        }

        if (!content.empty()) {
            content.pop_back(); // Remove the last space
        }
        
        return content;
    }
    /**
     * @brief Returns the coefficients of the polynomial.
     * @return A vector of Rational numbers representing the coefficients.
     */
    std::vector<Rational> get_coefficients() const {
        return coeffs;
    }
};

// ==== Game Messages ====

/**
 * @brief Class representing STATE message sent by the server to the client.
 * This message contains the approximation of the function.
 */
class StateMessage : public Message {
private:
    std::vector<Rational> approx; // Approximation of the function. approx.size() == K.

public:
    static constexpr const char *msg_type = "STATE";

    StateMessage(std::vector<Rational> approx) : approx(approx) {}
    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo &rinfo);
    std::string message_type() const override final { return msg_type; }
    std::string message_content() const override {
        std::string content;

        for (const auto& coeff : approx) {
            content += static_cast<std::string>(coeff) + " ";
        }

        if (!content.empty()) {
            content.pop_back(); // Remove the last space
        }
        
        return content;
    }

    std::vector<Rational> get_approximations() const {
        return approx;
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
    PVMessage(size_t p, Rational v) : point(p), value(v) {}
    static std::optional<std::pair<size_t, Rational>> deserialize_helper(
                                                    const std::string& msg_body,
                                                    ReceiveInfo& rinfo);
    std::string message_content() const override final {
        return std::to_string(point) + " " + static_cast<std::string>(value);
    }
    size_t get_point() const { return point; }
    Rational get_value() const { return value; }
};

/**
 * @brief Class representing PUT message sent by the client to the server.
 * This message contains the point and the value to be put.
 */
class PutMessage : public PVMessage {
public:
    static constexpr const char *msg_type = "PUT";

    using PVMessage::PVMessage; // Inherit constructor
    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo& rinfo);
    std::string message_type() const override final { return msg_type; }
};

/**
 * @brief Class representing BAD_PUT message sent by the server to the client.
 * This message indicates that the PUT message was not accepted.
 */
class BadPutMessage : public PVMessage {
public:
    static constexpr const char *msg_type = "BAD_PUT";
    using PVMessage::PVMessage; // Inherit constructor
    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo& rinfo);
    std::string message_type() const override final { return msg_type; }
};

/**
 * @brief Class representing PENALTY message sent by the server to the client.
 * This message indicates that the client has been penalized.
 */
class PenaltyMessage : public PVMessage {
public:
    static constexpr const char *msg_type = "PENALTY";
    using PVMessage::PVMessage; // Inherit constructor
    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo &rinfo);
    std::string message_type() const override final { return msg_type; }
};

// ==== Endgame Messages ====

/**
 * @brief Class representing END message sent by the server to the client.
 * This message indicates that the game has ended.
 */
class ScoringMessage : public Message {
private:
    std::vector<std::pair<std::string, Rational>> scores;

public:
    static constexpr const char *msg_type = "SCORING";

    ScoringMessage(std::vector<std::pair<std::string, Rational>> scores) : scores(scores) {}
    static MsgPtr deserialize(const std::string& msg_body, ReceiveInfo &rinfo);
    std::string message_type() const override final { return msg_type; }

    std::string message_content() const override {
        std::string content;

        // Sort the scores lexicographically by player id (ASCII order)
        std::vector<std::pair<std::string, Rational>> sorted_scores = scores;
        std::sort(sorted_scores.begin(), sorted_scores.end(),
            [](const auto& a, const auto& b) {
                return a.first < b.first;
            });

        for (const auto& [player, score] : sorted_scores) {
            content += player + " " + static_cast<std::string>(score) + " ";
        }

        if (!content.empty()) {
            content.pop_back(); // Remove the last space
        }

        return content;
    }

    /**
     * @brief Returns the scores of the players.
     * @return A map of player IDs to their scores.
     */
    std::vector<std::pair<std::string, Rational>> get_scores() const {
        return scores;
    }
};

MsgPtr deserialize_message(const char *buffer, const size_t len_received, ReceiveInfo &rinfo);

} // namespace msg
} // namespace tga

#endif // TGA_MSG_CMB_H