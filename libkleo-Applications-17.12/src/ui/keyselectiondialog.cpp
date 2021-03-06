/*  -*- c++ -*-
    keyselectiondialog.cpp

    This file is part of libkleopatra, the KDE keymanagement library
    Copyright (c) 2004 Klar�vdalens Datakonsult AB

    Based on kpgpui.cpp
    Copyright (C) 2001,2002 the KPGP authors
    See file libkdenetwork/AUTHORS.kpgp for details

    Libkleopatra is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.

    Libkleopatra is distributed in the hope that it will be useful,
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

#include "keyselectiondialog.h"

#include "keylistview.h"
#include "progressdialog.h"

#include "libkleo/dn.h"
#include "kleo_ui_debug.h"

// gpgme++
#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>

#include <qgpgme/keylistjob.h>
#include <qgpgme/protocol.h>

// KDE
#include <KLocalizedString>
#include <kwindowsystem.h>
#include <kconfig.h>
#include <kmessagebox.h>
#include <qpushbutton.h>
#include <kconfiggroup.h>
#include <qmenu.h>
#include <QLineEdit>

// Qt
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
#include <QProcess>

#include <QRegExp>
#include <QPushButton>
#include <QFrame>
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>

#include <string.h>
#include <qscrollbar.h>
#include <KSharedConfig>
#include <KConfigGroup>
#include <QDialogButtonBox>

static bool checkKeyUsage(const GpgME::Key &key, unsigned int keyUsage, QString *statusString = nullptr)
{
    auto setStatusString = [statusString](const QString &status) {
        if (statusString) {
            *statusString = status;
        }
    };

    if (keyUsage & Kleo::KeySelectionDialog::ValidKeys) {
        if (key.isInvalid()) {
            if (key.keyListMode() & GpgME::Validate) {
                qCDebug(KLEO_UI_LOG) << "key is invalid";
                setStatusString(i18n("The key is not valid."));
                return false;
            } else {
                qCDebug(KLEO_UI_LOG) << "key is invalid - ignoring";
            }
        }
        if (key.isExpired()) {
            qCDebug(KLEO_UI_LOG) << "key is expired";
            setStatusString(i18n("The key is expired."));
            return false;
        } else if (key.isRevoked()) {
            qCDebug(KLEO_UI_LOG) << "key is revoked";
            setStatusString(i18n("The key is revoked."));
            return false;
        } else if (key.isDisabled()) {
            qCDebug(KLEO_UI_LOG) << "key is disabled";
            setStatusString(i18n("The key is disabled."));
            return false;
        }
    }

    if (keyUsage & Kleo::KeySelectionDialog::EncryptionKeys &&
            !key.canEncrypt()) {
        qCDebug(KLEO_UI_LOG) << "key can't encrypt";
        setStatusString(i18n("The key is not designated for encryption."));
        return false;
    }
    if (keyUsage & Kleo::KeySelectionDialog::SigningKeys &&
            !key.canSign()) {
        qCDebug(KLEO_UI_LOG) << "key can't sign";
        setStatusString(i18n("The key is not designated for signing."));
        return false;
    }
    if (keyUsage & Kleo::KeySelectionDialog::CertificationKeys &&
            !key.canCertify()) {
        qCDebug(KLEO_UI_LOG) << "key can't certify";
        setStatusString(i18n("The key is not designated for certifying."));
        return false;
    }
    if (keyUsage & Kleo::KeySelectionDialog::AuthenticationKeys &&
            !key.canAuthenticate()) {
        qCDebug(KLEO_UI_LOG) << "key can't authenticate";
        setStatusString(i18n("The key is not designated for authentication."));
        return false;
    }

    if (keyUsage & Kleo::KeySelectionDialog::SecretKeys &&
            !(keyUsage & Kleo::KeySelectionDialog::PublicKeys) &&
            !key.hasSecret()) {
        qCDebug(KLEO_UI_LOG) << "key isn't secret";
        setStatusString(i18n("The key is not secret."));
        return false;
    }

    if (keyUsage & Kleo::KeySelectionDialog::TrustedKeys &&
            key.protocol() == GpgME::OpenPGP &&
            // only check this for secret keys for now.
            // Seems validity isn't checked for secret keylistings...
            !key.hasSecret()) {
        std::vector<GpgME::UserID> uids = key.userIDs();
        for (std::vector<GpgME::UserID>::const_iterator it = uids.begin(); it != uids.end(); ++it)
            if (!it->isRevoked() && it->validity() >= GpgME::UserID::Marginal) {
                return true;
            }
        qCDebug(KLEO_UI_LOG) << "key has no UIDs with validity >= Marginal";
        setStatusString(i18n("The key is not trusted enough."));
        return false;
    }
    // X.509 keys are always trusted, else they won't be the keybox.
    // PENDING(marc) check that this ^ is correct

    setStatusString(i18n("The key can be used."));
    return true;
}

static bool checkKeyUsage(const std::vector<GpgME::Key> &keys, unsigned int keyUsage)
{
    for (std::vector<GpgME::Key>::const_iterator it = keys.begin(); it != keys.end(); ++it)
        if (!checkKeyUsage(*it, keyUsage)) {
            return false;
        }
    return true;
}

static inline QString time_t2string(time_t t)
{
    QDateTime dt;
    dt.setTime_t(t);
    return dt.toString();
}

namespace
{

class ColumnStrategy : public Kleo::KeyListView::ColumnStrategy
{
public:
    ColumnStrategy(unsigned int keyUsage);

    QString title(int col) const override;
    int width(int col, const QFontMetrics &fm) const override;

    QString text(const GpgME::Key &key, int col) const override;
    QString toolTip(const GpgME::Key &key, int col) const override;
    QIcon icon(const GpgME::Key &key, int col) const override;

private:
    const QIcon mKeyGoodPix, mKeyBadPix, mKeyUnknownPix, mKeyValidPix;
    const unsigned int mKeyUsage;
};

static QString iconPath(const QString &name)
{
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("libkleopatra/pics/") + name + QStringLiteral(".png"));
}

ColumnStrategy::ColumnStrategy(unsigned int keyUsage)
    : Kleo::KeyListView::ColumnStrategy(),
      mKeyGoodPix(iconPath(QStringLiteral("key_ok"))),
      mKeyBadPix(iconPath(QStringLiteral("key_bad"))),
      mKeyUnknownPix(iconPath(QStringLiteral("key_unknown"))),
      mKeyValidPix(iconPath(QStringLiteral("key"))),
      mKeyUsage(keyUsage)
{
    if (keyUsage == 0)
        qCWarning(KLEO_UI_LOG)
                << "KeySelectionDialog: keyUsage == 0. You want to use AllKeys instead.";
}

QString ColumnStrategy::title(int col) const
{
    switch (col) {
    case 0: return i18n("Key ID");
    case 1: return i18n("User ID");
    default: return QString();
    }
}

int ColumnStrategy::width(int col, const QFontMetrics &fm) const
{
    if (col == 0) {
        static const char hexchars[] = "0123456789ABCDEF";
        int maxWidth = 0;
        for (unsigned int i = 0; i < 16; ++i) {
            maxWidth = qMax(fm.width(QLatin1Char(hexchars[i])), maxWidth);
        }
        return 8 * maxWidth + 2 * 16 /* KIconLoader::SizeSmall */;
    }
    return Kleo::KeyListView::ColumnStrategy::width(col, fm);
}

