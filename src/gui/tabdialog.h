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

#ifndef TAGDIALOG_H
#define TAGDIALOG_H

#include <QDialog>
#include <QStringList>

namespace Ui {
    class TabDialog;
}

/**
 * Dialog for naming and renaming tabs.
 */
class TabDialog : public QDialog
{
    Q_OBJECT

public:
    /** Tab dialog type (new tab or rename existing tab). */
    typedef enum {
        TabNew,
        TabRename,
        TabGroupRename
    } TabDialogType;

    explicit TabDialog(TabDialogType type, QWidget *parent = NULL);
    ~TabDialog();

    /** Set tab index to rename (emitted parameter of accepted()). */
    void setTabIndex(int tabIndex);

    /** Set existing tabs for validation. */
    void setTabs(const QStringList &tabs);

    /** Set current tab name. */
    void setTabName(const QString &tabName);

    /** Set current tab group name. */
    void setTabGroupName(const QString &tabGroupName);

signals:
    /** Signal emitted if tab @a name is accepted. */
    void accepted(const QString &name, int tabIndex);

    /** Signal emitted if tab group @a name is accepted. */
    void accepted(const QString &newName, const QString &oldName);

private slots:
    void onAccepted();

    /**
     * Validate tab name.
     * Tab name should be non-empty and should not be in existing tab list
     * (see setTabs()).
     */
    void validate();

private:
    Ui::TabDialog *ui;
    int m_tabIndex;
    QString m_tabGroupName;
    QStringList m_tabs;
};

#endif // TAGDIALOG_H
