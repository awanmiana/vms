#pragma once

// P1-12 deterministic voice leg. A speech recognizer is an interchangeable
// upstream source; this pure policy maps its language-tagged transcript to the
// existing typed command envelope. It performs no audio capture and no AI/NLU.

#include <string>

#include "command/CommandRegistry.h"

namespace vms::command {

enum class VoiceStatus {
    Matched,
    UnsupportedLanguage,
    LowConfidence,
    Ambiguous,
    NoMatch
};

const char* VoiceStatusName(VoiceStatus status);

struct VoiceCommand {
    VoiceStatus status = VoiceStatus::NoMatch;
    std::string commandId;
    Args args;
    std::string reason;
    explicit operator bool() const { return status == VoiceStatus::Matched; }
};

class VoiceCommandMapper {
public:
    explicit VoiceCommandMapper(double minimumConfidence = 0.80)
        : minimumConfidence_(minimumConfidence) {}

    VoiceCommand map(const std::string& transcript,
                     const std::string& languageTag,
                     double confidence) const;

    double minimumConfidence() const { return minimumConfidence_; }

private:
    double minimumConfidence_ = 0.80;
};

} // namespace vms::command
