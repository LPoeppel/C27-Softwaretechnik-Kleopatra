/* -*- mode: c++; c-basic-offset:4 -*-
    uiserver/echocommand.h

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

#ifndef __KLEOPATRA_UISERVER_ECHOCOMMAND_H__
#define __KLEOPATRA_UISERVER_ECHOCOMMAND_H__

#include "assuancommand.h"
#include <QObject>

namespace Kleo
{

/*!
  \brief GnuPG UI Server command for testing

  <h3>Usage</h3>

  \code
  ECHO [--inquire <keyword>] [--nohup]
  \endcode

  <h3>Description</h3>

  The ECHO command is a simple tool for testing. If a bulk input
  channel has been set up by the client, ECHO will read data from
  it, and pipe it right back into the bulk output channel. It is
  an error for an input channel to exist without an output
  channel.

  ECHO will also send back any non-option command line arguments
  in a status message. If the --inquire command line option has
  been given, ECHO will inquire with that keyword, and send the
  received data back on the status channel.
*/
class EchoCommand : public QObject, public AssuanCommandMixin<EchoCommand>
{
    Q_OBJECT
public:
    EchoCommand();
    ~EchoCommand();

    static const char *staticName()
    {
        return "ECHO";
    }

private:
    int doStart() override;
    void doCanceled() override;

private Q_SLOTS:
    void slotInquireData(int, const QByteArray &);
    void slotInputReadyRead();
    void slotOutputBytesWritten();

private:
    class Private;
    kdtools::pimpl_ptr<Private> d;
};

}

#endif /* __KLEOPATRA_UISERVER_ECHOCOMMAND_H__ */