QString ColumnStrategy::text(const GpgME::Key &key, int col) const
{
    switch (col) {
    case 0: {
        if (key.shortKeyID()) {
            return QString::fromUtf8(key.shortKeyID());
        } else {
            return xi18n("<placeholder>unknown</placeholder>");
        }
    }
    break;
    case 1: {
        const char *uid = key.userID(0).id();
        if (key.protocol() == GpgME::OpenPGP) {
            return uid && *uid ? QString::fromUtf8(uid) : QString();
        } else { // CMS
            return Kleo::DN(uid).prettyDN();
        }
    }
    break;
    default: return QString();
    }
}

QString ColumnStrategy::toolTip(const GpgME::Key &key, int) const
{
    const char *uid = key.userID(0).id();
    const char *fpr = key.primaryFingerprint();
    const char *issuer = key.issuerName();
    const GpgME::Subkey subkey = key.subkey(0);
    const QString expiry = subkey.neverExpires() ? i18n("never") : time_t2string(subkey.expirationTime());
    const QString creation = time_t2string(subkey.creationTime());
    QString keyStatusString;
    if (!checkKeyUsage(key, mKeyUsage, &keyStatusString)) {
        // Show the status in bold if there is a problem
        keyStatusString = QLatin1String("<b>") % keyStatusString % QLatin1String("</b>");
    }

    QString html = QStringLiteral("<qt><p style=\"style='white-space:pre'\">");
    if (key.protocol() == GpgME::OpenPGP) {
        html += i18n("OpenPGP key for <b>%1</b>", uid ? QString::fromUtf8(uid) : i18n("unknown"));
    } else {
        html += i18n("S/MIME key for <b>%1</b>", uid ? Kleo::DN(uid).prettyDN() : i18n("unknown"));
    }
    html += QStringLiteral("</p><table>");

    const auto addRow = [&html](const QString &name, const QString &value) {
        html += QStringLiteral("<tr><td align=\"right\"><b>%1: </b></td><td>%2</td></tr>").arg(name, value);
    };
    addRow(i18nc("Key creation date", "Created"), creation);
    addRow(i18nc("Key Expiration date", "Expiry"), expiry);
    addRow(i18nc("Key fingerprint", "Fingerprint"), fpr ? QString::fromLatin1(fpr) : i18n("unknown"));
    if (key.protocol() != GpgME::OpenPGP) {
        addRow(i18nc("Key issuer", "Issuer"), issuer ? Kleo::DN(issuer).prettyDN() : i18n("unknown"));
    }
    addRow(i18nc("Key status", "Status"), keyStatusString);
    html += QStringLiteral("</table></qt>");

    return html;
}

