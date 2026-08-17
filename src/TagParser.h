#pragma once
#include <string>
#include <vector>

namespace TagParser {

    // A found tag in the text
    struct FoundTag {
        int startPos;       // Position of first ^ in ^^
        int endPos;         // Position after last ^ in ^^
        std::string payload; // Everything between ^^ and ^^ (e.g. "L1:base64...")
    };

    // Find all ^^...^^ tags in text buffer
    std::vector<FoundTag> FindTags(const std::string& text);

    // Validate a tag payload format (starts with L1: or O1: etc.)
    bool IsValidPayload(const std::string& payload);

    // Check if selected text is already an encrypted tag
    bool IsEncryptedTag(const std::string& text);
}
