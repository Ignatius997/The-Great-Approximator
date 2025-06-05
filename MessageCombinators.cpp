#include <memory>
#include <string>
#include <map>
#include <optional>
#include <regex>
#include <utility>
#include <set>

#include "Message.h"
#include "MessageCombinators.h"
#include "netutils.h"
#include "args.h"
#include "Rational.h"
#include "config.h"

namespace tga {
namespace msg {

using tga::net::ReceiveInfo;
using tga::net::ReceivedDataStatus;
using tga::rat::Rational;

namespace {

/** Matches rational numbers with up to 7 digits after the dot. */
constexpr const char *rational_regex  = R"(-?\d+(?:\.\d{0,7})?)";
constexpr const char *player_id_regex = R"([A-Za-z0-9]+)";

using Deserializer = MsgPtr (*)(const std::string &msg_body, ReceiveInfo &rinfo);
std::map<std::string, Deserializer> deserializers = {
    {"HELLO",   &HelloMessage::deserialize},
    {"COEFF",   &CoeffMessage::deserialize},
    {"STATE",   &StateMessage::deserialize},
    {"PUT",     &PutMessage::deserialize},
    {"BAD_PUT", &BadPutMessage::deserialize},
    {"PENALTY", &PenaltyMessage::deserialize},
    {"SCORING", &ScoringMessage::deserialize}
};

/**
 * @brief Extracts coefficients from the message body.
 * 
 * @param msg_body The body of the message to extract coefficients from.
 * @param rinfo Reference to ReceiveInfo to store error information.
 * @return std::optional<std::vector<Rational>> A vector of Rational numbers
 *         if extraction is successful, otherwise std::nullopt.
 * 
 * @note This function assumes, that there is an appropriate
 * number of coefficients to extract. 
 */
std::optional<std::vector<Rational>> extract_coeffs(const std::string &msg_body,
                                                    ReceiveInfo &rinfo) {
    std::string full_regex = "^" + std::string(rational_regex) +
                         "(?: " + rational_regex + ")*\\r\\n$";
    static const std::regex re(full_regex);
    std::smatch match;
    if (!std::regex_match(msg_body, match, re)) {
        rinfo.err = ReceivedDataStatus::INVALID_FORMAT;
        return std::nullopt; // Invalid message format
    }

    // Extract rational numbers and store them in vector.
    std::vector<Rational> coeffs;
    static const std::regex num_re(rational_regex);
    auto numbers_begin = std::sregex_iterator(msg_body.begin(), msg_body.end(), num_re);
    auto numbers_end = std::sregex_iterator();

    for (auto it = numbers_begin; it != numbers_end; ++it) {
        coeffs.emplace_back(it->str());
    }

    return std::optional<std::vector<Rational>>(std::move(coeffs));
}

std::set <std::string> message_types_expected_by_server = {
    "HELLO", "PUT"
};

std::set <std::string> message_types_expected_by_client = {
    "COEFF", "STATE", "BAD_PUT", "PENALTY", "SCORING"
};

} // anonymous namespace

MsgPtr HelloMessage::deserialize(const std::string &msg_body,
                                            ReceiveInfo &rinfo) {
    std::string full_regex = std::string("^(") + player_id_regex + R"()\r\n$)";
    static const std::regex re(full_regex); // $player_id\r\n
    std::smatch match;

    if (!std::regex_match(msg_body, match, re)) {
        rinfo.err = ReceivedDataStatus::INVALID_FORMAT;
        return nullptr;
    }

    std::string player_id = match[1].str();
    return std::make_unique<HelloMessage>(player_id);
}

// TODO Przetestować te regexy
MsgPtr CoeffMessage::deserialize(const std::string& msg_body, ReceiveInfo& rinfo) {
    auto coeffs = extract_coeffs(msg_body, rinfo);
    return coeffs.has_value() ? std::make_unique<CoeffMessage>(std::move(*coeffs)) :
                                nullptr;
}

MsgPtr StateMessage::deserialize(const std::string& msg_body, ReceiveInfo& rinfo) {
    auto coeffs = extract_coeffs(msg_body, rinfo);
    return coeffs.has_value() ? std::make_unique<StateMessage>(*coeffs) :
                                nullptr;
}

std::optional<std::pair<size_t, Rational>> PVMessage::deserialize_helper(
                                                    const std::string& msg_body,
                                                    ReceiveInfo& rinfo) {
    std::string regex_str = std::string(R"(^(\d+) )") + "(" + rational_regex + ")" + R"(\r\n$)";
    static const std::regex re(regex_str);
    std::smatch match;

    if (!std::regex_match(msg_body, match, re)) {
        rinfo.err = ReceivedDataStatus::INVALID_FORMAT;
        return std::nullopt;
    }

    size_t point = (size_t) std::stoi(match[1].str()); // Conversion is safe here.
    tga::rat::Rational value(match[2].str());
    return std::make_pair(point, std::move(value));
}

MsgPtr PutMessage::deserialize(const std::string& msg_body, ReceiveInfo& rinfo) {
    auto pair_opt = PVMessage::deserialize_helper(msg_body, rinfo);
    if (!pair_opt.has_value()) return nullptr;
    return std::make_unique<PutMessage>(pair_opt->first,
                                    std::move(pair_opt->second));
}

MsgPtr BadPutMessage::deserialize(const std::string& msg_body, ReceiveInfo& rinfo) {
    auto pair_opt = PVMessage::deserialize_helper(msg_body, rinfo);
    if (!pair_opt.has_value()) return nullptr;
    return std::make_unique<BadPutMessage>(pair_opt->first,
                                        std::move(pair_opt->second));
}

MsgPtr PenaltyMessage::deserialize(const std::string& msg_body, ReceiveInfo& rinfo) {
    auto pair_opt = PVMessage::deserialize_helper(msg_body, rinfo);
    if (!pair_opt.has_value()) return nullptr;
    return std::make_unique<PenaltyMessage>(pair_opt->first,
                                        std::move(pair_opt->second));
}

MsgPtr ScoringMessage::deserialize(const std::string& msg_body, ReceiveInfo& rinfo) {
    std::string full_regex = "^" + std::string(player_id_regex) + " " + rational_regex +
                         "(?: " + player_id_regex + " " + rational_regex + ")*\\r\\n$";
    static const std::regex re(full_regex);

    if (!std::regex_match(msg_body, re)) {
        rinfo.err = ReceivedDataStatus::INVALID_FORMAT;
        return nullptr;
    }

    // Extract players ids and their scores.
    static const std::regex pair_re(std::string(player_id_regex) + " " + rational_regex);
    auto it = std::sregex_iterator(msg_body.begin(), msg_body.end(), pair_re);
    auto end = std::sregex_iterator();

    std::map<std::string, Rational> results;
    std::string last_id;

    for (; it != end; ++it) {
        std::string player_id = (*it)[1].str();
        Rational result((*it)[2].str());

        // Check lexicographical order of player IDs.
        if (!last_id.empty() && player_id <= last_id) {
            rinfo.err = ReceivedDataStatus::INVALID_VALUE;
            return nullptr;
        }
        last_id = player_id;

        results.insert({player_id, result});
    }

    return std::make_unique<ScoringMessage>(std::move(results));
}

/**
 * @brief Deserializes a message from a buffer into a message object.
 *
 * This function takes a raw buffer containing a serialized message, attempts to parse
 * the message type and its body, and then dispatches the body to the appropriate
 * deserializer function based on the message type. It also performs validation of the
 * message format and type, and sets error information in the provided ReceiveInfo
 * structure if deserialization fails.
 *
 * The function first extracts the message type (the substring before the first space).
 * It then checks if the message type is recognized and expected by the current side
 * (server or client). If the type is valid, it calls the corresponding deserializer
 * for the message body. If any step fails, the function sets an appropriate error
 * code in @p rinfo and returns nullptr.
 *
 * @param buffer Pointer to the buffer containing the serialized message.
 * @param msg_len Length of the message (with CRLF at the end).
 * @param rinfo Reference to a ReceiveInfo structure where error information will be stored.
 * @return MsgPtr A unique pointer to the deserialized message object on success,
 *         or nullptr if deserialization fails (with error details set in @p rinfo).
 *
 * @note The function expects the message to be in the format: "<TYPE> <BODY>",
 *       where <TYPE> is a recognized message type and <BODY> is the message content.
 * @note The function validates both the message type and the expected direction
 *       (server/client) for the message.
 * @note On failure, @p rinfo.err is set to one of the ReceivedDataStatus values
 *       indicating the reason for failure (e.g., INVALID_FORMAT, INVALID_TYPE).
 */
MsgPtr deserialize_message(const char *buffer,
                        const size_t msg_len,
                        ReceiveInfo &rinfo) {
    std::string full_msg(buffer, static_cast<size_t>(msg_len));    
    const size_t first_space_idx = full_msg.find(' ');

    if (first_space_idx == std::string::npos) { // No space found.
        rinfo.err = ReceivedDataStatus::INVALID_TYPE;
        return nullptr;
    }

    // Extract message type.
    std::string msg_type = full_msg.substr(0, first_space_idx);
    auto it = deserializers.find(msg_type);
    
    // Check if the message type is recognized.
    if (it == deserializers.end()) { // Unresolved message type.
        rinfo.err = ReceivedDataStatus::INVALID_TYPE;
        return nullptr;
    } else {
        // Check if the message type is expected by the this connection side.
        if (tga::config::server) {
            if (message_types_expected_by_server.find(msg_type) ==
                message_types_expected_by_server.end()) {
                rinfo.err = ReceivedDataStatus::INVALID_TYPE;
                return nullptr;
            }
        } else {
            if (message_types_expected_by_client.find(msg_type) ==
                message_types_expected_by_client.end()) {
                rinfo.err = ReceivedDataStatus::INVALID_TYPE;
                return nullptr;
            }
        }
    }

    std::string msg_body = full_msg.substr(first_space_idx + 1);
    auto deserializer = it->second;
    return deserializer(msg_body, rinfo);
}

} // namespace msg
} // namespace tga