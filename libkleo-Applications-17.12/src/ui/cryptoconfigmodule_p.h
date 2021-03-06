/*
    cryptoconfigmodule_p.h

    This file is part of libkleopatra
    Copyright (c) 2004,2005 Klarälvdalens Datakonsult AB

    Libkleopatra is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License,
    version 2, as published by the Free Software Foundation.

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

#ifndef CRYPTOCONFIGMODULE_P_H
#define CRYPTOCONFIGMODULE_P_H

#include <QWidget>

#include <QList>

class KLineEdit;
class QSpinBox;

class QPushButton;
class QGridLayout;
class QLabel;
class QCheckBox;
class QComboBox;

namespace Kleo
{
class FileNameRequester;
}

namespace QGpgME
{
class CryptoConfig;
class CryptoConfigComponent;
class CryptoConfigGroup;
class CryptoConfigEntry;
} // namespace QGpgME

namespace Kleo
{
class CryptoConfigComponentGUI;
class CryptoConfigGroupGUI;
class CryptoConfigEntryGUI;

/**
 * A widget corresponding to a component in the crypto config
 */
class CryptoConfigComponentGUI : public QWidget
{
    Q_OBJECT

public:
    CryptoConfigComponentGUI(CryptoConfigModule *module, QGpgME::CryptoConfigComponent *component,
                             QWidget *parent = nullptr);

    bool save();
    void load();
    void defaults();

private:
    QGpgME::CryptoConfigComponent *mComponent = nullptr;
    QList<CryptoConfigGroupGUI *> mGroupGUIs;
};

/**
 * A class managing widgets corresponding to a group in the crypto config
 */
class CryptoConfigGroupGUI : public QObject
{
    Q_OBJECT

public:
    CryptoConfigGroupGUI(CryptoConfigModule *module, QGpgME::CryptoConfigGroup *group,
                         QGridLayout *layout, QWidget *parent = nullptr);

    bool save();
    void load();
    void defaults();

private:
    QGpgME::CryptoConfigGroup *mGroup = nullptr;
    QList<CryptoConfigEntryGUI *> mEntryGUIs;
};

/**
 * Factory for CryptoConfigEntryGUI instances
 * Not a real factory, but can become one later.
 */
class CryptoConfigEntryGUIFactory
{
public:
    static CryptoConfigEntryGUI *createEntryGUI(
        CryptoConfigModule *module,
        QGpgME::CryptoConfigEntry *entry, const QString &entryName,
        QGridLayout *layout, QWidget *widget);
};

/**
 * Base class for the widget managers tied to an entry in the crypto config
 */
class CryptoConfigEntryGUI : public QObject
{
    Q_OBJECT
public:
    CryptoConfigEntryGUI(CryptoConfigModule *module,
                         QGpgME::CryptoConfigEntry *entry,
                         const QString &entryName);

    void load()
    {
        doLoad();
        mChanged = false;
    }
    void save()
    {
        Q_ASSERT(mChanged);
        doSave();
        mChanged = false;
    }
    void resetToDefault();

    QString description() const;
    bool isChanged() const
    {
        return mChanged;
    }

Q_SIGNALS:
    void changed();

protected Q_SLOTS:
    void slotChanged()
    {
        mChanged = true;
        emit changed();
    }

protected:
    virtual void doSave() = 0;
    virtual void doLoad() = 0;

    QGpgME::CryptoConfigEntry *mEntry = nullptr;
    QString mName;
    bool mChanged = false;
};

/**
 * A widget manager for a string entry in the crypto config
 */
class CryptoConfigEntryLineEdit : public CryptoConfigEntryGUI
{
    Q_OBJECT

public:
    CryptoConfigEntryLineEdit(CryptoConfigModule *module,
                              QGpgME::CryptoConfigEntry *entry,
                              const QString &entryName,
                              QGridLayout *layout,
                              QWidget *parent = nullptr);

    void doSave() override;
    void doLoad() override;
private:
    KLineEdit *mLineEdit = nullptr;
};

/**
 * A widget manager for a debug-level entry in the crypto config
 */
