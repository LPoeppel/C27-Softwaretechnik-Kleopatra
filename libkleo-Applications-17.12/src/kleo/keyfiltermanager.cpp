/*
    keyfiltermanager.cpp

    This file is part of libkleopatra, the KDE keymanagement library
    Copyright (c) 2004 Klarälvdalens Datakonsult AB

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

#include "keyfiltermanager.h"
#include "kconfigbasedkeyfilter.h"
#include "defaultkeyfilter.h"

#include "stl_util.h"

#include "libkleo_debug.h"
#include "utils/formatting.h"

#include <kconfig.h>
#include <kconfiggroup.h>
#include <KSharedConfig>
#include <KLocalizedString>
#include <QIcon>

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>
#include <QAbstractListModel>
#include <QModelIndex>

#include <algorithm>
#include <vector>
#include <climits>
#include <functional>

using namespace Kleo;
using namespace GpgME;

namespace
{

class Model : public QAbstractListModel
{
    KeyFilterManager::Private *m_keyFilterManagerPrivate;
public:
    explicit Model(KeyFilterManager::Private *p)
        : QAbstractListModel(nullptr), m_keyFilterManagerPrivate(p) {}

    int rowCount(const QModelIndex &) const override;
    QVariant data(const QModelIndex &idx, int role) const override;
    /* upgrade to public */ using QAbstractListModel::reset;
};

class AllCertificatesKeyFilter : public DefaultKeyFilter
{
public:
    AllCertificatesKeyFilter()
        : DefaultKeyFilter()
    {
        setSpecificity(UINT_MAX); // overly high for ordering
        setName(i18n("All Certificates"));
        setId(QStringLiteral("all-certificates"));
        setMatchContexts(Filtering);
    }
};

class MyCertificatesKeyFilter : public DefaultKeyFilter
{
public:
    MyCertificatesKeyFilter()
        : DefaultKeyFilter()
    {
        setHasSecret(Set);
        setSpecificity(UINT_MAX - 1); // overly high for ordering

        setName(i18n("My Certificates"));
        setId(QStringLiteral("my-certificates"));
        setMatchContexts(AnyMatchContext);
        setBold(true);
    }
};

//Custom created
class ForeignCertificatesKeyFilter : public DefaultKeyFilter
{
public:
    ForeignCertificatesKeyFilter()
        : DefaultKeyFilter()
    {
        //setHasSecret(NotSet);
        //setValidity(IsAtMost); //in src/kleo/defaultkeyfilter.cpp Line: 142ff
        setSpecificity(UINT_MAX - 2); // overly high for ordering
        setName(i18n("Foreign Certificates"));
        setId(QStringLiteral("foreign-certificates"));
        setHasSecret(NotSet);
        //setItalic(true);

    }
};

class TrustedCertificatesKeyFilter : public DefaultKeyFilter
{
public:
    TrustedCertificatesKeyFilter()
        : DefaultKeyFilter()
    {
        setRevoked(NotSet);
        setValidity(IsAtLeast);
        setValidityReferenceLevel(UserID::Marginal);
        setSpecificity(UINT_MAX - 3); // overly high for ordering

        setName(i18n("Trusted Certificates"));
        setId(QStringLiteral("trusted-certificates"));
        setMatchContexts(Filtering);
    }
};

class FullCertificatesKeyFilter : public DefaultKeyFilter
{
public:
    FullCertificatesKeyFilter()
        : DefaultKeyFilter()
    {
        setRevoked(NotSet);
        setValidity(IsAtLeast);
        setValidityReferenceLevel(UserID::Full);
        setSpecificity(UINT_MAX - 4);

        setName(i18n("Fully Trusted Certificates"));
        setId(QStringLiteral("full-certificates"));
        setMatchContexts(Filtering);
    }
};

class OtherCertificatesKeyFilter : public DefaultKeyFilter
{
public:
    OtherCertificatesKeyFilter()
        : DefaultKeyFilter()
    {
        setHasSecret(NotSet);
        setValidity(IsAtMost);
        setValidityReferenceLevel(UserID::Never);
        setSpecificity(UINT_MAX - 5); // overly high for ordering

        setName(i18n("Other Certificates"));
        setId(QStringLiteral("other-certificates"));
        setMatchContexts(Filtering);
    }
};


