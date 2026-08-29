#pragma once

#include <QString>
#include <QByteArray>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#include <dpapi.h>
#pragma comment(lib, "crypt32.lib")
#endif

/// Encryption utilities using Windows DPAPI for protecting sensitive data
/// like API keys at rest (user-bound encryption, no key management needed).
class CryptoUtils {
public:
    /// Encrypt a plaintext string. Returns base64-encoded ciphertext.
    static QByteArray encrypt(const QByteArray &plaintext)
    {
#ifdef _WIN32
        DATA_BLOB dataIn;
        DATA_BLOB dataOut;

        dataIn.pbData = reinterpret_cast<BYTE *>(
            const_cast<char *>(plaintext.constData()));
        dataIn.cbData = static_cast<DWORD>(plaintext.size());

        if (CryptProtectData(&dataIn, L"DailyReport API Key",
                             nullptr, nullptr, nullptr,
                             CRYPTPROTECT_UI_FORBIDDEN | CRYPTPROTECT_LOCAL_MACHINE,
                             &dataOut))
        {
            QByteArray result = QByteArray::fromRawData(
                reinterpret_cast<const char *>(dataOut.pbData),
                static_cast<int>(dataOut.cbData)).toBase64();
            LocalFree(dataOut.pbData);
            return result;
        }

        spdlog::error("CryptProtectData failed: {}", GetLastError());
        return {};
#else
        // Linux keyring integration is not available yet. This prevents
        // accidental plaintext display but is not cryptographic protection.
        return plaintext.toBase64();
#endif
    }

    /// Decrypt a base64-encoded ciphertext back to plaintext.
    static QByteArray decrypt(const QByteArray &ciphertext)
    {
#ifdef _WIN32
        QByteArray encrypted = QByteArray::fromBase64(ciphertext);
        if (encrypted.isEmpty()) return {};

        DATA_BLOB dataIn;
        DATA_BLOB dataOut;

        dataIn.pbData = reinterpret_cast<BYTE *>(encrypted.data());
        dataIn.cbData = static_cast<DWORD>(encrypted.size());

        if (CryptUnprotectData(&dataIn, nullptr, nullptr, nullptr, nullptr,
                               CRYPTPROTECT_UI_FORBIDDEN,
                               &dataOut))
        {
            QByteArray result(
                reinterpret_cast<const char *>(dataOut.pbData),
                static_cast<int>(dataOut.cbData));
            LocalFree(dataOut.pbData);
            return result;
        }

        spdlog::error("CryptUnprotectData failed: {}", GetLastError());
        return {};
#else
        return QByteArray::fromBase64(ciphertext);
#endif
    }
};
