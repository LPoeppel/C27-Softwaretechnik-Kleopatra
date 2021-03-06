/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-

    utils/formatting.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include "formatting.h"
#include "kleo/dn.h"

#include <gpgme++/key.h>
#include <gpgme++/importresult.h>

#include <QGpgME/CryptoConfig>
#include <QGpgME/Protocol>

#include <KLocalizedString>
#include <KEmailAddress>

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QTextDocument> // for Qt::escape
#include <QLocale>
#include <QIcon>

#include <gpgme++/gpgmepp_version.h>
#if GPGMEPP_VERSION > 0x10900
# define GPGME_HAS_KEY_IS_DEVS
#endif


using namespace GpgME;
using namespace Kleo;

//
// Name
//

QString Formatting::prettyName(int proto, const char *id, const char *name_, const char *comment_)
{

    if (proto == OpenPGP) {
        const QString name = QString::fromUtf8(name_);
        if (name.isEmpty()) {
            return QString();
        }
        const QString comment = QString::fromUtf8(comment_);
        if (comment.isEmpty()) {
            return name;
        }
        return QStringLiteral("%1 (%2)").arg(name, comment);
    }

    if (proto == CMS) {
        const DN subject(id);
        const QString cn = subject[QStringLiteral("CN")].trimmed();
        if (cn.isEmpty()) {
            return subject.prettyDN();
        }
        return cn;
    }

    return QString();
}

QString Formatting::prettyNameAndEMail(int proto, const char *id, const char *name_, const char *email_, const char *comment_)
{
    return prettyNameAndEMail(proto, QString::fromUtf8(id), QString::fromUtf8(name_), prettyEMail(email_, id), QString::fromUtf8(comment_));
}

QString Formatting::prettyNameAndEMail(int proto, const QString &id, const QString &name, const QString &email, const QString &comment)
{

    if (proto == OpenPGP) {
        if (name.isEmpty()) {
            if (email.isEmpty()) {
                return QString();
            } else if (comment.isEmpty()) {
                return QStringLiteral("<%1>").arg(email);
            } else {
                return QStringLiteral("(%2) <%1>").arg(email, comment);
            }
        }
        if (email.isEmpty()) {
            if (comment.isEmpty()) {
                return name;
            } else {
                return QStringLiteral("%1 (%2)").arg(name, comment);
            }
        }
        if (comment.isEmpty()) {
            return QStringLiteral("%1 <%2>").arg(name, email);
        } else {
            return QStringLiteral("%1 (%3) <%2>").arg(name, email, comment);
        }
    }

    if (proto == CMS) {
        const DN subject(id);
        const QString cn = subject[QStringLiteral("CN")].trimmed();
        if (cn.isEmpty()) {
            return subject.prettyDN();
        }
        return cn;
    }
    return QString();
}

QString Formatting::prettyUserID(const UserID &uid)
{
    if (uid.parent().protocol() == OpenPGP) {
        return prettyNameAndEMail(uid);
    }
    const QByteArray id = QByteArray(uid.id()).trimmed();
    if (id.startsWith('<')) {
        return prettyEMail(uid.email(), uid.id());
    }
    if (id.startsWith('('))
        // ### parse uri/dns:
    {
        return QString::fromUtf8(uid.id());
    } else {
        return DN(uid.id()).prettyDN();
    }
}

QString Formatting::prettyKeyID(const char *id)
{
    if (!id) {
        return QString();
    }
    return QLatin1String("0x") + QString::fromLatin1(id).toUpper();
}

QString Formatting::prettyNameAndEMail(const UserID &uid)
{
    return prettyNameAndEMail(uid.parent().protocol(), uid.id(), uid.name(), uid.email(), uid.comment());
}

QString Formatting::prettyNameAndEMail(const Key &key)
{
    return prettyNameAndEMail(key.userID(0));
}

QString Formatting::prettyName(const Key &key)
{
    return prettyName(key.userID(0));
}

QString Formatting::prettyName(const UserID &uid)
{
    return prettyName(uid.parent().protocol(), uid.id(), uid.name(), uid.comment());
}

QString Formatting::prettyName(const UserID::Signature &sig)
{
    return prettyName(OpenPGP, sig.signerUserID(), sig.signerName(), sig.signerComment());
}

