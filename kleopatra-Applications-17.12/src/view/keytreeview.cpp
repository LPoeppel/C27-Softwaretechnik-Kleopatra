/* -*- mode: c++; c-basic-offset:4 -*-
    view/keytreeview.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2009 Klarälvdalens Datakonsult AB

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

#include <config-kleopatra.h>

#include "keytreeview.h"

#include <Libkleo/KeyListModel>
#include <Libkleo/KeyListSortFilterProxyModel>
#include <Libkleo/KeyRearrangeColumnsProxyModel>
#include <Libkleo/Predicates>

#include <utils/headerview.h>

#include <Libkleo/Stl_Util>
#include <Libkleo/KeyFilter>

#include "kleopatra_debug.h"
#include <QTreeView>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QItemSelection>
#include <QLayout>


using namespace Kleo;
using namespace GpgME;

namespace
{

class TreeView : public QTreeView
{
public:
    explicit TreeView(QWidget *parent = nullptr) : QTreeView(parent) {}

    QSize minimumSizeHint() const override
    {
        const QSize min = QTreeView::minimumSizeHint();
        return QSize(min.width(), min.height() + 5 * fontMetrics().height());
    }
};

} // anon namespace

KeyTreeView::KeyTreeView(QWidget *parent)
    : QWidget(parent),
      m_proxy(new KeyListSortFilterProxyModel(this)),
      m_additionalProxy(nullptr),
      m_view(new TreeView(this)),
      m_flatModel(nullptr),
      m_hierarchicalModel(nullptr),
      m_stringFilter(),
      m_keyFilter(),
      m_isHierarchical(true)
{
    init();
}

KeyTreeView::KeyTreeView(const KeyTreeView &other)
    : QWidget(nullptr),
      m_proxy(new KeyListSortFilterProxyModel(this)),
      m_additionalProxy(other.m_additionalProxy ? other.m_additionalProxy->clone() : nullptr),
      m_view(new TreeView(this)),
      m_flatModel(other.m_flatModel),
      m_hierarchicalModel(other.m_hierarchicalModel),
      m_stringFilter(other.m_stringFilter),
      m_keyFilter(other.m_keyFilter),
      m_isHierarchical(other.m_isHierarchical)
{
    init();
    setColumnSizes(other.columnSizes());
    setSortColumn(other.sortColumn(), other.sortOrder());
}

KeyTreeView::KeyTreeView(const QString &text, const std::shared_ptr<KeyFilter> &kf, AbstractKeyListSortFilterProxyModel *proxy, QWidget *parent)
    : QWidget(parent),
      m_proxy(new KeyListSortFilterProxyModel(this)),
      m_additionalProxy(proxy),
      m_view(new TreeView(this)),
      m_flatModel(nullptr),
      m_hierarchicalModel(nullptr),
      m_stringFilter(text),
      m_keyFilter(kf),
      m_isHierarchical(true)
{
    init();
}

void KeyTreeView::setColumnSizes(const std::vector<int> &sizes)
{
    if (sizes.empty()) {
        return;
    }
    Q_ASSERT(m_view);
    Q_ASSERT(m_view->header());
    Q_ASSERT(qobject_cast<HeaderView *>(m_view->header()) == static_cast<HeaderView *>(m_view->header()));
    if (HeaderView *const hv = static_cast<HeaderView *>(m_view->header())) {
        hv->setSectionSizes(sizes);
    }
}

void KeyTreeView::setSortColumn(int sortColumn, Qt::SortOrder sortOrder)
{
    Q_ASSERT(m_view);
    m_view->sortByColumn(sortColumn, sortOrder);
}

int KeyTreeView::sortColumn() const
{
    Q_ASSERT(m_view);
    Q_ASSERT(m_view->header());
    return m_view->header()->sortIndicatorSection();
}

Qt::SortOrder KeyTreeView::sortOrder() const
{
    Q_ASSERT(m_view);
    Q_ASSERT(m_view->header());
    return m_view->header()->sortIndicatorOrder();
}

std::vector<int> KeyTreeView::columnSizes() const
{
    Q_ASSERT(m_view);
    Q_ASSERT(m_view->header());
    Q_ASSERT(qobject_cast<HeaderView *>(m_view->header()) == static_cast<HeaderView *>(m_view->header()));
    if (HeaderView *const hv = static_cast<HeaderView *>(m_view->header())) {
        return hv->sectionSizes();
    } else {
        return std::vector<int>();
    }
}

void KeyTreeView::init()
{
    KDAB_SET_OBJECT_NAME(m_proxy);
    KDAB_SET_OBJECT_NAME(m_view);
    if (m_additionalProxy && m_additionalProxy->objectName().isEmpty()) {
        KDAB_SET_OBJECT_NAME(m_additionalProxy);
    }

    QLayout *layout = new QVBoxLayout(this);
    KDAB_SET_OBJECT_NAME(layout);
    layout->setMargin(0);
    layout->addWidget(m_view);

    HeaderView *headerView = new HeaderView(Qt::Horizontal);
    KDAB_SET_OBJECT_NAME(headerView);
    m_view->setHeader(headerView);

    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    //m_view->setAlternatingRowColors( true );
    m_view->setAllColumnsShowFocus(true);
    m_view->setSortingEnabled(true);

    qDebug() << model();
    if (model()) {
        if (m_additionalProxy) {
            m_additionalProxy->setSourceModel(model());
        } else {
            m_proxy->setSourceModel(model());
        }
    }
    if (m_additionalProxy) {
        m_proxy->setSourceModel(m_additionalProxy);
        if (!m_additionalProxy->parent()) {
            m_additionalProxy->setParent(this);
        }
    }

    m_proxy->setFilterFixedString(m_stringFilter);
    m_proxy->setKeyFilter(m_keyFilter);
    KeyRearrangeColumnsProxyModel *rearangingModel = new KeyRearrangeColumnsProxyModel(this);
    rearangingModel->setSourceModel(m_proxy);
    /* TODO: Make this configurable by the user. E.g. kdepim/src/todo/todoview.cpp */
    rearangingModel->setSourceColumns(QVector<int>() << KeyListModelInterface::PrettyName
                                                     << KeyListModelInterface::PrettyEMail
                                                     << KeyListModelInterface::Validity
                                                     << KeyListModelInterface::ValidFrom
                                                     << KeyListModelInterface::ValidUntil
                                                     << KeyListModelInterface::TechnicalDetails
                                                     << KeyListModelInterface::ShortKeyID);
    m_view->setModel(rearangingModel);

    std::vector<int> defaultSizes;
    defaultSizes.push_back(280);
    defaultSizes.push_back(280);
    defaultSizes.push_back(120);
    defaultSizes.push_back(100);
    defaultSizes.push_back(100);
    defaultSizes.push_back(80);
    defaultSizes.push_back(80);
    setColumnSizes(defaultSizes);
}

