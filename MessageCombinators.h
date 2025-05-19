#ifndef TGA_MSG_CMB_H
#define TGA_MSG_CMB_H

#include <string>
#include <vector>
#include <map>

#include "Message.h"
#include "io.h"
#include "Rational.h"

namespace tga {
namespace msg {

// ==== Establishing Connection Messages ====

class HelloMessage : public Message {
private:
    const std::string player_id;

protected:
    std::string messageType() const override final {
        return "HELLO";
    }

    std::string messageContent() const override {
        return player_id;
    }

public:
    explicit HelloMessage(const std::string& id) : player_id(id) {}
};

class CoeffMessage : public Message {
protected:
    std::string messageType() const override final {
        return "COEFF";
    }

    std::string messageContent() const override {
        return tga::io::read_coeffs();
    }

public:
    CoeffMessage() = default;
};

// ==== Game Messages ====

class StateMessage : public Message {
private:
    // TODO Is this correct? What if we change this approx?
    // NOTE Change K to the actual size of the approximation
    const std::vector<tga::rat::Rational> &approx; // Approximation of the function. approx.size() == K.

protected:
    std::string messageType() const override final {
        return "STATE";
    }

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

public:
    StateMessage(const std::vector<tga::rat::Rational> &approx) : approx(approx) {}
};


// Point Value Messages

// NOTE Przetestować dziedziczenie
/**
 * @brief Point-Value-Message. Abstract base class for messages that contain a point and a value.
 */
class PVMessage : public Message {
private:
    const int point;
    const tga::rat::Rational value;

protected:
    std::string messageContent() const override final {
        return std::to_string(point) + " " + static_cast<std::string>(value);
    }

public:
    PVMessage(int p, tga::rat::Rational v) : point(p), value(v) {}
};

class PutMessage : public PVMessage {
private:
    const int point;
    const tga::rat::Rational value;

protected:
    std::string messageType() const override final {
        return "PUT";
    }

public:
    using PVMessage::PVMessage; // Inherit constructor
};

class BadPutMessage : public PVMessage {
protected:
    std::string messageType() const override final {
        return "BAD_PUT";
    }
    
public:
    using PVMessage::PVMessage; // Inherit constructor
};

class PenaltyMessage : public PVMessage {
protected:
    std::string messageType() const override final {
        return "PENALTY";
    }
public:
    using PVMessage::PVMessage; // Inherit constructor
};

// ==== Endgame Messages ====

class ScoringMessage : public Message {
private:
    const std::map<std::string, tga::rat::Rational> &scores;

protected:
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

public:
    ScoringMessage(const std::map<std::string, tga::rat::Rational> &scores) : scores(scores) {}
};

} // namespace msg
} // namespace tga

#endif // TGA_MSG_CMB_H