//
// EMail
//

QString Formatting::prettyEMail(const Key &key)
{
    for (unsigned int i = 0, end = key.numUserIDs(); i < end; ++i) {
        const QString email = prettyEMail(key.userID(i));
        if (!email.isEmpty()) {
            return email;
        }
    }
    return QString();
}

QString Formatting::prettyEMail(const UserID &uid)
{
    return prettyEMail(uid.email(), uid.id());
}

QString Formatting::prettyEMail(const UserID::Signature &sig)
{
    return prettyEMail(sig.signerEmail(), sig.signerUserID());
}

QString Formatting::prettyEMail(const char *email_, const char *id)
{
    QString email, name, comment;
    if (email_ && KEmailAddress::splitAddress(QString::fromUtf8(email_),
                                              name, email, comment) == KEmailAddress::AddressOk) {
        return email;
    } else {
        return DN(id)[QStringLiteral("EMAIL")].trimmed();
    }
}

//
// Tooltip
//

namespace
{

static QString protect_whitespace(QString s)
{
    static const QLatin1Char SP(' '), NBSP('\xA0');
    return s.replace(SP, NBSP);
}

template <typename T_arg>
QString format_row(const QString &field, const T_arg &arg)
{
    return QStringLiteral("<tr><th>%1:</th><td>%2</td></tr>").arg(protect_whitespace(field), arg);
}
QString format_row(const QString &field, const QString &arg)
{
    return QStringLiteral("<tr><th>%1:</th><td>%2</td></tr>").arg(protect_whitespace(field), arg.toHtmlEscaped());
}
QString format_row(const QString &field, const char *arg)
{
    return format_row(field, QString::fromUtf8(arg));
}

QString format_keytype(const Key &key)
{
    const Subkey subkey = key.subkey(0);
    if (key.hasSecret()) {
        return i18n("%1-bit %2 (secret key available)", subkey.length(), QLatin1String(subkey.publicKeyAlgorithmAsString()));
    } else {
        return i18n("%1-bit %2", subkey.length(), QLatin1String(subkey.publicKeyAlgorithmAsString()));
    }
}

QString format_keyusage(const Key &key)
{
    QStringList capabilities;
    if (key.canReallySign()) {
        if (key.isQualified()) {
            capabilities.push_back(i18n("Signing (Qualified)"));
        } else {
            capabilities.push_back(i18n("Signing"));
        }
    }
    if (key.canEncrypt()) {
        capabilities.push_back(i18n("Encryption"));
    }
    if (key.canCertify()) {
        capabilities.push_back(i18n("Certifying User-IDs"));
    }
    if (key.canAuthenticate()) {
        capabilities.push_back(i18n("SSH Authentication"));
    }
    return capabilities.join(QStringLiteral(", "));
}

static QString time_t2string(time_t t)
{
    QDateTime dt;
    dt.setTime_t(t);
    return QLocale().toString(dt, QLocale::ShortFormat);
}

static QString make_red(const QString &txt)
{
    return QLatin1String("<font color=\"red\">") + txt.toHtmlEscaped() + QLatin1String("</font>");
}

}

