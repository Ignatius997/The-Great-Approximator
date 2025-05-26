#include <memory>
#include <string>
#include <map>
#include <optional>
#include <regex>
#include <utility>

#include "Message.h"
#include "MessageCombinators.h"
#include "utils.h"
#include "args.h"
#include "Rational.h"

namespace tga {
namespace msg {

using tga::utils::ReceiveInfo;
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
 * @param exp_size Expected size of the coefficients vector.
 * @return std::optional<std::vector<Rational>> A vector of Rational numbers
 *         if extraction is successful, otherwise std::nullopt.
 */
std::optional<std::vector<Rational>> extract_coeffs(const std::string &msg_body,
                                                    ReceiveInfo &rinfo,
                                                    const size_t exp_size) {
    std::string full_regex = std::string("^") + rational_regex + R"((?: )" +
                             rational_regex + R"()*)\r\n$)";
    static const std::regex re(full_regex);
    std::smatch match;
    if (!std::regex_match(msg_body, match, re)) {
        rinfo.err = 1;
        return std::nullopt; // Invalid message format
    }

    // Extract rational numbers and store them in vector.
    std::vector<Rational> coeffs;
    static const std::regex num_re(rational_regex);
    auto numbers_begin = std::sregex_iterator(msg_body.begin(), msg_body.end(), num_re);
    auto numbers_end = std::sregex_iterator();

    for (auto it = numbers_begin; it != numbers_end; ++it) {
        coeffs.emplace_back(it->str());
        if (coeffs.size() > exp_size) {
            rinfo.err = 1; // Too many coefficients
            return std::nullopt;
        }
    }

    if (coeffs.size() != exp_size) {
        rinfo.err = 1; // Incorrect number of coefficients
        return std::nullopt;
    }

    return std::optional<std::vector<Rational>>(std::move(coeffs));
}

} // anonymous namespace

MsgPtr HelloMessage::deserialize(const std::string &msg_body,
                                            ReceiveInfo &rinfo) {
    std::string full_regex = std::string("^") + player_id_regex + R"(\r\n$)";
    static const std::regex re(full_regex); // $player_id\r\n
    std::smatch match;

    if (std::regex_match(msg_body, match, re)) {
        rinfo.err = 1;
        return nullptr; // Invalid message format.
    }

    std::string player_id = match[1].str();
    return std::make_unique<HelloMessage>(player_id);
}

// TODO Przetestować te regexy
MsgPtr CoeffMessage::deserialize(const std::string& msg_body, ReceiveInfo& rinfo) {
    auto coeffs = extract_coeffs(msg_body, rinfo, tga::args::server::N() + 1);
    return coeffs.has_value() ? std::make_unique<CoeffMessage>(std::move(*coeffs)) :
                                nullptr;
}

MsgPtr StateMessage::deserialize(const std::string& msg_body, ReceiveInfo& rinfo) {
    auto coeffs = extract_coeffs(msg_body, rinfo, tga::args::server::K() + 1);
    return coeffs.has_value() ? std::make_unique<StateMessage>(*coeffs) :
                                nullptr;
}

std::optional<std::pair<size_t, Rational>> PVMessage::deserialize_helper(
                                                    const std::string& msg_body,
                                                    ReceiveInfo& rinfo) {
    std::string regex_str = std::string(R"(^(\d+) )") + rational_regex + R"(\r\n$)";
    static const std::regex re(regex_str);
    std::smatch match;


    if (!std::regex_match(msg_body, match, re)) {
        rinfo.err = 1; // Invalid message format
        return std::nullopt;
    }

    size_t point = (size_t) std::stoi(match[1].str()); // Conversion is safe here.
    tga::rat::Rational value(match[2].str());

    if (point > tga::args::server::K()) {
        rinfo.err = 1; // Point out of range
        return std::nullopt;
    }

    if ((double) value < -5.0 || (double) value > 5.0) {
        rinfo.err = 1; // Value out of range
        return std::nullopt;
    }

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
    std::string full_regex = std::string("^") + player_id_regex + " " + rational_regex +
                            R"((?: )" + player_id_regex + " " + rational_regex + R"()*)\r\n$)";
    static const std::regex re(full_regex);

    if (!std::regex_match(msg_body, re)) {
        rinfo.err = 1;
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
            rinfo.err = 1;
            return nullptr;
        }
        last_id = player_id;

        results.at(player_id) = result; // Insert or update the score.
    }

    return std::make_unique<ScoringMessage>(std::move(results));
}

MsgPtr deserialize_message(const char *buffer,
                                            const size_t len_received,
                                            ReceiveInfo &rinfo) {
    std::string full_msg(buffer, static_cast<size_t>(len_received));
    
    const size_t first_space_idx = full_msg.find(' ');

    if (first_space_idx == std::string::npos) { // No space found.
        rinfo.msg_type = "UNKNOWN_MESSAGE_TYPE";
        return nullptr;
    }

    std::string msg_type = full_msg.substr(0, first_space_idx);
    auto it = deserializers.find(msg_type);
    
    if (it == deserializers.end()) {
        rinfo.msg_type = "UNKNOWN_MESSAGE_TYPE";
        return nullptr;
    }

    rinfo.msg_type = msg_type; // Set the message type in ReceiveInfo.
    std::string msg_body = full_msg.substr(first_space_idx + 1);
    auto deserializer = it->second;
    return deserializer(msg_body, rinfo);
}

} // namespace msg
} // namespace tga