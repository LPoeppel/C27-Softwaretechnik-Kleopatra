/* -*- mode: c++; c-basic-offset:4 -*-
    models/keylistsortfilterproxymodel.h

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
#ifndef __KLEOPATRA_MODELS_KEYLISTSORTFILTERPROXYMODEL_H__
#define __KLEOPATRA_MODELS_KEYLISTSORTFILTERPROXYMODEL_H__

#include <QSortFilterProxyModel>

#include "keylistmodelinterface.h"

#include <kleo_export.h>

#include <memory>

namespace GpgME
{
class Key;
}

namespace Kleo
{

class KeyFilter;

class KLEO_EXPORT AbstractKeyListSortFilterProxyModel : public QSortFilterProxyModel
                                                      , public KeyListModelInterface
{
    Q_OBJECT
protected:
    AbstractKeyListSortFilterProxyModel(const AbstractKeyListSortFilterProxyModel &);
public:
    explicit AbstractKeyListSortFilterProxyModel(QObject *parent = nullptr);
    ~AbstractKeyListSortFilterProxyModel();

    virtual AbstractKeyListSortFilterProxyModel *clone() const = 0;

    GpgME::Key key(const QModelIndex &idx) const override;
    std::vector<GpgME::Key> keys(const QList<QModelIndex> &indexes) const override;

    using QAbstractItemModel::index;
    QModelIndex index(const GpgME::Key &key) const override;
    QList<QModelIndex> indexes(const std::vector<GpgME::Key> &keys) const override;

private:
    void init();
};

class KLEO_EXPORT KeyListSortFilterProxyModel : public AbstractKeyListSortFilterProxyModel
{
    Q_OBJECT
protected:
    KeyListSortFilterProxyModel(const KeyListSortFilterProxyModel &);
public:
    explicit KeyListSortFilterProxyModel(QObject *parent = nullptr);
    ~KeyListSortFilterProxyModel();

    std::shared_ptr<const KeyFilter> keyFilter() const;
    void setKeyFilter(const std::shared_ptr<const KeyFilter> &kf);

    KeyListSortFilterProxyModel *clone() const override;

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    class Private;
    QScopedPointer<Private> const d;
};

}

#endif /* __KLEOPATRA_MODELS_KEYLISTMODEL_H__ */