/* This filter selects only VS-NfD-compliant keys if Kleopatra is used in
 * CO_DE_VS mode.  */
class DeVsCompliantKeyFilter : public DefaultKeyFilter
{
public:
    DeVsCompliantKeyFilter()
        : DefaultKeyFilter()
    {
        setName(i18n("VS-NfD-compliant Certificates"));
        setId(QStringLiteral("vs-compliant-certificates"));
        setSpecificity(UINT_MAX - 6); // overly high for ordering
    }
    bool matches (const Key &key, MatchContexts contexts) const override
    {
        return (contexts & Filtering) && Formatting::isKeyDeVs(key);
    }
};

/* This filter selects only invalid keys (i.e. those where not all
 * UIDs are at least fully valid).  */
class KeyNotValidFilter : public DefaultKeyFilter
{
public:
    KeyNotValidFilter()
        : DefaultKeyFilter()
    {
        setName(i18n("Not validated Certificates"));
        setId(QStringLiteral("not-validated-certificates"));
        setSpecificity(UINT_MAX - 6); // overly high for ordering
    }
    bool matches (const Key &key, MatchContexts contexts) const override
    {
        return (contexts & Filtering) && !Formatting::uidsHaveFullValidity(key);
    }
};

/* This filter gives valid keys (i.e. those where all UIDs are at
 * least fully valid) a light green background if Kleopatra is used in
 * CO_DE_VS mode.  */
class KeyDeVSValidAppearanceFilter : public DefaultKeyFilter
{
public:
    KeyDeVSValidAppearanceFilter()
        : DefaultKeyFilter()
    {
        // Ideally this would come from KColorScheme but we want to
        // avoid a dependency against kconfigwidgets. So we take
        // the color for positive background from breeze.
        setBgColor(QColor(0xD5, 0xFA,  0xE2));
    }
    bool matches (const Key &key, MatchContexts contexts) const override
    {
        return (contexts & Appearance) && Formatting::uidsHaveFullValidity(key) && Formatting::isKeyDeVs(key);
    }
};

/* This filter gives invalid keys (i.e. those where not all UIDs are
 * at least fully valid) a light red background if Kleopatra is used
 * in CO_DE_VS mode.  */
class KeyNotDeVSValidAppearanceFilter : public DefaultKeyFilter
{
public:
    KeyNotDeVSValidAppearanceFilter()
        : DefaultKeyFilter()
    {
        // Ideally this would come from KColorScheme but we want to
        // avoid a dependency against kconfigwidgets. So we take
        // the color for negative background from breeze.
        setBgColor(QColor(0xFA, 0xE9, 0xEB));
    }
    bool matches (const Key &key, MatchContexts contexts) const override
    {
        return (contexts & Appearance) && (!Formatting::uidsHaveFullValidity(key) || !Formatting::isKeyDeVs(key));
    }
};

}

static std::vector<std::shared_ptr<KeyFilter>> defaultFilters()
{
    std::vector<std::shared_ptr<KeyFilter> > result;
    result.reserve(6);
    result.push_back(std::shared_ptr<KeyFilter>(new MyCertificatesKeyFilter));
    result.push_back(std::shared_ptr<KeyFilter>(new ForeignCertificatesKeyFilter));
    result.push_back(std::shared_ptr<KeyFilter>(new TrustedCertificatesKeyFilter));
    result.push_back(std::shared_ptr<KeyFilter>(new FullCertificatesKeyFilter));
    result.push_back(std::shared_ptr<KeyFilter>(new OtherCertificatesKeyFilter));
    result.push_back(std::shared_ptr<KeyFilter>(new AllCertificatesKeyFilter));
    result.push_back(std::shared_ptr<KeyFilter>(new KeyNotValidFilter));
    if (Formatting::complianceMode() == QStringLiteral("de-vs")) {
        result.push_back(std::shared_ptr<KeyFilter>(new DeVsCompliantKeyFilter));
    }
    return result;
}

static std::vector<std::shared_ptr<KeyFilter>> defaultAppearanceFilters()
{
    std::vector<std::shared_ptr<KeyFilter> > result;
    if (Formatting::complianceMode() == QStringLiteral("de-vs")) {
        result.reserve(2);
        result.push_back(std::shared_ptr<KeyFilter>(new KeyDeVSValidAppearanceFilter));
        result.push_back(std::shared_ptr<KeyFilter>(new KeyNotDeVSValidAppearanceFilter));
    }
    return result;
}

