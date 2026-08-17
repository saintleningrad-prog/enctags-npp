#include "TagParser.h"
#include <regex>

namespace TagParser {

std::vector<FoundTag> FindTags(const std::string& text) {
    std::vector<FoundTag> tags;

    const std::string OPEN_DELIM  = "^^";
    const std::string CLOSE_DELIM = "^^";

    size_t searchFrom = 0;

    while (searchFrom < text.size()) {
        // Find opening ^^
        size_t openPos = text.find(OPEN_DELIM, searchFrom);
        if (openPos == std::string::npos)
            break;

        size_t payloadStart = openPos + OPEN_DELIM.size();

        // Find closing ^^ after the opening
        size_t closePos = text.find(CLOSE_DELIM, payloadStart);
        if (closePos == std::string::npos)
            break;

        // Extract payload
        std::string payload = text.substr(payloadStart, closePos - payloadStart);

        // Validate: payload must not be empty and must start with a known mode
        if (!payload.empty() && IsValidPayload(payload)) {
            FoundTag tag;
            tag.startPos = (int)openPos;
            tag.endPos   = (int)(closePos + CLOSE_DELIM.size());
            tag.payload  = payload;
            tags.push_back(tag);
        }

        // Continue searching after this tag
        searchFrom = closePos + CLOSE_DELIM.size();
    }

    return tags;
}

bool IsValidPayload(const std::string& payload) {
    // Valid formats: L1:..., O1:..., Q1:...
    if (payload.size() < 4) return false;

    char mode = payload[0];
    char ver  = payload[1];
    char sep  = payload[2];

    // Mode: L(ocal), O(PRF), Q(R)
    if (mode != 'L' && mode != 'O' && mode != 'Q') return false;

    // Version: currently only '1'
    if (ver != '1') return false;

    // Separator
    if (sep != ':') return false;

    // Rest should be base64-valid characters (A-Za-z0-9+/=)
    for (size_t i = 3; i < payload.size(); i++) {
        char c = payload[i];
        bool valid = (c >= 'A' && c <= 'Z') ||
                     (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') ||
                     c == '+' || c == '/' || c == '=';
        if (!valid) return false;
    }

    return true;
}

bool IsEncryptedTag(const std::string& text) {
    // Check if the entire text is a ^^...^^ tag
    if (text.size() < 8) return false;  // minimum: ^^L1:x^^
    if (text.substr(0, 2) != "^^") return false;
    if (text.substr(text.size() - 2) != "^^") return false;

    std::string payload = text.substr(2, text.size() - 4);
    return IsValidPayload(payload);
}

} // namespace TagParser
