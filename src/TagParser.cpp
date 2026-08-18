#include "TagParser.h"

namespace TagParser {

static const std::string DELIM = "^^";

std::vector<FoundTag> FindAllTags(const std::string& text) {
    std::vector<FoundTag> tags;
    size_t searchFrom = 0;

    while (searchFrom < text.size()) {
        size_t openPos = text.find(DELIM, searchFrom);
        if (openPos == std::string::npos) break;

        size_t payloadStart = openPos + DELIM.size();
        size_t closePos = text.find(DELIM, payloadStart);
        if (closePos == std::string::npos) break;

        std::string payload = text.substr(payloadStart, closePos - payloadStart);

        // Skip empty tags (^^^^ with nothing between)
        if (!payload.empty()) {
            FoundTag tag;
            tag.startPos    = (int)openPos;
            tag.endPos      = (int)(closePos + DELIM.size());
            tag.payload     = payload;
            tag.isEncrypted = IsValidPayload(payload);
            tags.push_back(tag);
        }

        searchFrom = closePos + DELIM.size();
    }

    return tags;
}

std::vector<FoundTag> FindEncryptedTags(const std::string& text) {
    auto all = FindAllTags(text);
    std::vector<FoundTag> result;
    for (auto& t : all)
        if (t.isEncrypted) result.push_back(t);
    return result;
}

std::vector<FoundTag> FindRawTags(const std::string& text) {
    auto all = FindAllTags(text);
    std::vector<FoundTag> result;
    for (auto& t : all)
        if (!t.isEncrypted) result.push_back(t);
    return result;
}

bool IsValidPayload(const std::string& payload) {
    if (payload.size() < 4) return false;

    char mode = payload[0];
    char ver  = payload[1];
    char sep  = payload[2];

    if (mode != 'L' && mode != 'O' && mode != 'Q') return false;
    if (ver != '1') return false;
    if (sep != ':') return false;

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
    if (text.size() < 8) return false;
    if (text.substr(0, 2) != "^^") return false;
    if (text.substr(text.size() - 2) != "^^") return false;

    std::string payload = text.substr(2, text.size() - 4);
    return IsValidPayload(payload);
}

int FindTagAtPosition(const std::vector<FoundTag>& tags, int pos) {
    for (int i = 0; i < (int)tags.size(); i++) {
        if (pos >= tags[i].startPos && pos <= tags[i].endPos) {
            return i;
        }
    }
    return -1;
}

} // namespace TagParser