QIcon ColumnStrategy::icon(const GpgME::Key &key, int col) const
{
    if (col != 0) {
        return QIcon();
    }
    // this key did not undergo a validating keylisting yet:
    if (!(key.keyListMode() & GpgME::Validate)) {
        return mKeyUnknownPix;
    }

    if (!checkKeyUsage(key, mKeyUsage)) {
        return mKeyBadPix;
    }

    if (key.protocol() == GpgME::CMS) {
        return mKeyGoodPix;
    }

    switch (key.userID(0).validity()) {
    default:
    case GpgME::UserID::Unknown:
    case GpgME::UserID::Undefined:
        return mKeyUnknownPix;
    case GpgME::UserID::Never:
        return mKeyValidPix;
    case GpgME::UserID::Marginal:
    case GpgME::UserID::Full:
    case GpgME::UserID::Ultimate:
        return mKeyGoodPix;
    }
}

}

static const int sCheckSelectionDelay = 250;

Kleo::KeySelectionDialog::KeySelectionDialog(const QString &title,
        const QString &text,
        const std::vector<GpgME::Key> &selectedKeys,
        unsigned int keyUsage,
        bool extendedSelection,
        bool rememberChoice,
        QWidget *parent,
        bool modal)
    : QDialog(parent),
      mOpenPGPBackend(nullptr),
      mSMIMEBackend(nullptr),
      mRememberCB(nullptr),
      mSelectedKeys(selectedKeys),
      mKeyUsage(keyUsage),
      mCurrentContextMenuItem(nullptr)
{
    setWindowTitle(title);
    setModal(modal);
    init(rememberChoice, extendedSelection, text, QString());
}

Kleo::KeySelectionDialog::KeySelectionDialog(const QString &title,
        const QString &text,
        const QString &initialQuery,
        const std::vector<GpgME::Key> &selectedKeys,
        unsigned int keyUsage,
        bool extendedSelection,
        bool rememberChoice,
        QWidget *parent,
        bool modal)
    : QDialog(parent),
      mOpenPGPBackend(nullptr),
      mSMIMEBackend(nullptr),
      mRememberCB(nullptr),
      mSelectedKeys(selectedKeys),
      mKeyUsage(keyUsage),
      mSearchText(initialQuery),
      mInitialQuery(initialQuery),
      mCurrentContextMenuItem(nullptr)
{
    setWindowTitle(title);
    setModal(modal);
    init(rememberChoice, extendedSelection, text, initialQuery);
}

Kleo::KeySelectionDialog::KeySelectionDialog(const QString &title,
        const QString &text,
        const QString &initialQuery,
        unsigned int keyUsage,
        bool extendedSelection,
        bool rememberChoice,
        QWidget *parent,
        bool modal)
    : QDialog(parent),
      mOpenPGPBackend(nullptr),
      mSMIMEBackend(nullptr),
      mRememberCB(nullptr),
      mKeyUsage(keyUsage),
      mSearchText(initialQuery),
      mInitialQuery(initialQuery),
      mCurrentContextMenuItem(nullptr)
{
    setWindowTitle(title);
    setModal(modal);
    init(rememberChoice, extendedSelection, text, initialQuery);
}

