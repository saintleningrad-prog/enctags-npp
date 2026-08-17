#include "FragmentRegistry.h"
#include <algorithm>

FragmentRegistry::FragmentRegistry() {}
FragmentRegistry::~FragmentRegistry() {}

void FragmentRegistry::Add(int startPos, int length,
                           const std::string& originalTag,
                           const std::string& decryptedText) {
    ManagedFragment frag;
    frag.indicatorId  = 0;
    frag.originalTag  = originalTag;
    frag.decryptedText = decryptedText;
    frag.startPos     = startPos;
    frag.length       = length;
    frag.active       = true;
    m_fragments.push_back(frag);
}

void FragmentRegistry::Remove(int index) {
    if (index >= 0 && index < (int)m_fragments.size()) {
        m_fragments[index].active = false;
    }
}

void FragmentRegistry::Clear() {
    m_fragments.clear();
}

const std::vector<ManagedFragment>& FragmentRegistry::GetAll() const {
    return m_fragments;
}

int FragmentRegistry::Count() const {
    int c = 0;
    for (auto& f : m_fragments)
        if (f.active) c++;
    return c;
}

int FragmentRegistry::FindAt(int pos) const {
    for (int i = 0; i < (int)m_fragments.size(); i++) {
        if (!m_fragments[i].active) continue;
        if (pos >= m_fragments[i].startPos &&
            pos < m_fragments[i].startPos + m_fragments[i].length) {
            return i;
        }
    }
    return -1;
}

void FragmentRegistry::AdjustPositions(int changePos, int changeLength) {
    // changeLength > 0: text inserted at changePos
    // changeLength < 0: text deleted at changePos
    for (auto& f : m_fragments) {
        if (!f.active) continue;

        if (changePos <= f.startPos) {
            // Change is before this fragment — shift position
            f.startPos += changeLength;
        } else if (changePos < f.startPos + f.length) {
            // Change is inside this fragment — adjust length
            f.length += changeLength;
        }
        // Change is after this fragment — no adjustment needed
    }
}

bool FragmentRegistry::IsManaged(int pos) const {
    return FindAt(pos) >= 0;
}
