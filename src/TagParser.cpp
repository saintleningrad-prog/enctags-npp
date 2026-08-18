#pragma once
#include <string>
#include <vector>

namespace TagParser {

    // A found ^^...^^ region in the text — may be encrypted or raw
    struct FoundTag {
        int startPos;        // Position of first ^ in ^^
        int endPos;           // Position after last ^ in ^^
        std::string payload;  // Everything between ^^ and ^^
        bool isEncrypted;     // true = "L1:base64...", false = raw plaintext
    };

    // Find ALL ^^...^^ regions in text, classified as encrypted or raw
    std::vector<FoundTag> FindAllTags(const std::string& text);

    // Find only encrypted tags (^^L1:...^^)
    std::vector<FoundTag> FindEncryptedTags(const std::string& text);

    // Find only raw (not yet encrypted) tags (^^plain text^^)
    std::vector<FoundTag> FindRawTags(const std::string& text);

    // Validate a tag payload format (starts with L1: or O1: etc.)
    bool IsValidPayload(const std::string& payload);

    // Check if a string (with surrounding ^^) is an encrypted tag
    bool IsEncryptedTag(const std::string& text);

    // Find the tag (encrypted or raw) that contains the given cursor position.
    // Returns index into the result vector, or -1 if cursor is not inside any tag.
    int FindTagAtPosition(const std::vector<FoundTag>& tags, int pos);
}
