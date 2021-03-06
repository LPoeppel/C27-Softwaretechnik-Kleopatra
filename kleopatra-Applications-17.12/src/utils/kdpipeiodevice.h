/*
  Copyright (C) 2007 Klarälvdalens Datakonsult AB

  KDPipeIODevice is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  KDPipeIODevice is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with KDPipeIODevice; see the file COPYING.LIB.  If not, write to the
  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#ifndef __KDTOOLSCORE_KDPIPEIODEVICE_H__
#define __KDTOOLSCORE_KDPIPEIODEVICE_H__

#include <QIODevice>

#include <utility>

class KDPipeIODevice : public QIODevice
{
    Q_OBJECT
    //KDAB_MAKE_CHECKABLE( KDPipeIODevice )
public:
    enum DebugLevel {
        NoDebug,
        Debug
    };

    static DebugLevel debugLevel();
    static void setDebugLevel(DebugLevel level);

    explicit KDPipeIODevice(QObject *parent = nullptr);
    explicit KDPipeIODevice(int fd, OpenMode = ReadOnly, QObject *parent = nullptr);
    explicit KDPipeIODevice(Qt::HANDLE handle, OpenMode = ReadOnly, QObject *parent = nullptr);
    ~KDPipeIODevice();

    static std::pair<KDPipeIODevice *, KDPipeIODevice *> makePairOfConnectedPipes();

    bool open(int fd, OpenMode mode = ReadOnly);
    bool open(Qt::HANDLE handle, OpenMode mode = ReadOnly);

    Qt::HANDLE handle() const;
    int descriptor() const;

    bool readWouldBlock() const;
    bool writeWouldBlock() const;

    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;
    bool canReadLine() const override;
    void close() override;
    bool isSequential() const override;
    bool atEnd() const override;

    bool waitForBytesWritten(int msecs) override;
    bool waitForReadyRead(int msecs) override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    using QIODevice::open;

private:
    class Private;
    Private *d;
};

#endif /* __KDTOOLSCORE_KDPIPEIODEVICE_H__ */

