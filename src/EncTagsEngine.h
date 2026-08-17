#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

// EncTags Crypto Engine
// Uses Windows BCrypt API — AES-256-GCM + PBKDF2-SHA256
// Zero external dependencies

namespace EncTagsEngine {

    // Salt size: 16 bytes
    constexpr int SALT_SIZE = 16;
    // AES-256 key: 32 bytes
    constexpr int KEY_SIZE = 32;
    // GCM Nonce: 12 bytes
    constexpr int NONCE_SIZE = 12;
    // GCM Auth Tag: 16 bytes
    constexpr int TAG_SIZE = 16;
    // PBKDF2 iterations
    constexpr int PBKDF2_ITERATIONS = 100000;

    // Result of encryption
    struct EncryptResult {
        bool success;
        std::string tag;      // Full ^^...^^ tag ready to insert
        std::string error;
    };

    // Result of decryption
    struct DecryptResult {
        bool success;
        std::string plaintext;
        std::string error;
    };

    // Encrypt plaintext with password, return ^^L1:salt:ct:tag^^ formatted string
    EncryptResult Encrypt(const std::string& plaintext, const std::string& password);

    // Decrypt a tag payload (everything between ^^ ^^), return plaintext
    DecryptResult Decrypt(const std::string& tagPayload, const std::string& password);

    // Base64 encode/decode
    std::string Base64Encode(const std::vector<uint8_t>& data);
    std::vector<uint8_t> Base64Decode(const std::string& encoded);

    // Generate random bytes
    std::vector<uint8_t> RandomBytes(int count);

    // Derive AES key from password + salt using PBKDF2-SHA256
    std::vector<uint8_t> DeriveKey(const std::string& password,
                                    const std::vector<uint8_t>& salt);

    // AES-256-GCM encrypt
    bool AesGcmEncrypt(const std::vector<uint8_t>& key,
                       const std::vector<uint8_t>& nonce,
                       const std::vector<uint8_t>& plaintext,
                       std::vector<uint8_t>& ciphertext,
                       std::vector<uint8_t>& authTag);

    // AES-256-GCM decrypt
    bool AesGcmDecrypt(const std::vector<uint8_t>& key,
                       const std::vector<uint8_t>& nonce,
                       const std::vector<uint8_t>& ciphertext,
                       const std::vector<uint8_t>& authTag,
                       std::vector<uint8_t>& plaintext);
}
