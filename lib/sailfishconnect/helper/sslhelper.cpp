/*
 * Copyright 2018 Richard Liebscher <richard.liebscher@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sslhelper.h"

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/pem.h>


#include <memory>
#include <QDebug>
#include <QDateTime>
#include <QLoggingCategory>
#include <QIODevice>
#include <QSslKey>
#include <QSslCertificate>

namespace SailfishConnect {
namespace Ssl {

static Q_LOGGING_CATEGORY(logger, "sailfishconnect.ssl")


struct BigNumDeleter {
    void operator()(BIGNUM* ptr) { BN_free(ptr); }
};
using BigNumPtr = std::unique_ptr<BIGNUM, BigNumDeleter>;


#define _SSL_PTR(name, sslName) \
    struct name##Deleter { \
        void operator()(sslName* ptr) { sslName##_free(ptr); } \
    }; \
    using name##Ptr = std::unique_ptr<sslName, name##Deleter>;
#define SSL_PTR(name, sslName) _SSL_PTR(name, sslName)

SSL_PTR(X509, X509)
SSL_PTR(Rsa, RSA)
SSL_PTR(Bio, BIO)
SSL_PTR(EvpPkey, EVP_PKEY)


static QByteArray getByteArray(BIO* bio)
{
    QByteArray buf;

    int ret;
    do {
        char block[1024];
        ret = BIO_read(bio, block, 1024);
        if (ret <= 0)
            break;

        buf.append(block, ret);
    } while (ret == 1024);

    return buf;
}

static BigNumPtr toBigNum(unsigned int value)
{
    BigNumPtr result(BN_new());

    int success = BN_set_word(result.get(), value);
    if (!success) {
        BN_zero(result.get());
    }

    return result;
}

QSslKey KeyGenerator::generateRsa(int bits)
{
    BigNumPtr e = toBigNum(RSA_F4);

    RsaPtr rsa(RSA_new());
    bool success = RSA_generate_key_ex(rsa.get(), bits, e.get(), nullptr);
    if (!success) {
        return QSslKey();
    }

    BioPtr memIo(BIO_new(BIO_s_mem()));
    PEM_write_bio_RSAPrivateKey(
                memIo.get(), rsa.get(), nullptr,
                nullptr, 0, nullptr, nullptr);

    return QSslKey(
                getByteArray(memIo.get()),
                QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
}


static void addInfoEntry(X509_NAME* name, int nid, const QString &val)
{
    if (val.isNull())
        return;

    QByteArray buf = val.toLatin1();
    X509_NAME_add_entry_by_NID(
        name, nid,
        MBSTRING_ASC, (unsigned char *)buf.data(), buf.size(), -1, 0);
}

QSslCertificate CertificateBuilder::selfSigned(const QSslKey &key) const
{
    if (key.type() != QSsl::PrivateKey)
        return QSslCertificate();

    X509Ptr cert(X509_new());

    // serial number
    ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), m_serialNumber);

    // notBefore / notAfter
    ASN1_TIME_set(
       X509_get_notBefore(cert.get()),
       m_notBefore.toMSecsSinceEpoch() / 1000);
    ASN1_TIME_set(
       X509_get_notAfter(cert.get()),
       m_notAfter.toMSecsSinceEpoch() / 1000);

    // key
    EvpPkeyPtr pkey(EVP_PKEY_new());
    switch (key.algorithm()) {
    case QSsl::Rsa:
        EVP_PKEY_set1_RSA(pkey.get(), reinterpret_cast<RSA*>(key.handle()));
        break;

    case QSsl::Dsa:
        EVP_PKEY_set1_DSA(pkey.get(), reinterpret_cast<DSA*>(key.handle()));
        break;

    case QSsl::Ec:
        EVP_PKEY_set1_EC_KEY(pkey.get(), reinterpret_cast<EC_KEY*>(key.handle()));
        break;

    default:
        return QSslCertificate();
    }
    X509_set_pubkey(cert.get(), pkey.get());

    // informations
    X509_NAME* name;
    name = X509_get_subject_name(cert.get());

    addInfoEntry(name, NID_commonName, m_info.value(CommonName));
    addInfoEntry(name, NID_countryName, m_info.value(Country));
    addInfoEntry(name, NID_localityName, m_info.value(Locality));
    addInfoEntry(name, NID_stateOrProvinceName, m_info.value(State));
    addInfoEntry(name, NID_organizationName, m_info.value(Organization));
    addInfoEntry(name, NID_organizationalUnitName,
                 m_info.value(OrganizationalUnit));

    // subject == issuer
    X509_set_issuer_name(cert.get(), name);

    // sign
    int bytes = X509_sign(cert.get(), pkey.get(), EVP_sha1());
    if (bytes == 0) {
        qCWarning(logger) << "certificate signing failed (X509_sign)";
        return QSslCertificate();
    }

    // convert to PEM
    BioPtr mem(BIO_new(BIO_s_mem()));
    bool success = PEM_write_bio_X509(mem.get(), cert.get());
    if (!success) {
        qCWarning(logger) << "convertion of certificate to PEM format failed";
        return QSslCertificate();
    }

    // create QSslCertificate
    return QSslCertificate(getByteArray(mem.get()), QSsl::Pem);
}

} // namespace Ssl
} // SailfishConnect
