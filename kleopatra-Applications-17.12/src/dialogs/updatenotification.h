/*  dialogs/updatenotification.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2017 by Bundesamt für Sicherheit in der Informationstechnik
    Software engineering by Intevation GmbH

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

#ifndef DIALOGS_UPDATENOTIFICATION_H
#define DIALOGS_UPDATENOTIFICATION_H

#include <QString>
#include <QDialog>

namespace Kleo
{

/** Updatenotification for Gpg4win
 *
 * On Windows it is usual for Applications to notifiy the user
 * about updates. To avoid network access in Kleopatra and
 * to have only one TLS stack in our package this is implemented
 * in dirmngr.
 */
class UpdateNotification : public QDialog
{
public:
    static void checkUpdate(QWidget *parent, bool force = false);
    UpdateNotification(QWidget *parent, const QString &version);};
} // namespace Kleo
#endif // DIALOGS_UPDATENOTIFICATION_H
