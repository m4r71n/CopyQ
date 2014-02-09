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

#ifndef ICONWIDGET_H
#define ICONWIDGET_H

#include <QWidget>

class QString;

class IconWidget : public QWidget
{
    Q_OBJECT
public:
    explicit IconWidget(int icon, QWidget *parent = NULL);

    explicit IconWidget(const QString &icon, QWidget *parent = NULL);

    virtual QSize sizeHint() const;

protected:
    virtual void paintEvent(QPaintEvent *event);

private:
    QString m_icon;
};

#endif // ICONWIDGET_H
