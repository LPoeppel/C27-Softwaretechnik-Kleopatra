/* -*- mode: c++; c-basic-offset:4 -*-
    models/subkeylistmodel.cpp

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

#include "subkeylistmodel.h"
#include "utils/formatting.h"

#include <gpgme++/key.h>

#include <KLocalizedString>

#include <QVariant>
#include <QDate>

#include <algorithm>
#include <iterator>

using namespace GpgME;
using namespace Kleo;

class SubkeyListModel::Private
{
    friend class ::Kleo::SubkeyListModel;
    SubkeyListModel *const q;
public:
    explicit Private(SubkeyListModel *qq)
        : q(qq), key() {}

private:
    Key key;
};

SubkeyListModel::SubkeyListModel(QObject *p)
    : QAbstractTableModel(p), d(new Private(this))
{

}

SubkeyListModel::~SubkeyListModel() {}

Key SubkeyListModel::key() const
{
    return d->key;
}

// slot
void SubkeyListModel::setKey(const Key &key)
{

    const Key oldKey = d->key;


    if (qstricmp(key.primaryFingerprint(), oldKey.primaryFingerprint()) != 0) {
        // different key -> reset
        beginResetModel();
        d->key = key;
        endResetModel();
        return;
    }

    d->key = key;

    // ### diff them, and signal more fine-grained than this:

    if (key.numSubkeys() > 0 && oldKey.numSubkeys() == key.numSubkeys()) {
        Q_EMIT dataChanged(index(0, 0), index(key.numSubkeys() - 1, NumColumns - 1));
    } else {
        Q_EMIT layoutAboutToBeChanged();
        Q_EMIT layoutChanged();
    }
}

Subkey SubkeyListModel::subkey(const QModelIndex &idx) const
{
    if (idx.isValid()) {
        return d->key.subkey(idx.row());
    } else {
        return Subkey();
    }
}

std::vector<Subkey> SubkeyListModel::subkeys(const QList<QModelIndex> &indexes) const
{
    std::vector<Subkey> result;
    result.reserve(indexes.size());
    std::transform(indexes.begin(), indexes.end(),
                   std::back_inserter(result),
                   [this](const QModelIndex &idx) {
                       return subkey(idx);
                   });
    return result;
}

QModelIndex SubkeyListModel::index(const Subkey &subkey, int col) const
{
    // O(N), but not sorted, so no better way...
    for (unsigned int row = 0, end = d->key.numSubkeys(); row != end; ++row)
        if (qstricmp(subkey.keyID(), d->key.subkey(row).keyID()) == 0) {
            return index(row, col);
        }
    return QModelIndex();
}

QList<QModelIndex> SubkeyListModel::indexes(const std::vector<Subkey> &subkeys) const
{
    QList<QModelIndex> result;
    result.reserve(subkeys.size());
    // O(N*M), but who cares...?
    std::transform(subkeys.begin(), subkeys.end(),
                   std::back_inserter(result),
                   [this](const Subkey &key) {
                       return index(key);
                   });
    return result;
}

void SubkeyListModel::clear()
{
    beginResetModel();
    d->key = Key::null;
    endResetModel();
}

int SubkeyListModel::columnCount(const QModelIndex &) const
{
    return NumColumns;
}

int SubkeyListModel::rowCount(const QModelIndex &pidx) const
{
    return pidx.isValid() ? 0 : d->key.numSubkeys();
}

QVariant SubkeyListModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o == Qt::Horizontal)
        if (role == Qt::DisplayRole || role == Qt::EditRole || role == Qt::ToolTipRole)
            switch (section) {
            case ID:         return i18n("ID");
            case Type:       return i18n("Type");
            case ValidFrom:  return i18n("Valid From");
            case ValidUntil: return i18n("Valid Until");
            case Status:     return i18n("Status");
            case Strength:   return i18n("Strength");
            case Usage:      return i18n("Usage");
            case NumColumns:;
            }
    return QVariant();
}

QVariant SubkeyListModel::data(const QModelIndex &idx, int role) const
{

    if (role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::ToolTipRole) {
        return QVariant();
    }

    const Subkey subkey = this->subkey(idx);
    if (subkey.isNull()) {
        return QVariant();
    }

    switch (idx.column()) {
    case ID:
        return QString::fromLatin1(subkey.keyID());
    case Type:
        return Formatting::type(subkey);
    case ValidFrom:
        if (role == Qt::EditRole) {
            return Formatting::creationDate(subkey);
        } else {
            return Formatting::creationDateString(subkey);
        }
    case ValidUntil:
        if (role == Qt::EditRole) {
            return Formatting::expirationDate(subkey);
        } else {
            return Formatting::expirationDateString(subkey);
        }
    case Status:
        return Formatting::validityShort(subkey);
    case Usage:
        return Formatting::usageString(subkey);
    case Strength:
        const QString algName = QString::fromStdString(subkey.algoName());
        // For ECC keys the algo name is something like bp512 and directly
        // indicated the "strength"
        return algName.isEmpty() ? QVariant(subkey.length()) : algName;
    }

    return QVariant();
}

