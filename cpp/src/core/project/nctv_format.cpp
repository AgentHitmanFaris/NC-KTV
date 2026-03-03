/*
 * NC-KTV Core — NCTV Binary Format Implementation
 * AES-256-GCM encrypted project files with zstd compression
 */

#include "nctv_format.h"
#include "project.h"

#include <QFile>
#include <QDataStream>
#include <QRandomGenerator>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <zstd.h>
#include <cstring>
#include <stdexcept>

namespace ncktv {

static const QByteArray FIXED_SALT = "NC-KTV-2025-SECURE-CONTAINER";

// ─── Key Derivation ──────────────────────────────────────────────────────────

QByteArray NCTVFormat::deriveKey(const QByteArray& salt) {
    QByteArray key(32, '\0');   // 256-bit key

    if (PKCS5_PBKDF2_HMAC(
            FIXED_SALT.constData(), FIXED_SALT.size(),
            reinterpret_cast<const unsigned char*>(salt.constData()), salt.size(),
            100000,             // iterations
            EVP_sha256(),
            32,
            reinterpret_cast<unsigned char*>(key.data())) != 1)
    {
        throw std::runtime_error("PBKDF2 key derivation failed");
    }

    return key;
}

// ─── AES-256-GCM ─────────────────────────────────────────────────────────────

QByteArray NCTVFormat::encrypt(const QByteArray& plaintext, const QByteArray& key,
                                const QByteArray& iv, QByteArray& tag)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("Failed to create cipher context");

    QByteArray ciphertext(plaintext.size() + 16, '\0');
    int outLen = 0, finalLen = 0;

    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                       reinterpret_cast<const unsigned char*>(key.constData()),
                       reinterpret_cast<const unsigned char*>(iv.constData()));

    EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()), &outLen,
                      reinterpret_cast<const unsigned char*>(plaintext.constData()),
                      plaintext.size());

    EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + outLen, &finalLen);

    tag.resize(16);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16,
                        reinterpret_cast<unsigned char*>(tag.data()));

    ciphertext.resize(outLen + finalLen);
    EVP_CIPHER_CTX_free(ctx);
    return ciphertext;
}

QByteArray NCTVFormat::decrypt(const QByteArray& ciphertext, const QByteArray& key,
                                const QByteArray& iv, const QByteArray& tag)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("Failed to create cipher context");

    QByteArray plaintext(ciphertext.size(), '\0');
    int outLen = 0, finalLen = 0;

    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                       reinterpret_cast<const unsigned char*>(key.constData()),
                       reinterpret_cast<const unsigned char*>(iv.constData()));

    EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()), &outLen,
                      reinterpret_cast<const unsigned char*>(ciphertext.constData()),
                      ciphertext.size());

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16,
                        const_cast<unsigned char*>(
                            reinterpret_cast<const unsigned char*>(tag.constData())));

    int ret = EVP_DecryptFinal_ex(ctx,
                                  reinterpret_cast<unsigned char*>(plaintext.data()) + outLen,
                                  &finalLen);
    EVP_CIPHER_CTX_free(ctx);

    if (ret <= 0)
        throw std::runtime_error("NCTV decryption failed: authentication tag mismatch");

    plaintext.resize(outLen + finalLen);
    return plaintext;
}

// ─── Pack ────────────────────────────────────────────────────────────────────

void NCTVFormat::pack(const Project& project, const QString& outputPath) {
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly))
        throw std::runtime_error("Cannot open output file: " + outputPath.toStdString());

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    // Write magic
    out.writeRawData(MAGIC, 4);

    // Write version
    out << VERSION;

    // Generate random salt and IV
    QByteArray salt(16, '\0');
    QByteArray iv(12, '\0');
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(salt.data()), 4);
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(iv.data()), 3);

    out.writeRawData(salt.constData(), 16);
    out.writeRawData(iv.constData(), 12);

    // Derive key
    QByteArray key = deriveKey(salt);

    // Serialize project JSON and compress with zstd
    std::string jsonStr = project.toJson().dump();
    QByteArray jsonData = QByteArray::fromStdString(jsonStr);

    size_t compBound = ZSTD_compressBound(jsonData.size());
    QByteArray compressed(static_cast<int>(compBound), '\0');
    size_t compSize = ZSTD_compress(compressed.data(), compBound,
                                     jsonData.constData(), jsonData.size(), 9);
    if (ZSTD_isError(compSize))
        throw std::runtime_error("ZSTD compression failed");
    compressed.resize(static_cast<int>(compSize));

    // Encrypt compressed payload
    QByteArray tag;
    QByteArray ciphertext = encrypt(compressed, key, iv, tag);

    // Write payload size + ciphertext
    uint32_t payloadSize = static_cast<uint32_t>(ciphertext.size());
    out << payloadSize;
    out.writeRawData(ciphertext.constData(), ciphertext.size());

    // Write GCM tag
    out.writeRawData(tag.constData(), 16);
}

// ─── Unpack ──────────────────────────────────────────────────────────────────

Project NCTVFormat::unpack(const QString& inputPath) {
    QFile file(inputPath);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("Cannot open file: " + inputPath.toStdString());

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    // Read & verify magic
    char magic[4];
    in.readRawData(magic, 4);
    if (std::memcmp(magic, MAGIC, 4) != 0)
        throw std::runtime_error("Invalid NCTV file: bad magic bytes");

    // Read version
    uint16_t version;
    in >> version;

    // Read salt & IV
    QByteArray salt(16, '\0');
    QByteArray iv(12, '\0');
    in.readRawData(salt.data(), 16);
    in.readRawData(iv.data(), 12);

    // Derive key
    QByteArray key = deriveKey(salt);

    // Read encrypted payload
    uint32_t payloadSize;
    in >> payloadSize;
    QByteArray ciphertext(static_cast<int>(payloadSize), '\0');
    in.readRawData(ciphertext.data(), payloadSize);

    // Read GCM tag
    QByteArray tag(16, '\0');
    in.readRawData(tag.data(), 16);

    // Decrypt
    QByteArray compressed = decrypt(ciphertext, key, iv, tag);

    // Decompress
    unsigned long long decompSize = ZSTD_getFrameContentSize(compressed.constData(), compressed.size());
    if (decompSize == ZSTD_CONTENTSIZE_ERROR || decompSize == ZSTD_CONTENTSIZE_UNKNOWN)
        throw std::runtime_error("Cannot determine decompressed size");

    QByteArray decompressed(static_cast<int>(decompSize), '\0');
    size_t result = ZSTD_decompress(decompressed.data(), decompSize,
                                     compressed.constData(), compressed.size());
    if (ZSTD_isError(result))
        throw std::runtime_error("ZSTD decompression failed");

    // Parse JSON
    auto j = nlohmann::json::parse(decompressed.toStdString());
    return Project::fromJson(j);
}

} // namespace ncktv
