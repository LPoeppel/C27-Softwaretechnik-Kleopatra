/*
    test_keylister.cpp

    This file is part of libkleopatra's test suite.
    Copyright (c) 2004 Klarälvdalens Datakonsult AB

    Libkleopatra is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License,
    version 2, as published by the Free Software Foundation.

    Libkleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

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

#include "test_keylister.h"

#include <qgpgme/keylistjob.h>
#include <qgpgme/protocol.h>

#include <gpgme++/keylistresult.h>
#include <gpgme++/key.h>

#include <KAboutData>

#include <QDebug>

#include <QMessageBox>
#include <QStringList>
#include <QTimer>

#include <QApplication>
#include <KLocalizedString>
#include <QCommandLineParser>

namespace
{
class TestColumnStrategy : public Kleo::KeyListView::ColumnStrategy
{
public:
    ~TestColumnStrategy() {}
    QString title(int col) const override;
    QString toolTip(const GpgME::Key &key, int col) const override;
    QString text(const GpgME::Key &key, int col) const override;
};

QString TestColumnStrategy::title(int col) const
{
    switch (col) {
    case 0: return QStringLiteral("Subject");
    case 1: return QStringLiteral("EMail");
    case 2: return QStringLiteral("Issuer");
    case 3: return QStringLiteral("Serial");
    case 4: return QStringLiteral("Protocol");
    case 5: return QStringLiteral("Validity");
    default: return QString();
    }
}

QString TestColumnStrategy::toolTip(const GpgME::Key &key, int) const
{
    return QStringLiteral("Fingerprint: ") + QString::fromUtf8(key.primaryFingerprint());
}

QString TestColumnStrategy::text(const GpgME::Key &key, int col) const
{
    if (key.isNull()) {
        return QStringLiteral("<null>");
    }
    switch (col) {
    case 0: return QString::fromUtf8(key.userID(0).id());
    case 1: return QString::fromUtf8(key.userID(0).email());
    case 2: return QString::fromUtf8(key.issuerName());
    case 3: return QString::fromLatin1(key.issuerSerial());
    case 4: return QString::fromLatin1(key.protocolAsString());
    case 5: return QString(QLatin1Char(key.userID(0).validityAsString()));
    default: return QString();
    }
}
}

CertListView::CertListView(QWidget *parent, Qt::WindowFlags f)
    : Kleo::KeyListView(new TestColumnStrategy(), nullptr, parent, f)
{
    setHierarchical(true);
    setRootIsDecorated(true);
}

CertListView::~CertListView() {}

void CertListView::slotResult(const GpgME::KeyListResult &result)
{
    qDebug() << "CertListView::slotResult()";
    if (result.isNull()) {
        QMessageBox::information(this, QStringLiteral("Key Listing Result"), QStringLiteral("KeyListResult is null!"));
    } else if (result.error())
        QMessageBox::critical(this, QStringLiteral("Key Listing Result"),
                              QStringLiteral("KeyListResult Error: %1").arg(QString::fromLatin1(result.error().asString())));
    else if (result.isTruncated()) {
        QMessageBox::information(this, QStringLiteral("Key Listing Result"), QStringLiteral("KeyListResult is truncated!"));
    } else {
        QMessageBox::information(this, QStringLiteral("Key Listing Result"), QStringLiteral("Key listing successful"));
    }
}

void CertListView::slotStart()
{
    qDebug() << "CertListView::slotStart()";
    QGpgME::KeyListJob *job = QGpgME::smime()->keyListJob(false);
    Q_ASSERT(job);
    QObject::connect(job, &QGpgME::KeyListJob::nextKey, this, &CertListView::slotAddKey);
    QObject::connect(job, &QGpgME::KeyListJob::result, this, &CertListView::slotResult);
#if 0
    QStringList l;
    l << "Marc";
    job->start(l, false);
#else
    job->start(QStringList(), false);
#endif
}

int main(int argc, char **argv)
{

    QApplication app(argc, argv);
    KAboutData aboutData(QStringLiteral("test_keylister"), i18n("KeyLister Test"), QStringLiteral("0.1"));
    QCommandLineParser parser;
    KAboutData::setApplicationData(aboutData);
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    CertListView *clv = new CertListView;
    clv->show();

    QTimer::singleShot(5000, clv, &CertListView::slotStart);

    return app.exec();
}