void Kleo::KeySelectionDialog::init(bool rememberChoice, bool extendedSelection,
                                    const QString &text, const QString &initialQuery)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mOkButton = buttonBox->button(QDialogButtonBox::Ok);
    mOkButton->setDefault(true);
    mOkButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    QPushButton *user1Button = new QPushButton;
    buttonBox->addButton(user1Button, QDialogButtonBox::ActionRole);
    QPushButton *user2Button = new QPushButton;
    buttonBox->addButton(user2Button, QDialogButtonBox::ActionRole);

    if (mKeyUsage & OpenPGPKeys) {
        mOpenPGPBackend = QGpgME::openpgp();
    }
    if (mKeyUsage & SMIMEKeys) {
        mSMIMEBackend = QGpgME::smime();
    }

    mCheckSelectionTimer = new QTimer(this);
    mStartSearchTimer = new QTimer(this);

    QFrame *page = new QFrame(this);
    mainLayout->addWidget(page);
    mainLayout->addWidget(buttonBox);

    mTopLayout = new QVBoxLayout(page);
    mTopLayout->setMargin(0);

    if (!text.isEmpty()) {
        QLabel *textLabel = new QLabel(text, page);
        textLabel->setWordWrap(true);

        // Setting the size policy is necessary as a workaround for https://issues.kolab.org/issue4429
        // and http://bugreports.qt.nokia.com/browse/QTBUG-8740
        textLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        connect(textLabel, &QLabel::linkActivated, this, &KeySelectionDialog::slotStartCertificateManager);
        mTopLayout->addWidget(textLabel);
    }

    QPushButton *const searchExternalPB =
        new QPushButton(i18n("Search for &External Certificates"), page);
    mTopLayout->addWidget(searchExternalPB, 0, Qt::AlignLeft);
    connect(searchExternalPB, &QAbstractButton::clicked, this, &KeySelectionDialog::slotStartSearchForExternalCertificates);
    if (initialQuery.isEmpty()) {
        searchExternalPB->hide();
    }

    QHBoxLayout *hlay = new QHBoxLayout();
    mTopLayout->addLayout(hlay);

    QLineEdit *le = new QLineEdit(page);
    le->setClearButtonEnabled(true);
    le->setText(initialQuery);

    QLabel *lbSearchFor =  new QLabel(i18n("&Search for:"), page);
    lbSearchFor->setBuddy(le);

    hlay->addWidget(lbSearchFor);
    hlay->addWidget(le, 1);
    le->setFocus();

    connect(le, &QLineEdit::textChanged,
            this, static_cast<void(KeySelectionDialog::*)(const QString &)>(&KeySelectionDialog::slotSearch));
    connect(mStartSearchTimer, &QTimer::timeout, this, &KeySelectionDialog::slotFilter);

    mKeyListView = new KeyListView(new ColumnStrategy(mKeyUsage), nullptr, page);
    mKeyListView->setObjectName(QStringLiteral("mKeyListView"));
    mKeyListView->header()->stretchLastSection();
    mKeyListView->setRootIsDecorated(true);
    mKeyListView->setSortingEnabled(true);
    mKeyListView->header()->setSortIndicatorShown(true);
    mKeyListView->header()->setSortIndicator(1, Qt::AscendingOrder);   // sort by User ID
    if (extendedSelection) {
        mKeyListView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
    mTopLayout->addWidget(mKeyListView, 10);

    if (rememberChoice) {
        mRememberCB = new QCheckBox(i18n("&Remember choice"), page);
        mTopLayout->addWidget(mRememberCB);
        mRememberCB->setWhatsThis(
            i18n("<qt><p>If you check this box your choice will "
                 "be stored and you will not be asked again."
                 "</p></qt>"));
    }

    connect(mCheckSelectionTimer, &QTimer::timeout,
            this, static_cast<void(KeySelectionDialog::*)()>(&KeySelectionDialog::slotCheckSelection));
    connectSignals();

    connect(mKeyListView, &Kleo::KeyListView::doubleClicked, this, &KeySelectionDialog::slotTryOk);
    connect(mKeyListView, &KeyListView::contextMenu, this, &KeySelectionDialog::slotRMB);

    user1Button->setText(i18n("&Reread Keys"));
    user2Button->setText(i18n("&Start Certificate Manager"));
    connect(user1Button, &QPushButton::clicked, this, &KeySelectionDialog::slotRereadKeys);
    connect(user2Button, &QPushButton::clicked, this, [this]() { slotStartCertificateManager(); });
    connect(mOkButton, &QPushButton::clicked, this, &KeySelectionDialog::slotOk);
    connect(buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &KeySelectionDialog::slotCancel);
    slotRereadKeys();
    mTopLayout->activate();

    if (qApp) {
        QSize dialogSize(sizeHint());
        KWindowSystem::setIcons(winId(), qApp->windowIcon().pixmap(32, 32),
                                qApp->windowIcon().pixmap(16, 16));

        KConfigGroup dialogConfig(KSharedConfig::openConfig(), "Key Selection Dialog");
        dialogSize = dialogConfig.readEntry("Dialog size", dialogSize);
        const QByteArray headerState = dialogConfig.readEntry("header", QByteArray());
        if (!headerState.isEmpty()) {
            mKeyListView->header()->restoreState(headerState);
        }
        resize(dialogSize);
    }
}

