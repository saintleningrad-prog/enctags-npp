#include "FragmentRegistry.h"

FragmentRegistry::FragmentRegistry() {}
FragmentRegistry::~FragmentRegistry() {}

void FragmentRegistry::Add(int startPos, int length,
                           const std::string& originalTag,
                           const std::string& decryptedText,
                           const std::string& password) {
    ManagedFragment frag;
    frag.originalTag   = originalTag;
    frag.decryptedText = decryptedText;
    frag.password      = password;
    frag.startPos       = startPos;
    frag.length         = length;
    frag.active         = true;
    m_fragments.push_back(frag);
}

void FragmentRegistry::Remove(int index) {
    if (index >= 0 && index < (int)m_fragments.size())
        m_fragments[index].active = false;
}

void FragmentRegistry::Clear() {
    m_fragments.clear();
}

const std::vector<ManagedFragment>& FragmentRegistry::GetAll() const {
    return m_fragments;
}

int FragmentRegistry::Count() const {
    int c = 0;
    for (auto& f : m_fragments) if (f.active) c++;
    return c;
}

int FragmentRegistry::FindAt(int pos) const {
    for (int i = 0; i < (int)m_fragments.size(); i++) {
        if (!m_fragments[i].active) continue;
        if (pos >= m_fragments[i].startPos &&
            pos <= m_fragments[i].startPos + m_fragments[i].length) {
            return i;
        }
    }
    return -1;
}

bool FragmentRegistry::IsManaged(int pos) const {
    return FindAt(pos) >= 0;
}
