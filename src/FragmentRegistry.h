#pragma once
#include <string>
#include <vector>
#include <windows.h>

// Tracks decrypted fragments in the editor buffer.
// When a tag is decrypted and replaced with plaintext,
// the registry remembers the original encrypted tag so it
// can re-encrypt on save.

struct ManagedFragment {
    int    indicatorId;           // Scintilla indicator slot
    std::string originalTag;     // "^^L1:salt:ct:tag^^" — for rollback
    std::string decryptedText;   // Original decrypted text (to detect edits)
    int    startPos;             // Start position in buffer
    int    length;               // Length of decrypted text in buffer
    bool   active;               // Still managed (false = user removed encryption)
};

class FragmentRegistry {
public:
    FragmentRegistry();
    ~FragmentRegistry();

    // Register a newly decrypted fragment
    void Add(int startPos, int length,
             const std::string& originalTag,
             const std::string& decryptedText);

    // Remove a fragment (user chose "remove encryption")
    void Remove(int index);

    // Clear all fragments (file closed)
    void Clear();

    // Get all active fragments
    const std::vector<ManagedFragment>& GetAll() const;

    // Number of active fragments
    int Count() const;

    // Find fragment at a given buffer position
    int FindAt(int pos) const;

    // Update positions after text insertion/deletion
    void AdjustPositions(int changePos, int changeLength);

    // Check if a position is inside a managed fragment
    bool IsManaged(int pos) const;

private:
    std::vector<ManagedFragment> m_fragments;
};