Kleo::KeySelectionDialog::~KeySelectionDialog()
{
    disconnectSignals();
    KConfigGroup dialogConfig(KSharedConfig::openConfig(), "Key Selection Dialog");
    dialogConfig.writeEntry("Dialog size", size());
    dialogConfig.writeEntry("header", mKeyListView->header()->saveState());
    dialogConfig.sync();
}

void Kleo::KeySelectionDialog::connectSignals()
{
    if (mKeyListView->isMultiSelection())
        connect(mKeyListView, &QTreeWidget::itemSelectionChanged, this, &KeySelectionDialog::slotSelectionChanged);
    else
        connect(mKeyListView, static_cast<void(KeyListView::*)(KeyListViewItem*)>(&KeyListView::selectionChanged),
                this, static_cast<void(KeySelectionDialog::*)(KeyListViewItem*)>(&KeySelectionDialog::slotCheckSelection));
}

void Kleo::KeySelectionDialog::disconnectSignals()
{
    if (mKeyListView->isMultiSelection())
        disconnect(mKeyListView, &QTreeWidget::itemSelectionChanged,
                   this, &KeySelectionDialog::slotSelectionChanged);
    else
        disconnect(mKeyListView, static_cast<void(KeyListView::*)(KeyListViewItem*)>(&KeyListView::selectionChanged),
                   this, static_cast<void(KeySelectionDialog::*)(KeyListViewItem*)>(&KeySelectionDialog::slotCheckSelection));
}

const GpgME::Key &Kleo::KeySelectionDialog::selectedKey() const
{
    static const GpgME::Key null = GpgME::Key::null;
    if (mKeyListView->isMultiSelection() || !mKeyListView->selectedItem()) {
        return null;
    }
    return mKeyListView->selectedItem()->key();
}

QString Kleo::KeySelectionDialog::fingerprint() const
{
    return QLatin1String(selectedKey().primaryFingerprint());
}

QStringList Kleo::KeySelectionDialog::fingerprints() const
{
    QStringList result;
    for (std::vector<GpgME::Key>::const_iterator it = mSelectedKeys.begin(); it != mSelectedKeys.end(); ++it)
        if (const char *fpr = it->primaryFingerprint()) {
            result.push_back(QLatin1String(fpr));
        }
    return result;
}

QStringList Kleo::KeySelectionDialog::pgpKeyFingerprints() const
{
    QStringList result;
    for (std::vector<GpgME::Key>::const_iterator it = mSelectedKeys.begin(); it != mSelectedKeys.end(); ++it)
        if (it->protocol() == GpgME::OpenPGP)
            if (const char *fpr = it->primaryFingerprint()) {
                result.push_back(QLatin1String(fpr));
            }
    return result;
}

QStringList Kleo::KeySelectionDialog::smimeFingerprints() const
{
    QStringList result;
    for (std::vector<GpgME::Key>::const_iterator it = mSelectedKeys.begin(); it != mSelectedKeys.end(); ++it)
        if (it->protocol() == GpgME::CMS)
            if (const char *fpr = it->primaryFingerprint()) {
                result.push_back(QLatin1String(fpr));
            }
    return result;
}

