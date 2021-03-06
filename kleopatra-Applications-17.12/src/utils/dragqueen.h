/* -*- mode: c++; c-basic-offset:4 -*-
    utils/dragqueen.h

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

#ifndef __KLEOPATRA_UTILS_DRAGQUEEN_H__
#define __KLEOPATRA_UTILS_DRAGQUEEN_H__

#include <QLabel>
#include <QPointer>
#include <QPoint>
#include <QMimeData>

namespace Kleo
{

class DragQueen : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(QString url READ url WRITE setUrl)
public:
    explicit DragQueen(QWidget *widget = nullptr, Qt::WindowFlags f = 0);
    explicit DragQueen(const QString &text, QWidget *widget = nullptr, Qt::WindowFlags f = 0);
    ~DragQueen();

    void setUrl(const QString &url);
    QString url() const;

    void setMimeData(QMimeData *md);
    QMimeData *mimeData() const;

protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;

private:
    QPointer<QMimeData> m_data;
    QPoint m_dragStartPosition;
    QString m_dataFormat;
};

}

#endif // __KLEOPATRA_CRYPTO_GUI_DRAGQUEEN_H__