class KeyFilterManager::Private
{
public:
    Private() : filters(), appearanceFilters(), model(this) {}
    void clear()
    {
        filters.clear();
        appearanceFilters.clear();
        model.reset();
    }

    std::vector<std::shared_ptr<KeyFilter>> filters;
    std::vector<std::shared_ptr<KeyFilter>> appearanceFilters;
    Model model;
};

KeyFilterManager *KeyFilterManager::mSelf = nullptr;

KeyFilterManager::KeyFilterManager(QObject *parent)
    : QObject(parent), d(new Private)
{
    mSelf = this;
    // ### DF: doesn't a KStaticDeleter work more reliably?
    if (QCoreApplication *app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, &QObject::deleteLater);
    }
    reload();
}

KeyFilterManager::~KeyFilterManager()
{
    mSelf = nullptr;
    if (d) {
        d->clear();
    }
    delete d; d = nullptr;
}

KeyFilterManager *KeyFilterManager::instance()
{
    if (!mSelf) {
        mSelf = new KeyFilterManager();
    }
    return mSelf;
}

const std::shared_ptr<KeyFilter> &KeyFilterManager::filterMatching(const Key &key, KeyFilter::MatchContexts contexts) const
{
    const auto it = std::find_if(d->filters.cbegin(), d->filters.cend(),
                                 [&key, contexts](const std::shared_ptr<KeyFilter> &filter) {
                                     return filter->matches(key, contexts);
                                 });
    if (it != d->filters.cend()) {
        return *it;
    }
    static const std::shared_ptr<KeyFilter> null;
    return null;
}

std::vector<std::shared_ptr<KeyFilter>> KeyFilterManager::filtersMatching(const Key &key, KeyFilter::MatchContexts contexts) const
{
    std::vector<std::shared_ptr<KeyFilter>> result;
    result.reserve(d->filters.size());
    std::remove_copy_if(d->filters.begin(), d->filters.end(),
                        std::back_inserter(result),
                        [&key, contexts](const std::shared_ptr<KeyFilter> &filter) {
                            return !filter->matches(key, contexts);
                        });
    return result;
}

namespace
{
struct ByDecreasingSpecificity : std::binary_function<std::shared_ptr<KeyFilter>, std::shared_ptr<KeyFilter>, bool> {
    bool operator()(const std::shared_ptr<KeyFilter> &lhs, const std::shared_ptr<KeyFilter> &rhs) const
    {
        return lhs->specificity() > rhs->specificity();
    }
};
}

void KeyFilterManager::reload()
{
    d->clear();

    d->filters = defaultFilters();
    d->appearanceFilters = defaultAppearanceFilters();
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("libkleopatrarc"));

    const QStringList groups = config->groupList().filter(QRegularExpression(QStringLiteral("^Key Filter #\\d+$")));
    for (QStringList::const_iterator it = groups.begin(); it != groups.end(); ++it) {
        const KConfigGroup cfg(config, *it);
        d->filters.push_back(std::shared_ptr<KeyFilter>(new KConfigBasedKeyFilter(cfg)));
    }
    std::stable_sort(d->filters.begin(), d->filters.end(), ByDecreasingSpecificity());
    qCDebug(LIBKLEO_LOG) << "final filter count is" << d->filters.size();
}

QAbstractItemModel *KeyFilterManager::model() const
{
    return &d->model;
}

const std::shared_ptr<KeyFilter> &KeyFilterManager::keyFilterByID(const QString &id) const
{
    const auto it = std::find_if(d->filters.begin(), d->filters.end(),
                                 [id](const std::shared_ptr<KeyFilter> &filter) {
                                    return filter->id() == id;
                                 });
    if (it != d->filters.end()) {
        return *it;
    }
    static const std::shared_ptr<KeyFilter> null;
    return null;
}

const std::shared_ptr<KeyFilter> &KeyFilterManager::fromModelIndex(const QModelIndex &idx) const
{
    if (!idx.isValid() || idx.model() != &d->model || idx.row() < 0 ||
            static_cast<unsigned>(idx.row()) >= d->filters.size()) {
        static const std::shared_ptr<KeyFilter> null;
        return null;
    }
    return d->filters[idx.row()];
}

