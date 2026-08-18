#pragma once
#include <string>
#include <vector>

// Tracks decrypted fragments currently shown as plaintext in the editor.
// Each fragment remembers which password decrypted it, so re-encryption
// on close/save can use the correct password per-fragment.

struct ManagedFragment {
    std::string originalTag;      // "^^L1:salt:ct:tag^^" — for rollback if unchanged
    std::string decryptedText;    // Text right after decryption (to detect user edits)
    std::string password;         // Password that decrypted this fragment
    int    startPos;              // Start position in current buffer
    int    length;                // Length of decrypted text in buffer
    bool   active;                // Still tracked (false = removed from tracking)
};

class FragmentRegistry {
public:
    FragmentRegistry();
    ~FragmentRegistry();

    void Add(int startPos, int length,
             const std::string& originalTag,
             const std::string& decryptedText,
             const std::string& password);

    void Remove(int index);
    void Clear();

    const std::vector<ManagedFragment>& GetAll() const;
    int Count() const;

    // Find fragment (active) at a given buffer position
    int FindAt(int pos) const;

    bool IsManaged(int pos) const;

private:
    std::vector<ManagedFragment> m_fragments;
};
