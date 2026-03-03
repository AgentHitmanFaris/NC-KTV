#pragma once
/*
 * NC-KTV Core — NCTV Binary Format
 * Port of nctv_format.py — AES-256-GCM encrypted project container
 *
 * Byte layout:
 *   0x00  4B   Magic "NCTV"
 *   0x04  2B   Version
 *   0x06  16B  Salt
 *   0x16  ...  Encrypted chunks
 */

#include <QString>
#include <cstdint>

namespace ncktv {

class Project;  // forward declaration

class NCTVFormat {
public:
    /// Pack project into encrypted .nctv file
    static void pack(const Project& project, const QString& outputPath);

    /// Unpack encrypted .nctv file into Project
    static Project unpack(const QString& inputPath);

private:
    static constexpr char     MAGIC[4]   = {'N','C','T','V'};
    static constexpr uint16_t VERSION    = 1;
    static constexpr int      CHUNK_SIZE = 65536;   // 64KB

    /// Derive AES-256 key from fixed salt using PBKDF2-HMAC-SHA256
    static QByteArray deriveKey(const QByteArray& salt);

    /// Encrypt a block with AES-256-GCM
    static QByteArray encrypt(const QByteArray& plaintext, const QByteArray& key,
                              const QByteArray& iv, QByteArray& tag);

    /// Decrypt a block with AES-256-GCM
    static QByteArray decrypt(const QByteArray& ciphertext, const QByteArray& key,
                              const QByteArray& iv, const QByteArray& tag);
};

} // namespace ncktv