QString Formatting::toolTip(const Key &key, int flags)
{
    if (flags == 0 || (key.protocol() != CMS && key.protocol() != OpenPGP)) {
        return QString();
    }

    const Subkey subkey = key.subkey(0);

    QString result;
    if (flags & Validity) {
        if (key.protocol() == OpenPGP || (key.keyListMode() & Validate))
            if (key.isRevoked()) {
                result = make_red(i18n("Revoked"));
            } else if (key.isExpired()) {
                result = make_red(i18n("Expired"));
            } else if (key.isDisabled()) {
                result = i18n("Disabled");
            } else {
                unsigned int fullyTrusted = 0;
                for (const auto &uid: key.userIDs()) {
                    if (uid.validity() >= UserID::Validity::Full) {
                        fullyTrusted++;
                    }
                }
                if (fullyTrusted == key.numUserIDs()) {
                    result = i18n("All User-IDs are certified.");
                    const auto compliance = complianceStringForKey(key);
                    if (!compliance.isEmpty()) {
                        result += QStringLiteral("<br>") + compliance;
                    }
                } else {
                    result = i18np("One User-ID is not certified.", "%1 User-IDs are not certified.", key.numUserIDs() - fullyTrusted);
                }
            }
        else {
            result = i18n("The validity cannot be checked at the moment.");
        }
    }
    if (flags == Validity) {
        return result;
    }

    result += QLatin1String("<table border=\"0\">");
    if (key.protocol() == CMS) {
        if (flags & SerialNumber) {
            result += format_row(i18n("Serial number"), key.issuerSerial());
        }
        if (flags & Issuer) {
            result += format_row(i18n("Issuer"), key.issuerName());
        }
    }
    if (flags & UserIDs) {
        const std::vector<UserID> uids = key.userIDs();
        if (!uids.empty())
            result += format_row(key.protocol() == CMS
                                 ? i18n("Subject")
                                 : i18n("User-ID"), prettyUserID(uids.front()));
        if (uids.size() > 1)
            for (std::vector<UserID>::const_iterator it = uids.begin() + 1, end = uids.end(); it != end; ++it)
                if (!it->isRevoked() && !it->isInvalid()) {
                    result += format_row(i18n("a.k.a."), prettyUserID(*it));
                }
    }
    if (flags & ExpiryDates) {
        result += format_row(i18n("Created"), time_t2string(subkey.creationTime()));

        if (key.isExpired()) {
            result += format_row(i18n("Expired"), time_t2string(subkey.expirationTime()));
        } else if (!subkey.neverExpires()) {
            result += format_row(i18n("Expires"), time_t2string(subkey.expirationTime()));
        }
    }
    if (flags & CertificateType) {
        result += format_row(i18n("Type"), format_keytype(key));
    }
    if (flags & CertificateUsage) {
        result += format_row(i18n("Usage"), format_keyusage(key));
    }
    if (flags & KeyID) {
        result += format_row(i18n("Key-ID"), QString::fromLatin1(key.shortKeyID()));
    }
    if (flags & Fingerprint) {
        result += format_row(i18n("Fingerprint"), key.primaryFingerprint());
    }
    if (flags & OwnerTrust) {
        if (key.protocol() == OpenPGP) {
            result += format_row(i18n("Certification trust"), ownerTrustShort(key));
        } else if (key.isRoot()) {
            result += format_row(i18n("Trusted issuer?"),
                                 key.userID(0).validity() == UserID::Ultimate ? i18n("Yes") :
                                 /* else */                                     i18n("No"));
        }
    }

    if (flags & StorageLocation) {
        if (const char *card = subkey.cardSerialNumber()) {
            result += format_row(i18n("Stored"), i18nc("stored...", "on SmartCard with serial no. %1", QString::fromUtf8(card)));
        } else {
            result += format_row(i18n("Stored"), i18nc("stored...", "on this computer"));
        }
    }
    result += QLatin1String("</table>");

    return result;
}

//
// Creation and Expiration
//

namespace
{
static QDate time_t2date(time_t t)
{
    if (!t) {
        return QDate();
    }
    QDateTime dt;
    dt.setTime_t(t);
    return dt.date();
}
static QString date2string(const QDate &date)
{
    return QLocale().toString(date, QLocale::ShortFormat);
}

template <typename T>
QString expiration_date_string(const T &tee)
{
    return tee.neverExpires() ? QString() : date2string(time_t2date(tee.expirationTime()));
}
template <typename T>
QDate creation_date(const T &tee)
{
    return time_t2date(tee.creationTime());
}
template <typename T>
QDate expiration_date(const T &tee)
{
    return time_t2date(tee.expirationTime());
}
}

QString Formatting::expirationDateString(const Key &key)
{
    return expiration_date_string(key.subkey(0));
}

QString Formatting::expirationDateString(const Subkey &subkey)
{
    return expiration_date_string(subkey);
}

QString Formatting::expirationDateString(const UserID::Signature &sig)
{
    return expiration_date_string(sig);
}

QDate Formatting::expirationDate(const Key &key)
{
    return expiration_date(key.subkey(0));
}

QDate Formatting::expirationDate(const Subkey &subkey)
{
    return expiration_date(subkey);
}

QDate Formatting::expirationDate(const UserID::Signature &sig)
{
    return expiration_date(sig);
}

QString Formatting::creationDateString(const Key &key)
{
    return date2string(creation_date(key.subkey(0)));
}

