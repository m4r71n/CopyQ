/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TESTS_H
#define TESTS_H

#include <QObject>
#include <QStringList>

class RemoteProcess;
class QProcess;
class QByteArray;

/**
 * Tests for the application.
 */
class Tests : public QObject
{
    Q_OBJECT

public:
    explicit Tests(QObject *parent = NULL);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void moveAndDeleteItems();

    void clipboardToItem();
    void itemToClipboard();
    void tabAddRemove();
    void action();
    void insertRemoveItems();
    void renameTab();
    void importExportTab();
    void separator();
    void eval();
    void rawData();

    void nextPrevious();

private:
    bool startServer();
    bool stopServer();
    bool isServerRunning();

    /** Set clipboard through monitor process. */
    void setClipboard(const QByteArray &bytes, const QString &mime = QString("text/plain"));

    QProcess *m_server;
    RemoteProcess *m_monitor; /// Process to provide clipboard set by tests.
};

#endif // TESTS_H
