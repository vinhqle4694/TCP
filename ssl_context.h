#pragma once

#include <memory>
#include <string>
#include <vector>

#ifdef TCP_SSL_SUPPORT
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#endif

namespace tcp {

// SSL/TLS Context for secure connections
class SslContext {
public:
    enum class Method {
        TLS,           // TLS (recommended)
        TLS_CLIENT,    // TLS client
        TLS_SERVER,    // TLS server
        DTLS,          // DTLS
        DTLS_CLIENT,   // DTLS client
        DTLS_SERVER    // DTLS server
    };
    
    enum class VerifyMode {
        None,          // No verification
        Peer,          // Verify peer
        FailIfNoPeer,  // Fail if no peer cert
        Once           // Verify once
    };

    SslContext(Method method = Method::TLS);
    ~SslContext();

    // Non-copyable
    SslContext(const SslContext&) = delete;
    SslContext& operator=(const SslContext&) = delete;

    // Movable
    SslContext(SslContext&& other) noexcept;
    SslContext& operator=(SslContext&& other) noexcept;

    // Certificate and key management
    bool loadCertificate(const std::string& certFile);
    bool loadCertificateChain(const std::string& chainFile);
    bool loadPrivateKey(const std::string& keyFile);
    bool loadCertificateFromMemory(const std::vector<uint8_t>& certData);
    bool loadPrivateKeyFromMemory(const std::vector<uint8_t>& keyData);

    // CA certificate management
    bool loadCaCertificate(const std::string& caCertFile);
    bool loadCaCertificateDir(const std::string& caCertDir);
    bool loadCaCertificateFromMemory(const std::vector<uint8_t>& caCertData);

    // Verification
    void setVerifyMode(VerifyMode mode);
    VerifyMode getVerifyMode() const { return verifyMode_; }
    void setVerifyDepth(int depth);
    int getVerifyDepth() const { return verifyDepth_; }

    // Cipher suites
    bool setCipherSuites(const std::string& ciphers);
    std::string getCipherSuites() const;

    // Protocol versions
    bool setMinProtocolVersion(int version);
    bool setMaxProtocolVersion(int version);
    int getMinProtocolVersion() const;
    int getMaxProtocolVersion() const;

    // Session management
    void setSessionCacheMode(int mode);
    int getSessionCacheMode() const;
    void setSessionTimeout(long timeout);
    long getSessionTimeout() const;

    // SNI (Server Name Indication)
    bool setSniHostname(const std::string& hostname);
    std::string getSniHostname() const { return sniHostname_; }

    // ALPN (Application Layer Protocol Negotiation)
    bool setAlpnProtocols(const std::vector<std::string>& protocols);
    std::vector<std::string> getAlpnProtocols() const;

    // Validation
    bool isValid() const;
    std::string getLastError() const;

    // Internal access (for TcpConnection)
    void* getContext() const { return context_; }

    // Static utility methods
    static std::string getOpenSslVersion();
    static std::vector<std::string> getAvailableCiphers();
    static bool isOpenSslAvailable();
    static std::shared_ptr<SslContext> createClientContext();
    static std::shared_ptr<SslContext> createServerContext();

private:
    void* context_;  // SSL_CTX* handle
    Method method_;
    VerifyMode verifyMode_;
    int verifyDepth_;
    std::string sniHostname_;
    std::vector<std::string> alpnProtocols_;
    mutable std::string lastError_;

    // Internal methods
    bool initialize();
    void cleanup();
    static int verifyCallback(int preverify_ok, void* x509_ctx);
    static int sniCallback(void* ssl, int* ad, void* arg);
    static int alpnCallback(void* ssl, const unsigned char** out, unsigned char* outlen,
                           const unsigned char* in, unsigned int inlen, void* arg);
    void setLastError(const std::string& error);
    
#ifdef TCP_SSL_SUPPORT
    static bool sslInitialized_;
    static std::mutex sslMutex_;
    static void initializeOpenSsl();
    static void cleanupOpenSsl();
#endif
};

// SSL/TLS Certificate information
struct CertificateInfo {
    std::string subject;
    std::string issuer;
    std::string serialNumber;
    std::string version;
    std::string notBefore;
    std::string notAfter;
    std::string fingerprint;
    std::vector<std::string> subjectAltNames;
    std::vector<std::string> issuerAltNames;
    bool isValid;
    bool isSelfSigned;
    bool isExpired;
    int keyBits;
    std::string keyAlgorithm;
    std::string signatureAlgorithm;
};

// SSL/TLS Connection information
struct SslConnectionInfo {
    std::string protocol;
    std::string cipher;
    std::string cipherBits;
    std::string peerCertificate;
    CertificateInfo certificateInfo;
    bool isVerified;
    std::vector<std::string> alpnProtocols;
    std::string selectedAlpnProtocol;
};

// SSL/TLS utility functions
class SslUtils {
public:
    // Certificate parsing
    static CertificateInfo parseCertificate(const std::vector<uint8_t>& certData);
    static CertificateInfo parseCertificateFile(const std::string& certFile);
    static std::vector<CertificateInfo> parseCertificateChain(const std::vector<uint8_t>& chainData);
    static std::vector<CertificateInfo> parseCertificateChainFile(const std::string& chainFile);

    // Certificate validation
    static bool validateCertificate(const CertificateInfo& cert, const std::string& hostname = "");
    static bool validateCertificateChain(const std::vector<CertificateInfo>& chain);

    // Key pair generation
    static std::pair<std::vector<uint8_t>, std::vector<uint8_t>> generateKeyPair(int keyBits = 2048);
    static std::vector<uint8_t> generateSelfSignedCertificate(
        const std::vector<uint8_t>& privateKey,
        const std::string& subject,
        int validDays = 365
    );

    // PEM/DER conversion
    static std::vector<uint8_t> pemToDer(const std::string& pem);
    static std::string derToPem(const std::vector<uint8_t>& der, const std::string& label = "CERTIFICATE");

    // Error handling
    static std::string getLastSslError();
    static std::string sslErrorToString(unsigned long error);
};

} // namespace tcp
