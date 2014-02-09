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

#include "itemdelegate.h"

#include "common/client_server.h"
#include "common/contenttype.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "item/itemeditorwidget.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QEvent>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>

namespace {

const char propertySelectedItem[] = "CopyQ_selected";

inline void reset(ItemWidget **ptr, ItemWidget *value = NULL)
{
    delete *ptr;
    *ptr = value != NULL ? value : NULL;
}

int itemMargin()
{
    const int dpi = QApplication::desktop()->physicalDpiX();
    return ( dpi <= 120 ) ? 4 : 4 * dpi / 120;
}

bool isItemSelected(QObject *object)
{
    for ( QObject *parent = object; parent != NULL; parent = parent->parent() ) {
        if ( parent->objectName() == "item" )
            return parent->property(propertySelectedItem).toBool();
    }

    Q_ASSERT(false);
    return false;
}

} // namespace

ItemDelegate::ItemDelegate(QListView *parent)
    : QItemDelegate(parent)
    , m_parent(parent)
    , m_saveOnReturnKey(true)
    , m_re()
    , m_maxSize(2048, 2048 * 8)
    , m_vMargin( itemMargin() )
    , m_hMargin( m_vMargin * 2 + 6 )
    , m_foundFont()
    , m_foundPalette()
    , m_numberFont()
    , m_numberSize( QSize(0, 0) )
    , m_numberPalette()
    , m_antialiasing(true)
    , m_cache()
{
}

ItemDelegate::~ItemDelegate()
{
}

QSize ItemDelegate::sizeHint(const QModelIndex &index) const
{
    int row = index.row();
    if ( row < m_cache.size() ) {
        const ItemWidget *w = m_cache[row];
        if (w != NULL) {
            QWidget *ww = w->widget();
            return QSize( ww->width() + 2 * m_hMargin + m_numberSize.width(),
                          qMax(ww->height() + 2 * m_vMargin, m_numberSize.height()) );
        }
    }
    return QSize(0, 512);
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem &,
                             const QModelIndex &index) const
{
    return sizeHint(index);
}

bool ItemDelegate::eventFilter(QObject *object, QEvent *event)
{
    // resize event for items
    switch ( event->type() ) {
    case QEvent::Resize:
        emit rowSizeChanged();
        break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove: {
        // Don't pass mouse events to item if keyboard modifier is pressed or the item is
        // not yet selected (will be selected by the event).
        if ( static_cast<QMouseEvent*>(event)->modifiers() != Qt::NoModifier || !isItemSelected(object) ) {
            QApplication::sendEvent(m_parent, event);
            return true;
        }
        break;
    }
    default:
        break;
    }

    return false;
}

void ItemDelegate::dataChanged(const QModelIndex &a, const QModelIndex &b)
{
    // recalculating sizes of many items is expensive (when searching)
    // - assume that highlighted (matched) text has same size
    // - recalculate size only if item edited
    int row = a.row();
    if ( row == b.row() ) {
        reset(&m_cache[row]);
        emit rowSizeChanged();
    }
}

void ItemDelegate::rowsRemoved(const QModelIndex &, int start, int end)
{
    for( int i = end; i >= start; --i ) {
        delete m_cache.takeAt(i);
    }
}

void ItemDelegate::rowsMoved(const QModelIndex &, int sourceStart, int sourceEnd,
                             const QModelIndex &, int destinationRow)
{
    int dest = sourceStart < destinationRow ? destinationRow-1 : destinationRow;
    for( int i = sourceStart; i <= sourceEnd; ++i ) {
        m_cache.move(i,dest);
        ++dest;
    }
}

void ItemDelegate::rowsInserted(const QModelIndex &, int start, int end)
{
    for( int i = start; i <= end; ++i )
        m_cache.insert(i, NULL);
}

ItemWidget *ItemDelegate::cache(const QModelIndex &index)
{
    int n = index.row();

    ItemWidget *w = m_cache[n];
    if (w == NULL) {
        w = ConfigurationManager::instance()->itemFactory()->createItem(index, m_parent->viewport());
        setIndexWidget(index, w);
    }

    return w;
}

bool ItemDelegate::hasCache(const QModelIndex &index) const
{
    return m_cache[index.row()] != NULL;
}

void ItemDelegate::setItemMaximumSize(const QSize &size)
{
    int width = size.width() - 2 * m_hMargin - m_numberSize.width();

    if (m_maxSize.width() == width)
        return;

    m_maxSize.setWidth(width);

    for( int i = 0; i < m_cache.length(); ++i ) {
        ItemWidget *w = m_cache[i];
        if (w != NULL)
            w->updateSize(m_maxSize);
    }
}

void ItemDelegate::updateRowPosition(int row, int y)
{
    ItemWidget *w = m_cache[row];
    if (w != NULL)
        w->widget()->move( QPoint(m_numberSize.width() + m_hMargin, y + m_vMargin) );
}

void ItemDelegate::setRowVisible(int row, bool visible)
{
    ItemWidget *w = m_cache[row];
    if (w != NULL)
        w->widget()->setVisible(visible);
}