void Kleo::KeySelectionDialog::slotRereadKeys()
{
    mKeyListView->clear();
    mListJobCount = 0;
    mTruncated = 0;
    mSavedOffsetY = mKeyListView->verticalScrollBar()->value();

    disconnectSignals();
    mKeyListView->setEnabled(false);

    // FIXME: save current selection
    if (mOpenPGPBackend) {
        startKeyListJobForBackend(mOpenPGPBackend, std::vector<GpgME::Key>(), false /*non-validating*/);
    }
    if (mSMIMEBackend) {
        startKeyListJobForBackend(mSMIMEBackend, std::vector<GpgME::Key>(), false /*non-validating*/);
    }

    if (mListJobCount == 0) {
        mKeyListView->setEnabled(true);
        KMessageBox::information(this,
                                 i18n("No backends found for listing keys. "
                                      "Check your installation."),
                                 i18n("Key Listing Failed"));
        connectSignals();
    }
}

void Kleo::KeySelectionDialog::slotStartCertificateManager(const QString &query)
{
    QStringList args;

    if (!query.isEmpty()) {
        args << QStringLiteral("--search") << query;
    }
    if (!QProcess::startDetached(QStringLiteral("kleopatra"), args))
        KMessageBox::error(this,
                           i18n("Could not start certificate manager; "
                                "please check your installation."),
                           i18n("Certificate Manager Error"));
    else {
        qCDebug(KLEO_UI_LOG) << "\nslotStartCertManager(): certificate manager started.";
    }
}

#ifndef __KLEO_UI_SHOW_KEY_LIST_ERROR_H__
#define __KLEO_UI_SHOW_KEY_LIST_ERROR_H__
static void showKeyListError(QWidget *parent, const GpgME::Error &err)
{
    Q_ASSERT(err);
    const QString msg = i18n("<qt><p>An error occurred while fetching "
                             "the keys from the backend:</p>"
                             "<p><b>%1</b></p></qt>",
                             QString::fromLocal8Bit(err.asString()));

    KMessageBox::error(parent, msg, i18n("Key Listing Failed"));
}
#endif // __KLEO_UI_SHOW_KEY_LIST_ERROR_H__

namespace
{
struct ExtractFingerprint {
    QString operator()(const GpgME::Key &key)
    {
        return QLatin1String(key.primaryFingerprint());
    }
};
}

void Kleo::KeySelectionDialog::startKeyListJobForBackend(const QGpgME::Protocol *backend, const std::vector<GpgME::Key> &keys, bool validate)
{
    Q_ASSERT(backend);
    QGpgME::KeyListJob *job = backend->keyListJob(false, false, validate);    // local, w/o sigs, validation as givem
    if (!job) {
        return;
    }

    connect(job, &QGpgME::KeyListJob::result, this, &KeySelectionDialog::slotKeyListResult);
    if (validate)
        connect(job, &QGpgME::KeyListJob::nextKey, mKeyListView, &KeyListView::slotRefreshKey);
    else
        connect(job, &QGpgME::KeyListJob::nextKey, mKeyListView, &KeyListView::slotAddKey);

    QStringList fprs;
    std::transform(keys.begin(), keys.end(), std::back_inserter(fprs), ExtractFingerprint());
    const GpgME::Error err = job->start(fprs, mKeyUsage & SecretKeys && !(mKeyUsage & PublicKeys));

    if (err) {
        return showKeyListError(this, err);
    }

#ifndef LIBKLEO_NO_PROGRESSDIALOG
    // FIXME: create a MultiProgressDialog:
    (void)new ProgressDialog(job, validate ? i18n("Checking selected keys...") : i18n("Fetching keys..."), this);
#endif
    ++mListJobCount;
}

static void selectKeys(Kleo::KeyListView *klv, const std::vector<GpgME::Key> &selectedKeys)
{
    klv->clearSelection();
    if (selectedKeys.empty()) {
        return;
    }
    for (std::vector<GpgME::Key>::const_iterator it = selectedKeys.begin(); it != selectedKeys.end(); ++it)
        if (Kleo::KeyListViewItem *item = klv->itemByFingerprint(it->primaryFingerprint())) {
            item->setSelected(true);
        }
}

