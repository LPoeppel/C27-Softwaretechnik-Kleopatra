/* -*- mode: c++; c-basic-offset:4 -*-
    mainwindow.h

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB
    2016 by Bundesamt für Sicherheit in der Informationstechnik
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

#ifndef __KLEOPATRA_MAINWINDOW_DESKTOP_H__
#define __KLEOPATRA_MAINWINDOW_DESKTOP_H__

#include <KXmlGuiWindow>

#include <utils/pimpl_ptr.h>

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr, Qt::WindowFlags f = KDE_DEFAULT_WINDOWFLAGS);
    ~MainWindow();

public Q_SLOTS:
    void importCertificatesFromFile(const QStringList &files);

protected:
    QByteArray savedGeometry;

    void closeEvent(QCloseEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;
    void readProperties(const KConfigGroup &cg) override;
    void saveProperties(KConfigGroup &cg) override;

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
    Q_PRIVATE_SLOT(d, void closeAndQuit())
    Q_PRIVATE_SLOT(d, void selfTest())
    Q_PRIVATE_SLOT(d, void configureBackend())
    Q_PRIVATE_SLOT(d, void configureToolbars())
    Q_PRIVATE_SLOT(d, void editKeybindings())
    Q_PRIVATE_SLOT(d, void gnupgLogViewer())
    Q_PRIVATE_SLOT(d, void slotConfigCommitted())
    Q_PRIVATE_SLOT(d, void slotContextMenuRequested(QAbstractItemView *, QPoint))
    Q_PRIVATE_SLOT(d, void slotFocusQuickSearch())
    Q_PRIVATE_SLOT(d, void toggleSmartcardView())
    Q_PRIVATE_SLOT(d, void forceUpdateCheck())
    Q_PRIVATE_SLOT(d, void openCompendium())
};

#endif /* __KLEOPATRA_MAINWINDOW_DESKTOP_H__ */
