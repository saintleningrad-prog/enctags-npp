#include "EncTagsEngine.h"
#include <sstream>
#include <algorithm>

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

namespace EncTagsEngine {

static const char b64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const std::vector<uint8_t>& data) {
    std::string out;
    int val = 0, valb = -6;
    for (size_t i = 0; i < data.size(); i++) {
        val = (val << 8) + data[i];
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        out.push_back(b64chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4)
        out.push_back('=');
    return out;
}

std::vector<uint8_t> Base64Decode(const std::string& encoded) {
    int lut[256];
    for (int i = 0; i < 256; i++) lut[i] = -1;
    for (int i = 0; i < 64; i++) lut[(unsigned char)b64chars[i]] = i;

    std::vector<uint8_t> out;
    int val = 0, valb = -8;
    for (size_t i = 0; i < encoded.size(); i++) {
        unsigned char c = (unsigned char)encoded[i];
        if (lut[c] == -1) break;
        val = (val << 6) + lut[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back((uint8_t)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::vector<uint8_t> RandomBytes(int count) {
    std::vector<uint8_t> buf(count);
    BCryptGenRandom(NULL, buf.data(), (ULONG)count, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return buf;
}

std::vector<uint8_t> DeriveKey(const std::string& password,
                                const std::vector<uint8_t>& salt) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    std::vector<uint8_t> derivedKey(KEY_SIZE);

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &hAlg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);

    if (!NT_SUCCESS(status)) return {};

    status = BCryptDeriveKeyPBKDF2(
        hAlg,
        (PUCHAR)password.data(), (ULONG)password.size(),
        (PUCHAR)salt.data(), (ULONG)salt.size(),
        PBKDF2_ITERATIONS,
        derivedKey.data(), (ULONG)derivedKey.size(),
        0);

    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!NT_SUCCESS(status)) return {};
    return derivedKey;
}

bool AesGcmEncrypt(const std::vector<uint8_t>& key,
                   const std::vector<uint8_t>& nonce,
                   const std::vector<uint8_t>& plaintext,
                   std::vector<uint8_t>& ciphertext,
                   std::vector<uint8_t>& authTag) {

    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_KEY_HANDLE hKey = NULL;
    NTSTATUS status;
    bool ok = false;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!NT_SUCCESS(status)) return false;

    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
        (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!NT_SUCCESS(status)) { BCryptCloseAlgorithmProvider(hAlg, 0); return false; }

    status = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0,
        (PUCHAR)key.data(), (ULONG)key.size(), 0);
    if (!NT_SUCCESS(status)) { BCryptCloseAlgorithmProvider(hAlg, 0); return false; }

    {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = (PUCHAR)nonce.data();
        authInfo.cbNonce = (ULONG)nonce.size();

        authTag.resize(TAG_SIZE);
        authInfo.pbTag = authTag.data();
        authInfo.cbTag = (ULONG)authTag.size();

        ULONG cbResult = 0;
        ciphertext.resize(plaintext.size());

        status = BCryptEncrypt(hKey,
            (PUCHAR)plaintext.data(), (ULONG)plaintext.size(),
            &authInfo,
            NULL, 0,
            ciphertext.data(), (ULONG)ciphertext.size(),
            &cbResult, 0);

        if (NT_SUCCESS(status)) {
            ciphertext.resize(cbResult);
            ok = true;
        }
    }

    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

bool AesGcmDecrypt(const std::vector<uint8_t>& key,
                   const std::vector<uint8_t>& nonce,
                   const std::vector<uint8_t>& ciphertext,
                   const std::vector<uint8_t>& authTag,
                   std::vector<uint8_t>& plaintext) {

    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_KEY_HANDLE hKey = NULL;
    NTSTATUS status;
    bool ok = false;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!NT_SUCCESS(status)) return false;

    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
        (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!NT_SUCCESS(status)) { BCryptCloseAlgorithmProvider(hAlg, 0); return false; }

    status = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0,
        (PUCHAR)key.data(), (ULONG)key.size(), 0);
    if (!NT_SUCCESS(status)) { BCryptCloseAlgorithmProvider(hAlg, 0); return false; }

    {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = (PUCHAR)nonce.data();
        authInfo.cbNonce = (ULONG)nonce.size();
        authInfo.pbTag = (PUCHAR)authTag.data();
        authInfo.cbTag = (ULONG)authTag.size();

        ULONG cbResult = 0;
        plaintext.resize(ciphertext.size());

        status = BCryptDecrypt(hKey,
            (PUCHAR)ciphertext.data(), (ULONG)ciphertext.size(),
            &authInfo,
            NULL, 0,
            plaintext.data(), (ULONG)plaintext.size(),
            &cbResult, 0);

        if (NT_SUCCESS(status)) {
            plaintext.resize(cbResult);
            ok = true;
        }
    }

    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

EncryptResult Encrypt(const std::string& plaintext, const std::string& password) {
    EncryptResult result;
    result.success = false;

    auto salt  = RandomBytes(SALT_SIZE);
    auto nonce = RandomBytes(NONCE_SIZE);

    auto key = DeriveKey(password, salt);
    if (key.empty()) {
        result.error = "Key derivation failed";
        return result;
    }

    std::vector<uint8_t> ct, tag;
    std::vector<uint8_t> pt(plaintext.begin(), plaintext.end());

    if (!AesGcmEncrypt(key, nonce, pt, ct, tag)) {
        result.error = "AES-GCM encryption failed";
        return result;
    }

    std::vector<uint8_t> blob;
    blob.insert(blob.end(), salt.begin(),  salt.end());
    blob.insert(blob.end(), nonce.begin(), nonce.end());
    blob.insert(blob.end(), ct.begin(),    ct.end());
    blob.insert(blob.end(), tag.begin(),   tag.end());

    std::string b64 = Base64Encode(blob);

    result.tag = "^^L1:" + b64 + "^^";
    result.success = true;
    return result;
}

DecryptResult Decrypt(const std::string& tagPayload, const std::string& password) {
    DecryptResult result;
    result.success = false;

    if (tagPayload.size() < 4 || tagPayload[0] != 'L' || tagPayload[1] != '1' || tagPayload[2] != ':') {
        result.error = "Unknown tag format";
        return result;
    }

    std::string b64data = tagPayload.substr(3);
    auto blob = Base64Decode(b64data);

    if (blob.size() < (size_t)(SALT_SIZE + NONCE_SIZE + TAG_SIZE + 1)) {
        result.error = "Tag data too short";
        return result;
    }

    std::vector<uint8_t> salt(blob.begin(), blob.begin() + SALT_SIZE);
    std::vector<uint8_t> nonce(blob.begin() + SALT_SIZE, blob.begin() + SALT_SIZE + NONCE_SIZE);
    std::vector<uint8_t> authTag(blob.end() - TAG_SIZE, blob.end());
    std::vector<uint8_t> ct(blob.begin() + SALT_SIZE + NONCE_SIZE, blob.end() - TAG_SIZE);

    auto key = DeriveKey(password, salt);
    if (key.empty()) {
        result.error = "Key derivation failed";
        return result;
    }

    std::vector<uint8_t> pt;
    if (!AesGcmDecrypt(key, nonce, ct, authTag, pt)) {
        result.error = "Wrong password or corrupted data";
        return result;
    }

    result.plaintext = std::string(pt.begin(), pt.end());
    result.success = true;
    return result;
}

} // namespace EncTagsEngine