KeyTreeView::~KeyTreeView() {}

static QAbstractProxyModel *find_last_proxy(QAbstractProxyModel *pm)
{
    Q_ASSERT(pm);
    while (QAbstractProxyModel *const sm = qobject_cast<QAbstractProxyModel *>(pm->sourceModel())) {
        pm = sm;
    }
    return pm;
}

void KeyTreeView::setFlatModel(AbstractKeyListModel *model)
{
    if (model == m_flatModel) {
        return;
    }
    m_flatModel = model;
    if (!m_isHierarchical)
        // TODO: this fails when called after setHierarchicalView( false )...
    {
        find_last_proxy(m_proxy)->setSourceModel(model);
    }
}

void KeyTreeView::setHierarchicalModel(AbstractKeyListModel *model)
{
    if (model == m_hierarchicalModel) {
        return;
    }
    m_hierarchicalModel = model;
    if (m_isHierarchical) {
        find_last_proxy(m_proxy)->setSourceModel(model);
        m_view->expandAll();
        for (int column = 0; column < m_view->header()->count(); ++column) {
            m_view->header()->resizeSection(column, qMax(m_view->header()->sectionSize(column), m_view->header()->sectionSizeHint(column)));
        }
    }
}

void KeyTreeView::setStringFilter(const QString &filter)
{
    if (filter == m_stringFilter) {
        return;
    }
    m_stringFilter = filter;
    m_proxy->setFilterFixedString(filter);
    Q_EMIT stringFilterChanged(filter);
}

void KeyTreeView::setKeyFilter(const std::shared_ptr<KeyFilter> &filter)
{
    if (filter == m_keyFilter || (filter && m_keyFilter && filter->id() == m_keyFilter->id())) {
        return;
    }
    m_keyFilter = filter;
    m_proxy->setKeyFilter(filter);
    Q_EMIT keyFilterChanged(filter);
}

static QItemSelection itemSelectionFromKeys(const std::vector<Key> &keys, const KeyListSortFilterProxyModel &proxy)
{
    QItemSelection result;
    for (const Key &key : keys) {
        const QModelIndex mi = proxy.index(key);
        if (mi.isValid()) {
            result.merge(QItemSelection(mi, mi), QItemSelectionModel::Select);
        }
    }
    return result;
}

