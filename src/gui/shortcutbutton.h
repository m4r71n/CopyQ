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

#ifndef SHORTCUTBUTTON_H
#define SHORTCUTBUTTON_H

#include <QKeySequence>
#include <QList>
#include <QWidget>

class QHBoxLayout;
class QPushButton;

/**
 * Widget with buttons for defining shortcuts and single button for adding shortcuts.
 */
class ShortcutButton : public QWidget
{
    Q_OBJECT
public:
    explicit ShortcutButton(const QKeySequence &defaultShortcut, QWidget *parent = NULL);

    /** Expect modifier or accept shortcuts without one. */
    void setExpectModifier(bool expectModifier) { m_expectModifier = expectModifier; }

    /** Creates new shortcut button for @a shortcut if it's valid and same button doesn't exist. */
    void addShortcut(const QKeySequence &shortcut);

    /** Remove all shortcut buttons. */
    void clearShortcuts();

    /** Remove all shortcut buttons and add button with shortcut passed in constructor if valid. */
    void resetShortcuts();

    /** Return valid shortcuts defined by buttons. */
    QList<QKeySequence> shortcuts() const;

    void updateIcons();

    /** Add icon and tooltip to buttons that contain shortcut from @a ambiguousShortcuts list. */
    void checkAmbiguousShortcuts(const QList<QKeySequence> &ambiguousShortcuts,
                                 const QIcon &warningIcon, const QString &warningToolTip);

signals:
    /** Emited if new @a shortcut (with button) was added. */
    void shortcutAdded(const QKeySequence &shortcut);
    /** Emited if @a shortcut (with button) was removed. */
    void shortcutRemoved(const QKeySequence &shortcut);

protected:
    bool focusNextPrevChild(bool next);

private slots:
    void onShortcutButtonClicked();
    void onButtonAddShortcutClicked();
    void addShortcut(QPushButton *shortcutButton);

    int shortcutButtonCount() const;
    QWidget *shortcutButton(int index) const;

private:
    QKeySequence shortcutForButton(const QWidget &w) const;

    QKeySequence m_defaultShortcut;
    QHBoxLayout *m_layout;
    QPushButton *m_buttonAddShortcut;
    bool m_expectModifier;
};

#endif // SHORTCUTBUTTON_H