void Kleo::KeySelectionDialog::slotKeyListResult(const GpgME::KeyListResult &res)
{
    if (res.error()) {
        showKeyListError(this, res.error());
    } else if (res.isTruncated()) {
        ++mTruncated;
    }

    if (--mListJobCount > 0) {
        return;    // not yet finished...
    }

    if (mTruncated > 0)
        KMessageBox::information(this,
                                 i18np("<qt>One backend returned truncated output.<p>"
                                       "Not all available keys are shown</p></qt>",
                                       "<qt>%1 backends returned truncated output.<p>"
                                       "Not all available keys are shown</p></qt>",
                                       mTruncated),
                                 i18n("Key List Result"));

    mKeyListView->flushKeys();

    mKeyListView->setEnabled(true);
    mListJobCount = mTruncated = 0;
    mKeysToCheck.clear();

    selectKeys(mKeyListView, mSelectedKeys);

    slotFilter();

    connectSignals();

    slotSelectionChanged();

    // restore the saved position of the contents
    mKeyListView->verticalScrollBar()->setValue(mSavedOffsetY); mSavedOffsetY = 0;
}

void Kleo::KeySelectionDialog::slotSelectionChanged()
{
    qCDebug(KLEO_UI_LOG) << "KeySelectionDialog::slotSelectionChanged()";

    // (re)start the check selection timer. Checking the selection is delayed
    // because else drag-selection doesn't work very good (checking key trust
    // is slow).
    mCheckSelectionTimer->start(sCheckSelectionDelay);
}

namespace
{
struct AlreadyChecked {
    bool operator()(const GpgME::Key &key) const
    {
        return key.keyListMode() & GpgME::Validate;
    }
};
}

void Kleo::KeySelectionDialog::slotCheckSelection(KeyListViewItem *item)
{
    qCDebug(KLEO_UI_LOG) << "KeySelectionDialog::slotCheckSelection()";

    mCheckSelectionTimer->stop();

    mSelectedKeys.clear();

    if (!mKeyListView->isMultiSelection()) {
        if (item) {
            mSelectedKeys.push_back(item->key());
        }
    }

    for (KeyListViewItem *it = mKeyListView->firstChild(); it; it = it->nextSibling())
        if (it->isSelected()) {
            mSelectedKeys.push_back(it->key());
        }

    mKeysToCheck.clear();
    std::remove_copy_if(mSelectedKeys.begin(), mSelectedKeys.end(),
                        std::back_inserter(mKeysToCheck),
                        AlreadyChecked());
    if (mKeysToCheck.empty()) {
        mOkButton->setEnabled(!mSelectedKeys.empty() &&
                              checkKeyUsage(mSelectedKeys, mKeyUsage));
        return;
    }

    // performed all fast checks - now for validating key listing:
    startValidatingKeyListing();
}

void Kleo::KeySelectionDialog::startValidatingKeyListing()
{
    if (mKeysToCheck.empty()) {
        return;
    }

    mListJobCount = 0;
    mTruncated = 0;
    mSavedOffsetY = mKeyListView->verticalScrollBar()->value();

    disconnectSignals();
    mKeyListView->setEnabled(false);

    std::vector<GpgME::Key> smime, openpgp;
    for (std::vector<GpgME::Key>::const_iterator it = mKeysToCheck.begin(); it != mKeysToCheck.end(); ++it)
        if (it->protocol() == GpgME::OpenPGP) {
            openpgp.push_back(*it);
        } else {
            smime.push_back(*it);
        }

    if (!openpgp.empty()) {
        Q_ASSERT(mOpenPGPBackend);
        startKeyListJobForBackend(mOpenPGPBackend, openpgp, true /*validate*/);
    }
    if (!smime.empty()) {
        Q_ASSERT(mSMIMEBackend);
        startKeyListJobForBackend(mSMIMEBackend, smime, true /*validate*/);
    }

    Q_ASSERT(mListJobCount > 0);
}

bool Kleo::KeySelectionDialog::rememberSelection() const
{
    return mRememberCB && mRememberCB->isChecked();
}

void Kleo::KeySelectionDialog::slotRMB(Kleo::KeyListViewItem *item, const QPoint &p)
{
    if (!item) {
        return;
    }

    mCurrentContextMenuItem = item;

    QMenu menu;
    menu.addAction(i18n("Recheck Key"), this, &KeySelectionDialog::slotRecheckKey);
    menu.exec(p);
}