void KeyTreeView::selectKeys(const std::vector<Key> &keys)
{
    m_view->selectionModel()->select(itemSelectionFromKeys(keys, *m_proxy), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

std::vector<Key> KeyTreeView::selectedKeys() const
{
    return m_proxy->keys(m_view->selectionModel()->selectedRows());
}

void KeyTreeView::setHierarchicalView(bool on)
{
    if (on == m_isHierarchical) {
        return;
    }
    if (on && !hierarchicalModel()) {
        qCWarning(KLEOPATRA_LOG) <<  "hierarchical view requested, but no hierarchical model set";
        return;
    }
    if (!on && !flatModel()) {
        qCWarning(KLEOPATRA_LOG) << "flat view requested, but no flat model set";
        return;
    }
    const std::vector<Key> selectedKeys = m_proxy->keys(m_view->selectionModel()->selectedRows());
    const Key currentKey = m_proxy->key(m_view->currentIndex());

    m_isHierarchical = on;
    find_last_proxy(m_proxy)->setSourceModel(model());
    if (on) {
        m_view->expandAll();
    }
    selectKeys(selectedKeys);
    if (!currentKey.isNull()) {
        const QModelIndex currentIndex = m_proxy->index(currentKey);
        if (currentIndex.isValid()) {
            m_view->selectionModel()->setCurrentIndex(m_proxy->index(currentKey), QItemSelectionModel::NoUpdate);
            m_view->scrollTo(currentIndex);
        }
    }
    Q_EMIT hierarchicalChanged(on);
}

void KeyTreeView::setKeys(const std::vector<Key> &keys)
{
    std::vector<Key> sorted = keys;
    _detail::sort_by_fpr(sorted);
    _detail::remove_duplicates_by_fpr(sorted);
    m_keys = sorted;
    if (m_flatModel) {
        m_flatModel->setKeys(sorted);
    }
    if (m_hierarchicalModel) {
        m_hierarchicalModel->setKeys(sorted);
    }
    if (!sorted.empty())
        if (QHeaderView *const hv = m_view ? m_view->header() : nullptr) {
            hv->resizeSections(QHeaderView::ResizeToContents);
        }
}

void KeyTreeView::addKeysImpl(const std::vector<Key> &keys, bool select)
{
    if (keys.empty()) {
        return;
    }
    if (m_keys.empty()) {
        setKeys(keys);
        return;
    }

    std::vector<Key> sorted = keys;
    _detail::sort_by_fpr(sorted);
    _detail::remove_duplicates_by_fpr(sorted);

    std::vector<Key> newKeys = _detail::union_by_fpr(sorted, m_keys);
    m_keys.swap(newKeys);

    if (m_flatModel) {
        m_flatModel->addKeys(sorted);
    }
    if (m_hierarchicalModel) {
        m_hierarchicalModel->addKeys(sorted);
    }

    if (select) {
        selectKeys(sorted);
    }
}

void KeyTreeView::addKeysSelected(const std::vector<Key> &keys)
{
    addKeysImpl(keys, true);
}

void KeyTreeView::addKeysUnselected(const std::vector<Key> &keys)
{
    addKeysImpl(keys, false);
}

void KeyTreeView::removeKeys(const std::vector<Key> &keys)
{
    if (keys.empty()) {
        return;
    }
    std::vector<Key> sorted = keys;
    _detail::sort_by_fpr(sorted);
    _detail::remove_duplicates_by_fpr(sorted);
    std::vector<Key> newKeys;
    newKeys.reserve(m_keys.size());
    std::set_difference(m_keys.begin(), m_keys.end(),
                        sorted.begin(), sorted.end(),
                        std::back_inserter(newKeys),
                        _detail::ByFingerprint<std::less>());
    m_keys.swap(newKeys);

    if (m_flatModel) {
        std::for_each(sorted.cbegin(), sorted.cend(),
                      [this](const Key &key) { m_flatModel->removeKey(key); });
    }
    if (m_hierarchicalModel) {
        std::for_each(sorted.cbegin(), sorted.cend(),
                      [this](const Key &key) { m_hierarchicalModel->removeKey(key); });
    }

}

static const struct {
    const char *signal;
    const char *slot;
} connections[] = {
    {
        SIGNAL(stringFilterChanged(QString)),
        SLOT(setStringFilter(QString))
    },
    {
        SIGNAL(keyFilterChanged(std::shared_ptr<Kleo::KeyFilter>)),
        SLOT(setKeyFilter(std::shared_ptr<Kleo::KeyFilter>))
    },
};
static const unsigned int numConnections = sizeof connections / sizeof * connections;

void KeyTreeView::disconnectSearchBar(const QObject *bar)
{
    for (unsigned int i = 0; i < numConnections; ++i) {
        disconnect(this, connections[i].signal, bar,  connections[i].slot);
        disconnect(bar,  connections[i].signal, this, connections[i].slot);
    }
}

bool KeyTreeView::connectSearchBar(const QObject *bar)
{
    for (unsigned int i = 0; i < numConnections; ++i)
        if (!connect(this, connections[i].signal, bar,  connections[i].slot) ||
                !connect(bar,  connections[i].signal, this, connections[i].slot)) {
            return false;
        }
    return true;
}

