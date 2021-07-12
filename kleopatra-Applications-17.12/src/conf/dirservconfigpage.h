/* -*- mode: c++; c-basic-offset:4 -*-
    conf/dirservconfigpage.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2004,2008 Klarälvdalens Datakonsult AB

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

#ifndef DIRSERVCONFIGPAGE_H
#define DIRSERVCONFIGPAGE_H

#include <KCModule>

#include <QGpgME/CryptoConfig>

class QCheckBox;
class QLabel;
class QTimeEdit;
class QSpinBox;
namespace Kleo
{
class DirectoryServicesWidget;
}

/**
 * "Directory Services" configuration page for kleopatra's configuration dialog
 * The user can configure LDAP servers in this page, to be used for listing/fetching
 * remote certificates in kleopatra.
 */
class DirectoryServicesConfigurationPage : public KCModule
{
    Q_OBJECT
public:
    explicit DirectoryServicesConfigurationPage(QWidget *parent = nullptr, const QVariantList &args = QVariantList());

    void load() override;
    void save() override;
    void defaults() override;

private:
    QGpgME::CryptoConfigEntry *configEntry(const char *componentName,
                                           const char *groupName,
                                           const char *entryName,
                                           QGpgME::CryptoConfigEntry::ArgType argType,
                                           bool isList,
                                           bool showError = true);

    Kleo::DirectoryServicesWidget *mWidget;
    QTimeEdit *mTimeout;
    QSpinBox *mMaxItems;
    QLabel *mMaxItemsLabel;
    QCheckBox *mAddNewServersCB;

    QGpgME::CryptoConfigEntry *mX509ServicesEntry;
    QGpgME::CryptoConfigEntry *mOpenPGPServiceEntry;
    QGpgME::CryptoConfigEntry *mTimeoutConfigEntry;
    QGpgME::CryptoConfigEntry *mMaxItemsConfigEntry;
    QGpgME::CryptoConfigEntry *mAddNewServersConfigEntry;

    QGpgME::CryptoConfig *mConfig;
};

#endif