QString Formatting::creationDateString(const Subkey &subkey)
{
    return date2string(creation_date(subkey));
}

QString Formatting::creationDateString(const UserID::Signature &sig)
{
    return date2string(creation_date(sig));
}

QDate Formatting::creationDate(const Key &key)
{
    return creation_date(key.subkey(0));
}

QDate Formatting::creationDate(const Subkey &subkey)
{
    return creation_date(subkey);
}

QDate Formatting::creationDate(const UserID::Signature &sig)
{
    return creation_date(sig);
}

//
// Types
//

QString Formatting::displayName(Protocol p)
{
    if (p == CMS) {
        return i18nc("X.509/CMS encryption standard", "X.509");
    }
    if (p == OpenPGP) {
        return i18n("OpenPGP");
    }
    return i18nc("Unknown encryption protocol", "Unknown");
}

QString Formatting::type(const Key &key)
{
    return displayName(key.protocol());
}

QString Formatting::type(const Subkey &subkey)
{
    return QString::fromUtf8(subkey.publicKeyAlgorithmAsString());
}

//
// Status / Validity
//

QString Formatting::ownerTrustShort(const Key &key)
{
    return ownerTrustShort(key.ownerTrust());
}

QString Formatting::ownerTrustShort(Key::OwnerTrust trust)
{
    switch (trust) {
    case Key::Unknown:   return i18nc("unknown trust level", "unknown");
    case Key::Never:     return i18n("untrusted");
    case Key::Marginal:  return i18nc("marginal trust", "marginal");
    case Key::Full:      return i18nc("full trust", "full");
    case Key::Ultimate:  return i18nc("ultimate trust", "ultimate");
    case Key::Undefined: return i18nc("undefined trust", "undefined");
    default:
        Q_ASSERT(!"unexpected owner trust value");
        break;
    }
    return QString();
}

QString Formatting::validityShort(const Subkey &subkey)
{
    if (subkey.isRevoked()) {
        return i18n("revoked");
    }
    if (subkey.isExpired()) {
        return i18n("expired");
    }
    if (subkey.isDisabled()) {
        return i18n("disabled");
    }
    if (subkey.isInvalid()) {
        return i18n("invalid");
    }
    return i18nc("as in good/valid signature", "good");
}

QString Formatting::validityShort(const UserID &uid)
{
    if (uid.isRevoked()) {
        return i18n("revoked");
    }
    if (uid.isInvalid()) {
        return i18n("invalid");
    }
    switch (uid.validity()) {
    case UserID::Unknown:   return i18nc("unknown trust level", "unknown");
    case UserID::Undefined: return i18nc("undefined trust", "undefined");
    case UserID::Never:     return i18n("untrusted");
    case UserID::Marginal:  return i18nc("marginal trust", "marginal");
    case UserID::Full:      return i18nc("full trust", "full");
    case UserID::Ultimate:  return i18nc("ultimate trust", "ultimate");
    }
    return QString();
}

QString Formatting::validityShort(const UserID::Signature &sig)
{
    switch (sig.status()) {
    case UserID::Signature::NoError:
        if (!sig.isInvalid()) {
            if (sig.certClass() > 0) {
                return i18n("class %1", sig.certClass());
            } else {
                return i18nc("good/valid signature", "good");
            }
        }
        Q_FALLTHROUGH();
        // fall through:
    case UserID::Signature::GeneralError:
        return i18n("invalid");
    case UserID::Signature::SigExpired:   return i18n("expired");
    case UserID::Signature::KeyExpired:   return i18n("certificate expired");
    case UserID::Signature::BadSignature: return i18nc("fake/invalid signature", "bad");
    case UserID::Signature::NoPublicKey:  return QString();
    }
    return QString();
}

QString Formatting::formatKeyLink(const Key &key)
{
    if (key.isNull()) {
        return QString();
    }
    return QStringLiteral("<a href=\"key:%1\">%2</a>").arg(QLatin1String(key.primaryFingerprint()), Formatting::prettyName(key));
}

QString Formatting::formatForComboBox(const GpgME::Key &key)
{
    const QString name = prettyName(key);
    QString mail = prettyEMail(key);
    if (!mail.isEmpty()) {
        mail = QLatin1Char('<') + mail + QLatin1Char('>');
    }
    return i18nc("name, email, key id", "%1 %2 (%3)", name, mail, QLatin1String(key.shortKeyID())).simplified();
}