void ItemDelegate::nextItemLoader(const QModelIndex &index)
{
    ItemWidget *w = m_cache[index.row()];
    if (w != NULL) {
        ItemWidget *w2 = ConfigurationManager::instance()->itemFactory()->nextItemLoader(index, w);
        if (w2 != NULL)
            setIndexWidget(index, w2);
    }
}

void ItemDelegate::previousItemLoader(const QModelIndex &index)
{
    ItemWidget *w = m_cache[index.row()];
    if (w != NULL) {
        ItemWidget *w2 = ConfigurationManager::instance()->itemFactory()->previousItemLoader(index, w);
        if (w2 != NULL)
            setIndexWidget(index, w2);
    }
}

ItemEditorWidget *ItemDelegate::createCustomEditor(QWidget *parent, const QModelIndex &index,
                                                   bool editNotes)
{
    cache(index);
    ItemEditorWidget *editor = new ItemEditorWidget(m_cache[index.row()], index, editNotes, parent);
    loadEditorSettings(editor);
    return editor;
}

void ItemDelegate::loadEditorSettings(ItemEditorWidget *editor)
{
    editor->setEditorPalette(m_editorPalette);
    editor->setEditorFont(m_editorFont);
    editor->setSaveOnReturnKey(m_saveOnReturnKey);
}

void ItemDelegate::setIndexWidget(const QModelIndex &index, ItemWidget *w)
{
    reset(&m_cache[index.row()], w);
    if (w == NULL)
        return;

    QWidget *ww = w->widget();

    if (!m_antialiasing) {
        QFont f = ww->font();
        f.setStyleStrategy(QFont::NoAntialias);
        ww->setFont(f);
        foreach (QWidget *child, ww->findChildren<QWidget *>("item_child"))
            child->setFont(f);
    }

    // Try to get proper size by showing item momentarily.
    ww->show();
    w->updateSize(m_maxSize);
    ww->hide();

    ww->installEventFilter(this);
    foreach (QWidget *child, ww->findChildren<QWidget *>())
        child->installEventFilter(this);

    emit rowSizeChanged();
}

void ItemDelegate::invalidateCache()
{
    for( int i = 0; i < m_cache.length(); ++i )
        reset(&m_cache[i]);
}

void ItemDelegate::setSearch(const QRegExp &re)
{
    m_re = re;
}

void ItemDelegate::setSearchStyle(const QFont &font, const QPalette &palette)
{
    m_foundFont = font;
    m_foundPalette = palette;
}

void ItemDelegate::setEditorStyle(const QFont &font, const QPalette &palette)
{
    m_editorFont = font;
    m_editorPalette = palette;
}

void ItemDelegate::setNumberStyle(const QFont &font, const QPalette &palette)
{
    m_numberFont = font;
    if ( !m_numberSize.isEmpty() ) {
        m_numberSize = QFontMetrics(m_numberFont).boundingRect( QString("0123") ).size()
                + QSize(m_hMargin / 2, 2 * m_vMargin);
    }
    m_numberPalette = palette;
}

void ItemDelegate::setShowNumber(bool show)
{
    if ( show == !m_numberSize.isEmpty() )
        return;

    if (show) {
        m_numberSize = QSize(1, 1);
        setNumberStyle(m_numberFont, m_numberPalette);
    } else {
        m_numberSize = QSize(0, 0);
    }
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    int row = index.row();
    ItemWidget *w = m_cache[row];
    if (w == NULL)
        return;

    const QRect &rect = option.rect;

    bool isSelected = option.state & QStyle::State_Selected;

    /* Alternate colors from last item so deleting or inserting a row won't make following
     * items switch background style. Otherwise the effect can be distracting.
     * Property "alternatingRowColors" must be disabled in QAbstractItemView.
     */
    QStyleOptionViewItemV4 option2(option);
    if ( (m_cache.size() & 1) == (row & 1) )
        option2.features &= ~QStyleOptionViewItemV2::Alternate;
    else
        option2.features |= QStyleOptionViewItemV2::Alternate;

    /* render background (selected, alternate, ...) */
    QStyle *style = m_parent->style();
    style->drawControl(QStyle::CE_ItemViewItem, &option2, painter, m_parent);
    QPalette::ColorRole role = isSelected ? QPalette::HighlightedText : QPalette::Text;

    /* render number */
    QRect numRect(0, 0, 0, 0);
    if ( !m_numberSize.isEmpty() ) {
        const QString num = QString::number(row);
        painter->save();
        painter->setFont(m_numberFont);
        style->drawItemText(painter, rect.translated(m_hMargin / 2, m_vMargin), 0,
                            m_numberPalette, true, num,
                            role);
        painter->restore();
        numRect = style->itemTextRect( QFontMetrics(m_numberFont), rect, 0,
                                       true, num );
    }

    /* highlight search string */
    w->setHighlight(m_re, m_foundFont, m_foundPalette);

    /* text color for selected/unselected item */
    QWidget *ww = w->widget();
    if ( ww->property(propertySelectedItem) != isSelected ) {
        ww->setProperty(propertySelectedItem, isSelected);
        if ( !ww->property("CopyQ_no_style").toBool() ) {
            ww->setStyle(style);
            foreach (QWidget *child, ww->findChildren<QWidget *>())
                child->setStyle(style);
            ww->update();
        }
    }
}
