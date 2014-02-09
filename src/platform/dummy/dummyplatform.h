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

#ifndef DUMMYPLATFORM_H
#define DUMMYPLATFORM_H

#include "platform/platformnativeinterface.h"

#include <QApplication>
#include <QCoreApplication>

class PlatformWindow
{
public:
    QString getTitle() { return QString(); }

    void raise() {}

    void pasteClipboard() {}
};

class DummyPlatform : public PlatformNativeInterface
{
public:
    PlatformWindowPtr getWindow(WId) { return PlatformWindowPtr(); }

    PlatformWindowPtr getCurrentWindow() { return PlatformWindowPtr(); }

    bool canAutostart() { return false; }

    bool isAutostartEnabled() { return false; }

    void setAutostartEnabled(bool) {}

    QApplication *createServerApplication(int &argc, char **argv) { return new QApplication(argc, argv); }

    QApplication *createMonitorApplication(int &argc, char **argv) { return new QApplication(argc, argv); }

    QCoreApplication *createClientApplication(int &argc, char **argv) { return new QCoreApplication(argc, argv); }

    void loadSettings() {}
};

#endif // DUMMYPLATFORM_H