namespace
{

static QString keyToString(const Key &key)
{

    Q_ASSERT(!key.isNull());

    const QString email = Formatting::prettyEMail(key);
    const QString name = Formatting::prettyName(key);

    if (name.isEmpty()) {
        return email;
    } else if (email.isEmpty()) {
        return name;
    } else {
        return QStringLiteral("%1 <%2>").arg(name, email);
    }
}

}

const char *Formatting::summaryToString(const Signature::Summary summary)
{
    if (summary & Signature::Red) {
        return "RED";
    }
    if (summary & Signature::Green) {
        return "GREEN";
    }
    return "YELLOW";
}

QString Formatting::signatureToString(const Signature &sig, const Key &key)
{
    if (sig.isNull()) {
        return QString();
    }

    const bool red   = (sig.summary() & Signature::Red);
    const bool valid = (sig.summary() & Signature::Valid);

    if (red)
        if (key.isNull())
            if (const char *fpr = sig.fingerprint()) {
                return i18n("Bad signature by unknown certificate %1: %2", QString::fromLatin1(fpr), QString::fromLocal8Bit(sig.status().asString()));
            } else {
                return i18n("Bad signature by an unknown certificate: %1", QString::fromLocal8Bit(sig.status().asString()));
            }
        else {
            return i18n("Bad signature by %1: %2", keyToString(key), QString::fromLocal8Bit(sig.status().asString()));
        }

    else if (valid)
        if (key.isNull())
            if (const char *fpr = sig.fingerprint()) {
                return i18n("Good signature by unknown certificate %1.", QString::fromLatin1(fpr));
            } else {
                return i18n("Good signature by an unknown certificate.");
            }
        else {
            return i18n("Good signature by %1.", keyToString(key));
        }

    else if (key.isNull())
        if (const char *fpr = sig.fingerprint()) {
            return i18n("Invalid signature by unknown certificate %1: %2", QString::fromLatin1(fpr), QString::fromLocal8Bit(sig.status().asString()));
        } else {
            return i18n("Invalid signature by an unknown certificate: %1", QString::fromLocal8Bit(sig.status().asString()));
        }
    else {
        return i18n("Invalid signature by %1: %2", keyToString(key), QString::fromLocal8Bit(sig.status().asString()));
    }
}

//
// ImportResult
//

QString Formatting::importMetaData(const Import &import, const QStringList &ids)
{
    const QString result = importMetaData(import);
    if (result.isEmpty()) {
        return QString();
    } else
        return result + QLatin1Char('\n') +
               i18n("This certificate was imported from the following sources:") + QLatin1Char('\n') +
               ids.join(QLatin1Char('\n'));
}

QString Formatting::importMetaData(const Import &import)
{

    if (import.isNull()) {
        return QString();
    }

    if (import.error().isCanceled()) {
        return i18n("The import of this certificate was canceled.");
    }
    if (import.error())
        return i18n("An error occurred importing this certificate: %1",
                    QString::fromLocal8Bit(import.error().asString()));

    const unsigned int status = import.status();
    if (status & Import::NewKey)
        return (status & Import::ContainedSecretKey)
               ? i18n("This certificate was new to your keystore. The secret key is available.")
               : i18n("This certificate is new to your keystore.");

    QStringList results;
    if (status & Import::NewUserIDs) {
        results.push_back(i18n("New user-ids were added to this certificate by the import."));
    }
    if (status & Import::NewSignatures) {
        results.push_back(i18n("New signatures were added to this certificate by the import."));
    }
    if (status & Import::NewSubkeys) {
        results.push_back(i18n("New subkeys were added to this certificate by the import."));
    }

    return results.empty()
           ? i18n("The import contained no new data for this certificate. It is unchanged.")
           : results.join(QLatin1Char('\n'));
}

//
// Overview in CertificateDetailsDialog
//

QString Formatting::formatOverview(const Key &key)
{
    return toolTip(key, AllOptions);
}

