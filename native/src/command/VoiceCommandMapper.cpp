#include "command/VoiceCommandMapper.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

namespace vms::command {

const char* VoiceStatusName(VoiceStatus status) {
    switch (status) {
        case VoiceStatus::Matched:             return "matched";
        case VoiceStatus::UnsupportedLanguage: return "unsupported-language";
        case VoiceStatus::LowConfidence:       return "low-confidence";
        case VoiceStatus::Ambiguous:           return "ambiguous";
        case VoiceStatus::NoMatch:             return "no-match";
    }
    return "no-match";
}

namespace {

VoiceCommand refused(VoiceStatus status, const std::string& reason) {
    VoiceCommand out;
    out.status = status;
    out.reason = reason;
    return out;
}

VoiceCommand matched(const std::string& id, Args args = {}) {
    VoiceCommand out;
    out.status = VoiceStatus::Matched;
    out.commandId = id;
    out.args = std::move(args);
    return out;
}

std::string lowerAscii(std::string value) {
    for (char& ch : value)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

std::vector<std::string> tokenize(const std::string& transcript) {
    std::string normalized;
    normalized.reserve(transcript.size());
    for (unsigned char raw : transcript) {
        const char ch = static_cast<char>(raw);
        if (std::isalnum(raw) || ch == '.' || ch == '-' || ch == '_')
            normalized += static_cast<char>(std::tolower(raw));
        else
            normalized += ' ';
    }
    std::istringstream input(normalized);
    std::vector<std::string> tokens;
    for (std::string token; input >> token;) tokens.push_back(token);
    while (!tokens.empty() && (tokens.front() == "please" ||
                               tokens.front() == "vms"))
        tokens.erase(tokens.begin());
    return tokens;
}

bool integerToken(const std::string& token, std::int64_t& out) {
    static const std::map<std::string, std::int64_t> words = {
        {"zero", 0}, {"one", 1}, {"two", 2}, {"three", 3},
        {"four", 4}, {"five", 5}, {"six", 6}, {"seven", 7},
        {"eight", 8}, {"nine", 9}, {"ten", 10}, {"eleven", 11},
        {"twelve", 12}, {"thirteen", 13}, {"fourteen", 14},
        {"fifteen", 15}, {"sixteen", 16}, {"twenty-five", 25},
        {"thirty", 30}, {"thirty-six", 36}, {"forty-nine", 49},
        {"sixty", 60}, {"sixty-four", 64}
    };
    const auto word = words.find(token);
    if (word != words.end()) {
        out = word->second;
        return true;
    }
    char* end = nullptr;
    const long long value = std::strtoll(token.c_str(), &end, 10);
    if (!end || *end != '\0') return false;
    out = static_cast<std::int64_t>(value);
    return true;
}

bool numberToken(const std::string& token, double& out) {
    std::int64_t integer = 0;
    if (integerToken(token, integer)) {
        out = static_cast<double>(integer);
        return true;
    }
    char* end = nullptr;
    const double value = std::strtod(token.c_str(), &end);
    if (!end || *end != '\0' || !std::isfinite(value)) return false;
    out = value;
    return true;
}

bool onOff(const std::string& token, bool& out) {
    if (token == "on" || token == "enable" || token == "enabled") {
        out = true;
        return true;
    }
    if (token == "off" || token == "disable" || token == "disabled") {
        out = false;
        return true;
    }
    return false;
}

bool containsOr(const std::vector<std::string>& tokens) {
    return std::find(tokens.begin(), tokens.end(), "or") != tokens.end();
}

} // namespace

VoiceCommand VoiceCommandMapper::map(const std::string& transcript,
                                     const std::string& languageTag,
                                     double confidence) const {
    const std::string language = lowerAscii(languageTag);
    // Only grammar tags actually covered by this release are accepted. Do
    // not treat an arbitrary "en-*" string as BCP 47 or as tested grammar.
    if (!(language == "en" || language == "en-us" || language == "en-gb"))
        return refused(VoiceStatus::UnsupportedLanguage,
                       "no tested deterministic grammar for language '" +
                           languageTag + "'");
    if (!std::isfinite(confidence) || confidence < minimumConfidence_ ||
        confidence > 1.0)
        return refused(VoiceStatus::LowConfidence,
                       "recognition confidence is outside policy range");

    std::vector<std::string> t = tokenize(transcript);
    if (t.empty()) return refused(VoiceStatus::NoMatch, "empty transcript");
    if (containsOr(t))
        return refused(VoiceStatus::Ambiguous,
                       "alternatives are not executable voice commands");

    // A spoken confirmation is deliberately discarded. Voice may request a
    // dangerous action, but CommandRegistry still receives confirmed=false.
    if (!t.empty() && t.back() == "confirm") t.pop_back();

    std::int64_t integer = 0;
    double number = 0.0;
    bool flag = false;

    if (t.size() == 2 && t[0] == "focus" && integerToken(t[1], integer))
        return matched("workspace.focus", {{"tile", integer}});
    if (t.size() == 3 && t[0] == "focus" &&
        (t[1] == "camera" || t[1] == "tile") &&
        integerToken(t[2], integer))
        return matched("workspace.focus", {{"tile", integer}});
    if (t.size() == 5 && t[0] == "set" &&
        (t[1] == "camera" || t[1] == "tile") &&
        integerToken(t[2], integer) && t[3] == "quality")
        return matched("workspace.quality",
                       {{"tile", integer}, {"tier", t[4]}});
    if ((t.size() == 2 && t[0] == "layout" &&
         integerToken(t[1], integer)) ||
        (t.size() == 3 && t[0] == "set" && t[1] == "layout" &&
         integerToken(t[2], integer)))
        return matched("workspace.layout", {{"count", integer}});
    if (t.size() == 2 && t[0] == "sweep" && onOff(t[1], flag))
        return matched("workspace.sweep", {{"on", flag}});
    if (t.size() == 3 && t[0] == "auto" && t[1] == "sweep" &&
        onOff(t[2], flag))
        return matched("workspace.sweep", {{"on", flag}});
    if (t.size() == 2 && t[0] == "spatial" && onOff(t[1], flag))
        return matched("workspace.spatial", {{"on", flag}});

    if ((t.size() == 3 || t.size() == 4) && t[0] == "start" &&
        t[1] == "replay" && integerToken(t[2], integer) &&
        (t.size() == 3 || t[3] == "seconds"))
        return matched("replay.start", {{"seconds", integer}});
    if (t == std::vector<std::string>({"return", "to", "live"}))
        return matched("replay.live");
    if (t == std::vector<std::string>({"play", "playback"}))
        return matched("playback.play");
    if (t == std::vector<std::string>({"pause", "playback"}))
        return matched("playback.pause");
    if (t.size() == 4 && t[0] == "set" && t[1] == "playback" &&
        t[2] == "speed" && numberToken(t[3], number))
        return matched("playback.speed", {{"rate", number}});
    if (t.size() == 4 && t[0] == "seek" && t[1] == "playback" &&
        t[3] == "percent" && numberToken(t[2], number))
        return matched("playback.seek", {{"position", number / 100.0}});

    if (t.size() == 3 && t[1] == "alarm" &&
        integerToken(t[2], integer)) {
        if (t[0] == "acknowledge")
            return matched("alarm.ack", {{"id", integer}});
        if (t[0] == "escalate")
            return matched("alarm.escalate", {{"id", integer}});
        if (t[0] == "clear")
            return matched("alarm.clear", {{"id", integer}});
    }

    if (t.size() == 3 && t[1] == "device") {
        if (t[0] == "detach")
            return matched("device.detach", {{"id", t[2]}, {"detached", true}});
        if (t[0] == "attach")
            return matched("device.detach", {{"id", t[2]}, {"detached", false}});
        if (t[0] == "remove")
            return matched("device.remove", {{"id", t[2]}});
    }

    return refused(VoiceStatus::NoMatch,
                   "transcript does not match the active deterministic grammar");
}

} // namespace vms::command
