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

#ifndef APP_H
#define APP_H

#include <QScopedPointer>
#include <QString>

class QCoreApplication;

/** Application class. */
class App
{
public:
    explicit App(QCoreApplication *application, const QString &sessionName = QString());

    virtual ~App();

    /**
     * Execute application. Returns immediately if exit() was called before.
     * @return Exit code.
     */
    int exec();

    /**
     * Exit application with given exit code.
     */
    virtual void exit(int exitCode=0);

private:
    QScopedPointer<QCoreApplication> m_app;
    int m_exitCode;
    bool m_closed;
};

#endif // APP_H