QModelIndex KeyFilterManager::toModelIndex(const std::shared_ptr<KeyFilter> &kf) const
{
    if (!kf) {
        return QModelIndex();
    }
    const auto pair = std::equal_range(d->filters.cbegin(), d->filters.cend(), kf, ByDecreasingSpecificity());
    const auto it = std::find(pair.first, pair.second, kf);
    if (it != pair.second) {
        return d->model.index(it - d->filters.begin());
    } else {
        return QModelIndex();
    }
}

int Model::rowCount(const QModelIndex &) const
{
    return m_keyFilterManagerPrivate->filters.size();
}

QVariant Model::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid()
        || idx.model() != this
        || idx.row() < 0
        || static_cast<unsigned>(idx.row()) >  m_keyFilterManagerPrivate->filters.size()) {
        return QVariant();
    }

    const auto filter = m_keyFilterManagerPrivate->filters[idx.row()];
    switch (role) {
    case Qt::DecorationRole:
        return filter->icon();

    case Qt::DisplayRole:
    case Qt::EditRole:
    case Qt::ToolTipRole:  /* Most useless tooltip ever.  */
        return filter->name();

    case Qt::UserRole:
        return filter->id();

    default:
        return QVariant();
    }
}

static KeyFilter::FontDescription get_fontdescription(const std::vector<std::shared_ptr<KeyFilter>> &filters, const Key &key, const KeyFilter::FontDescription &initial)
{
    return kdtools::accumulate_if(filters.begin(), filters.end(),
                                  [&key](const std::shared_ptr<KeyFilter> &filter) {
                                      return filter->matches(key, KeyFilter::Appearance);
                                  },
                                  initial,
                                  [](const KeyFilter::FontDescription &lhs,
                                     const std::shared_ptr<KeyFilter> &rhs) {
                                      return lhs.resolve(rhs->fontDescription());
                                  });
}

QFont KeyFilterManager::font(const Key &key, const QFont &baseFont) const
{
    KeyFilter::FontDescription fd;

    fd = get_fontdescription(d->appearanceFilters, key, KeyFilter::FontDescription());
    fd = get_fontdescription(d->filters, key, fd);

    return fd.font(baseFont);
}

static QColor get_color(const std::vector<std::shared_ptr<KeyFilter>> &filters, const Key &key, QColor(KeyFilter::*fun)() const)
{
    const auto it = std::find_if(filters.cbegin(), filters.cend(),
                                 [&fun, &key](const std::shared_ptr<KeyFilter> &filter) {
                                    return filter->matches(key, KeyFilter::Appearance)
                                    && (filter.get()->*fun)().isValid();
                                 });
    if (it == filters.cend()) {
        return QColor();
    } else {
        return (it->get()->*fun)();
    }
}

static QString get_string(const std::vector<std::shared_ptr<KeyFilter>> &filters, const Key &key, QString(KeyFilter::*fun)() const)
{
    const auto it = std::find_if(filters.cbegin(), filters.cend(),
                                 [&fun, &key](const std::shared_ptr<KeyFilter> &filter) {
                                     return filter->matches(key, KeyFilter::Appearance)
                                            && !(filter.get()->*fun)().isEmpty();
                                 });
    if (it == filters.cend()) {
        return QString();
    } else {
        return (*it)->icon();
    }
}

QColor KeyFilterManager::bgColor(const Key &key) const
{
    QColor color;

    color = get_color(d->appearanceFilters, key, &KeyFilter::bgColor);
    if (!color.isValid()) {
        color = get_color(d->filters, key, &KeyFilter::bgColor);
    }
    return color;
}

QColor KeyFilterManager::fgColor(const Key &key) const
{
    QColor color;

    color = get_color(d->appearanceFilters, key, &KeyFilter::fgColor);
    if (!color.isValid()) {
        color = get_color(d->filters, key, &KeyFilter::fgColor);
    }
    return color;
}

QIcon KeyFilterManager::icon(const Key &key) const
{
    QString icon;

    icon = get_string(d->appearanceFilters, key, &KeyFilter::icon);
    if (icon.isEmpty()) {
        icon = get_string(d->filters, key, &KeyFilter::icon);
    }
    return icon.isEmpty() ? QIcon() : QIcon::fromTheme(icon);
}