class CryptoConfigEntryDebugLevel : public CryptoConfigEntryGUI
{
    Q_OBJECT
public:
    CryptoConfigEntryDebugLevel(CryptoConfigModule *module, QGpgME::CryptoConfigEntry *entry,
                                const QString &entryName, QGridLayout *layout, QWidget *parent = nullptr);

    void doSave() override;
    void doLoad() override;
private:
    QComboBox *mComboBox = nullptr;
};

/**
 * A widget manager for a path entry in the crypto config
 */
class CryptoConfigEntryPath : public CryptoConfigEntryGUI
{
    Q_OBJECT

public:
    CryptoConfigEntryPath(CryptoConfigModule *module,
                          QGpgME::CryptoConfigEntry *entry,
                          const QString &entryName,
                          QGridLayout *layout,
                          QWidget *parent = nullptr);

    void doSave() override;
    void doLoad() override;
private:
    Kleo::FileNameRequester *mFileNameRequester = nullptr;
};

/**
 * A widget manager for a directory path entry in the crypto config
 */
class CryptoConfigEntryDirPath : public CryptoConfigEntryGUI
{
    Q_OBJECT

public:
    CryptoConfigEntryDirPath(CryptoConfigModule *module,
                             QGpgME::CryptoConfigEntry *entry,
                             const QString &entryName,
                             QGridLayout *layout,
                             QWidget *parent = nullptr);

    void doSave() override;
    void doLoad() override;
private:
    Kleo::FileNameRequester *mFileNameRequester = nullptr;
};

/**
 * A widget manager for an int/uint entry in the crypto config
 */
class CryptoConfigEntrySpinBox : public CryptoConfigEntryGUI
{
    Q_OBJECT

public:
    CryptoConfigEntrySpinBox(CryptoConfigModule *module,
                             QGpgME::CryptoConfigEntry *entry,
                             const QString &entryName,
                             QGridLayout *layout,
                             QWidget *parent = nullptr);
    void doSave() override;
    void doLoad() override;
private:
    enum { Int, UInt, ListOfNone } mKind;
    QSpinBox *mNumInput = nullptr;
};

/**
 * A widget manager for a bool entry in the crypto config
 */
class CryptoConfigEntryCheckBox : public CryptoConfigEntryGUI
{
    Q_OBJECT

public:
    CryptoConfigEntryCheckBox(CryptoConfigModule *module,
                              QGpgME::CryptoConfigEntry *entry,
                              const QString &entryName,
                              QGridLayout *layout,
                              QWidget *parent = nullptr);
    void doSave() override;
    void doLoad() override;
private:
    QCheckBox *mCheckBox = nullptr;
};

/**
 * A widget manager for an LDAP list entry in the crypto config
 */
class CryptoConfigEntryLDAPURL : public CryptoConfigEntryGUI
{
    Q_OBJECT

public:
    CryptoConfigEntryLDAPURL(CryptoConfigModule *module,
                             QGpgME::CryptoConfigEntry *entry,
                             const QString &entryName,
                             QGridLayout *layout,
                             QWidget *parent = nullptr);
    void doSave() override;
    void doLoad() override;
private Q_SLOTS:
    void slotOpenDialog();
private:
    void setURLList(const QList<QUrl> &urlList);
    QLabel *mLabel = nullptr;
    QPushButton *mPushButton = nullptr;
    QList<QUrl> mURLList;
};

/**
 * A widget manager for a (OpenPGP) keyserver entry in the crypto config
 */
class CryptoConfigEntryKeyserver : public CryptoConfigEntryGUI
{
    Q_OBJECT

public:
    CryptoConfigEntryKeyserver(CryptoConfigModule *module,
                               QGpgME::CryptoConfigEntry *entry,
                               const QString &entryName,
                               QGridLayout *layout,
                               QWidget *parent = nullptr);
    void doSave() override;
    void doLoad() override;
private Q_SLOTS:
    void slotOpenDialog();
private:
    QLabel *mLabel = nullptr;
    QPushButton *mPushButton = nullptr;
    ParsedKeyserver mParsedKeyserver;
};
}

#endif // CRYPTOCONFIGMODULE_P_H