QString Formatting::usageString(const Subkey &sub)
{
    QStringList usageStrings;
    if (sub.canCertify()) {
        usageStrings << i18n("Certify");
    }
    if (sub.canSign()) {
        usageStrings << i18n("Sign");
    }
    if (sub.canEncrypt()) {
        usageStrings << i18n("Encrypt");
    }
    if (sub.canAuthenticate()) {
        usageStrings << i18n("Authenticate");
    }
    return usageStrings.join(QStringLiteral(", "));
}

QString Formatting::summaryLine(const Key &key)
{
    return keyToString(key) + QLatin1Char(' ') +
           i18nc("(validity, protocol, creation date)",
                 "(%1, %2, created: %3)",
		 Formatting::complianceStringShort(key),
		 displayName(key.protocol()),
                 Formatting::creationDateString(key));
}

// Icon for certificate selection indication
QIcon Formatting::iconForUid(const UserID &uid)
{
    switch (uid.validity()) {
        case UserID::Ultimate:
        case UserID::Full:
        case UserID::Marginal:
            return QIcon::fromTheme(QStringLiteral("emblem-success"));
        case UserID::Never:
            return QIcon::fromTheme(QStringLiteral("emblem-error"));
        case UserID::Undefined:
        case UserID::Unknown:
        default:
            return QIcon::fromTheme(QStringLiteral("emblem-information"));
    }
}

QString Formatting::validity(const UserID &uid)
{
    switch (uid.validity()) {
        case UserID::Ultimate:
            return i18n("The certificate is marked as your own.");
        case UserID::Full:
            return i18n("The certificate belongs to this recipient.");
        case UserID::Marginal:
            return i18n("The trust model indicates marginally that the certificate belongs to this recipient.");
        case UserID::Never:
            return i18n("This certificate should not be used.");
        case UserID::Undefined:
        case UserID::Unknown:
        default:
            return i18n("There is no indication that this certificate belongs to this recipient.");
    }
}

bool Formatting::uidsHaveFullValidity(const GpgME::Key &key)
{
    for (const auto &uid: key.userIDs()) {
        if (uid.validity() < UserID::Validity::Full) {
            return false;
        }
    }
    return true;
}

QString Formatting::complianceMode()
{
    const QGpgME::CryptoConfig *const config = QGpgME::cryptoConfig();
    if (!config) {
        return QString();
    }
    const QGpgME::CryptoConfigEntry *const entry = config->entry(QStringLiteral("gpg"), QStringLiteral("Configuration"), QStringLiteral("compliance"));

    if (!entry || entry->stringValue() == QStringLiteral("gnupg")) {
        return QString();
    }
    return entry->stringValue();
}

bool Formatting::isKeyDeVs(const GpgME::Key &key)
{
#ifdef GPGME_HAS_KEY_IS_DEVS
    for (const auto &sub: key.subkeys()) {
        if (sub.isExpired() || sub.isRevoked()) {
            // Ignore old subkeys
            continue;
        }
        if (!sub.isDeVs()) {
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

QString Formatting::complianceStringForKey(const GpgME::Key &key)
{
    // There will likely be more in the future for other institutions
    // for now we only have DE-VS
    if (complianceMode() == QStringLiteral("de-vs")) {
        if (uidsHaveFullValidity(key) && isKeyDeVs(key)) {
            return i18nc("VS-NfD conforming is a German standard for restricted documents. For which special restrictions about algorithms apply. The string describes if a key is compliant with that..",
                         "May be used for VS-NfD-compliant communication.");
        } else {
            return i18nc("VS-NfD-conforming is a German standard for restricted documents. For which special restrictions about algorithms apply. The string describes if a key is compliant to that..",
                         "May <b>not</b> be used for VS-NfD-compliant communication.");
        }
    }
    return QString();
}

QString Formatting::complianceStringShort(const GpgME::Key &key)
{
    if (Formatting::uidsHaveFullValidity(key)) {
        if (complianceMode() == QStringLiteral("de-vs")
            && Formatting::isKeyDeVs(key)) {
            return QStringLiteral("★ ") +
                i18nc("VS-NfD-conforming is a German standard for restricted documents for which special restrictions about algorithms apply.  The string states that a key is compliant with that.",
                      "VS-NfD-compliant");
        }
        return i18nc("As in all user IDs are valid.", "certified");
    }

    return i18nc("As in not all user IDs are valid.", "not certified");
}
