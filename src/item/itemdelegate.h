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

#ifndef ITEMDELEGATE_H
#define ITEMDELEGATE_H

#include <QItemDelegate>
#include <QRegExp>

class Item;
class ItemEditorWidget;
class ItemWidget;
class QListView;

/**
 * Delegate for items in ClipboardBrowser.
 *
 * Creates editor on demand and draws contents of all items.
 *
 * To achieve better performance the first call to get sizeHint() value for
 * an item returns some default value (so it doesn't have to render all items).
 *
 * Before calling paint() for an index item on given index must be cached
 * using cache().
 */
class ItemDelegate : public QItemDelegate
{
    Q_OBJECT

    public:
        ItemDelegate(QListView *parent = NULL);

        ~ItemDelegate();

        QSize sizeHint(const QModelIndex &index) const;

        QSize sizeHint(const QStyleOptionViewItem &option,
                       const QModelIndex &index) const;

        bool eventFilter(QObject *object, QEvent *event);

        /** Remove all cached items (cache is refreshed using paint()). */
        void invalidateCache();

        /** Set regular expression for highlighting. */
        void setSearch(const QRegExp &re);

        /** Return regular expression for highlighting. */
        const QRegExp &searchExpression() const { return m_re; }

        /** Search highlight style. */
        void setSearchStyle(const QFont &font, const QPalette &palette);

        /** Editor widget style. */
        void setEditorStyle(const QFont &font, const QPalette &palette);

        /** Item number style. */
        void setNumberStyle(const QFont &font, const QPalette &palette);

        /** Show/hide item number. */
        void setShowNumber(bool show);

        /** Return cached item, create it if it doesn't exist. */
        ItemWidget *cache(const QModelIndex &index);

        /** Return true only if item at index is already in cache. */
        bool hasCache(const QModelIndex &index) const;

        /** Set maximum size for all items. */
        void setItemMaximumSize(const QSize &size);

        /** Save edited item on return or ctrl+return. */
        void setSaveOnEnterKey(bool enable) { m_saveOnReturnKey = enable; }

        /** Enable/disable font antialiasing. */
        void setFontAntialiasing(bool enable) { m_antialiasing = enable; }

        /** Update row position. */
        void updateRowPosition(int row, int y);

        /** Show/hide row. */
        void setRowVisible(int row, bool visible);

        /** Use next item loader available for @a index. */
        void nextItemLoader(const QModelIndex &index);

        /** Use previous item loader available for @a index. */
        void previousItemLoader(const QModelIndex &index);

        /** Create internal item editor widget. */
        ItemEditorWidget *createCustomEditor(QWidget *parent, const QModelIndex &index,
                                             bool editNotes);

        /** Load settings for @a editor. */
        void loadEditorSettings(ItemEditorWidget *editor);

    public slots:
        void dataChanged(const QModelIndex &a, const QModelIndex &b);
        void rowsRemoved(const QModelIndex &parent, int start, int end);
        void rowsInserted(const QModelIndex &parent, int start, int end);
        void rowsMoved(const QModelIndex &parent, int sourceStart, int sourceEnd,
                       const QModelIndex &destination, int destinationRow);

    signals:
        /** Emitted if size of a widget has changed. */
        void rowSizeChanged();

    protected:
        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

    private:
        void setIndexWidget(const QModelIndex &index, ItemWidget *w);

        void updateItemMaximumSize();

        QListView *m_parent;
        bool m_saveOnReturnKey;
        QRegExp m_re;
        QSize m_maxSize;
        int m_vMargin;
        int m_hMargin;

        QFont m_foundFont;
        QPalette m_foundPalette;
        QFont m_editorFont;
        QPalette m_editorPalette;
        QFont m_numberFont;
        QSize m_numberSize;
        QPalette m_numberPalette;
        bool m_antialiasing;

        QList<ItemWidget*> m_cache;
};

#endif