void Kleo::KeySelectionDialog::slotRecheckKey()
{
    if (!mCurrentContextMenuItem || mCurrentContextMenuItem->key().isNull()) {
        return;
    }

    mKeysToCheck.clear();
    mKeysToCheck.push_back(mCurrentContextMenuItem->key());
}

void Kleo::KeySelectionDialog::slotTryOk()
{
    if (!mSelectedKeys.empty() && checkKeyUsage(mSelectedKeys, mKeyUsage)) {
        slotOk();
    }
}

void Kleo::KeySelectionDialog::slotOk()
{
    if (mCheckSelectionTimer->isActive()) {
        slotCheckSelection();
    }
#if 0 //LAurent I don't understand why we returns here.
    // button could be disabled again after checking the selected key1
    if (!mSelectedKeys.empty() && checkKeyUsage(mSelectedKeys, mKeyUsage)) {
        return;
    }
#endif
    mStartSearchTimer->stop();
    accept();
}

void Kleo::KeySelectionDialog::slotCancel()
{
    mCheckSelectionTimer->stop();
    mStartSearchTimer->stop();
    reject();
}

void Kleo::KeySelectionDialog::slotSearch(const QString &text)
{
    mSearchText = text.trimmed().toUpper();
    slotSearch();
}

void Kleo::KeySelectionDialog::slotSearch()
{
    mStartSearchTimer->setSingleShot(true);
    mStartSearchTimer->start(sCheckSelectionDelay);
}

void Kleo::KeySelectionDialog::slotFilter()
{
    if (mSearchText.isEmpty()) {
        showAllItems();
        return;
    }

    // OK, so we need to filter:
    QRegExp keyIdRegExp(QLatin1String("(?:0x)?[A-F0-9]{1,8}"), Qt::CaseInsensitive);
    if (keyIdRegExp.exactMatch(mSearchText)) {
        if (mSearchText.startsWith(QStringLiteral("0X")))
            // search for keyID only:
        {
            filterByKeyID(mSearchText.mid(2));
        } else
            // search for UID and keyID:
        {
            filterByKeyIDOrUID(mSearchText);
        }
    } else {
        // search in UID:
        filterByUID(mSearchText);
    }
}

void Kleo::KeySelectionDialog::filterByKeyID(const QString &keyID)
{
    Q_ASSERT(keyID.length() <= 8);
    Q_ASSERT(!keyID.isEmpty());   // regexp in slotFilter should prevent these
    if (keyID.isEmpty()) {
        showAllItems();
    } else
        for (KeyListViewItem *item = mKeyListView->firstChild(); item; item = item->nextSibling()) {
            item->setHidden(!item->text(0).toUpper().startsWith(keyID));
        }
}

static bool anyUIDMatches(const Kleo::KeyListViewItem *item, QRegExp &rx)
{
    if (!item) {
        return false;
    }

    const std::vector<GpgME::UserID> uids = item->key().userIDs();
    for (std::vector<GpgME::UserID>::const_iterator it = uids.begin(); it != uids.end(); ++it)
        if (it->id() && rx.indexIn(QString::fromUtf8(it->id())) >= 0) {
            return true;
        }
    return false;
}

void Kleo::KeySelectionDialog::filterByKeyIDOrUID(const QString &str)
{
    Q_ASSERT(!str.isEmpty());

    // match beginnings of words:
    QRegExp rx(QLatin1String("\\b") + QRegExp::escape(str), Qt::CaseInsensitive);

    for (KeyListViewItem *item = mKeyListView->firstChild(); item; item = item->nextSibling()) {
        item->setHidden(!item->text(0).toUpper().startsWith(str) && !anyUIDMatches(item, rx));
    }

}

void Kleo::KeySelectionDialog::filterByUID(const QString &str)
{
    Q_ASSERT(!str.isEmpty());

    // match beginnings of words:
    QRegExp rx(QLatin1String("\\b") + QRegExp::escape(str), Qt::CaseInsensitive);

    for (KeyListViewItem *item = mKeyListView->firstChild(); item; item = item->nextSibling()) {
        item->setHidden(!anyUIDMatches(item, rx));
    }
}

void Kleo::KeySelectionDialog::showAllItems()
{
    for (KeyListViewItem *item = mKeyListView->firstChild(); item; item = item->nextSibling()) {
        item->setHidden(false);
    }
}

