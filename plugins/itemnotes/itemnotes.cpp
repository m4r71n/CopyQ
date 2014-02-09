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

#include "itemnotes.h"
#include "ui_itemnotessettings.h"

#include "common/contenttype.h"
#include "gui/iconwidget.h"

#include <QBoxLayout>
#include <QLabel>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPainter>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolTip>
#include <QtPlugin>
#include <QDebug>

namespace {

// Limit number of characters for performance reasons.
const int defaultMaxBytes = 10*1024;

const char mimeEncryptedData[] = "application/x-copyq-item-notes";

const int notesIndent = 16;

} // namespace

ItemNotes::ItemNotes(ItemWidget *childItem, const QString &text,
                     bool notesAtBottom, bool showIconOnly, bool showToolTip)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidget(this)
    , m_notes( showIconOnly ? NULL : new QTextEdit(this) )
    , m_icon( showIconOnly ? new IconWidget(IconEditSign, this) : NULL )
    , m_childItem(childItem)
    , m_notesAtBottom(notesAtBottom)
    , m_timerShowToolTip(NULL)
    , m_toolTipText()
    , m_copyOnMouseUp(false)
{
    m_childItem->widget()->setObjectName("item_child");
    m_childItem->widget()->setParent(this);

    QBoxLayout *layout;

    if (showIconOnly) {
        layout = new QHBoxLayout(this);
        layout->addWidget(m_icon, 0, Qt::AlignRight | Qt::AlignTop);
        layout->addWidget(m_childItem->widget());
    } else {
        m_notes->setObjectName("item_child");

        m_notes->document()->setDefaultFont(font());

        m_notes->setReadOnly(true);
        m_notes->setUndoRedoEnabled(false);

        m_notes->setFocusPolicy(Qt::NoFocus);
        m_notes->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_notes->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_notes->setFrameStyle(QFrame::NoFrame);
        m_notes->setContextMenuPolicy(Qt::NoContextMenu);

        // Selecting text copies it to clipboard.
        connect( m_notes, SIGNAL(selectionChanged()), SLOT(onSelectionChanged()) );

        m_notes->setPlainText( text.left(defaultMaxBytes) );

        layout = new QVBoxLayout(this);

        QHBoxLayout *labelLayout = new QHBoxLayout;
        labelLayout->setMargin(0);
        labelLayout->setContentsMargins(notesIndent, 0, 0, 0);
        labelLayout->addWidget(m_notes, 0, Qt::AlignLeft);

        if (notesAtBottom) {
            layout->addWidget( m_childItem->widget() );
            layout->addLayout(labelLayout);
        } else {
            layout->addLayout(labelLayout);
            layout->addWidget( m_childItem->widget() );
        }
    }

    if (showToolTip) {
        m_timerShowToolTip = new QTimer(this);
        m_timerShowToolTip->setInterval(250);
        m_timerShowToolTip->setSingleShot(true);
        connect( m_timerShowToolTip, SIGNAL(timeout()),
                 this, SLOT(showToolTip()) );
        m_toolTipText = text;
    }

    layout->setMargin(0);
    layout->setSpacing(0);
}

void ItemNotes::setCurrent(bool current)
{
    if (m_timerShowToolTip == NULL)
        return;

    QToolTip::hideText();

    if (current)
        m_timerShowToolTip->start();
    else
        m_timerShowToolTip->stop();
}

void ItemNotes::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    m_childItem->setHighlight(re, highlightFont, highlightPalette);

    if (m_notes != NULL) {
        QList<QTextEdit::ExtraSelection> selections;

        if ( !re.isEmpty() ) {
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground( highlightPalette.base() );
            selection.format.setForeground( highlightPalette.text() );
            selection.format.setFont(highlightFont);

            QTextCursor cur = m_notes->document()->find(re);
            int a = cur.position();
            while ( !cur.isNull() ) {
                if ( cur.hasSelection() ) {
                    selection.cursor = cur;
                    selections.append(selection);
                } else {
                    cur.movePosition(QTextCursor::NextCharacter);
                }
                cur = m_notes->document()->find(re, cur);
                int b = cur.position();
                if (a == b) {
                    cur.movePosition(QTextCursor::NextCharacter);
                    cur = m_notes->document()->find(re, cur);
                    b = cur.position();
                    if (a == b) break;
                }
                a = b;
            }
        }

        m_notes->setExtraSelections(selections);
    }

    update();
}

QWidget *ItemNotes::createEditor(QWidget *parent) const
{
    return m_childItem.isNull() ? NULL : m_childItem->createEditor(parent);
}

void ItemNotes::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    Q_ASSERT( !m_childItem.isNull() );
    return m_childItem->setEditorData(editor, index);
}

void ItemNotes::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    Q_ASSERT( !m_childItem.isNull() );
    return m_childItem->setModelData(editor, model, index);
}

bool ItemNotes::hasChanges(QWidget *editor) const
{
    Q_ASSERT( !m_childItem.isNull() );
    return m_childItem->hasChanges(editor);
}

QObject *ItemNotes::createExternalEditor(const QModelIndex &index, QWidget *parent) const
{
    return m_childItem ? m_childItem->createExternalEditor(index, parent)
                       : ItemWidget::createExternalEditor(index, parent);
}

void ItemNotes::updateSize(const QSize &maximumSize)
{
    setMaximumSize(maximumSize);

    if (m_notes) {
        const int w = maximumSize.width() - 2 * notesIndent - 8;
        QTextDocument *doc = m_notes->document();
        doc->setTextWidth(w);
        m_notes->setFixedSize( doc->idealWidth() + 16, doc->size().height() );
    }

    if ( !m_childItem.isNull() )
        m_childItem->updateSize(maximumSize);

    adjustSize();
    setFixedSize(sizeHint());
}

void ItemNotes::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        if (m_notes) {
            const QPoint pos = m_notes->viewport()->mapFrom(this, e->pos());
            m_notes->setTextCursor( m_notes->cursorForPosition(pos) );
            QWidget::mousePressEvent(e);
        }
        e->ignore();
    } else {
        QWidget::mousePressEvent(e);
    }
}

void ItemNotes::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_notes && m_copyOnMouseUp) {
        m_copyOnMouseUp = false;
        if ( m_notes->textCursor().hasSelection() )
            m_notes->copy();
    } else {
        QWidget::mouseReleaseEvent(e);
    }
}

void ItemNotes::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    // Decorate notes.
    if (m_notes != NULL) {
        QPainter p(this);

        QColor c = p.pen().color();
        c.setAlpha(80);
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawRect(m_notes->x() - notesIndent + 4, m_notes->y() + 4,
                   notesIndent - 4, m_notes->height() - 8);
    }
}

void ItemNotes::onSelectionChanged()
{
    m_copyOnMouseUp = true;
}

void ItemNotes::showToolTip()
{
    QToolTip::hideText();

    QPoint toolTipPosition = QPoint(parentWidget()->contentsRect().width() - 16, height() - 16);
    toolTipPosition = mapToGlobal(toolTipPosition);

    QToolTip::showText(toolTipPosition, m_toolTipText, this);
}

ItemNotesLoader::ItemNotesLoader()
    : ui(NULL)
{
}

ItemNotesLoader::~ItemNotesLoader()
{
    delete ui;
}

QStringList ItemNotesLoader::formatsToSave() const
{
    return QStringList(mimeEncryptedData);
}

QVariantMap ItemNotesLoader::applySettings()
{
    m_settings["notes_at_bottom"] = ui->radioButtonBottom->isChecked();
    m_settings["icon_only"] = ui->radioButtonIconOnly->isChecked();
    m_settings["show_tooltip"] = ui->checkBoxShowToolTip->isChecked();
    return m_settings;
}

QWidget *ItemNotesLoader::createSettingsWidget(QWidget *parent)
{
    delete ui;
    ui = new Ui::ItemNotesSettings;
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    if ( m_settings["icon_only"].toBool() )
        ui->radioButtonIconOnly->setChecked(true);
    else if ( m_settings["notes_at_bottom"].toBool() )
        ui->radioButtonBottom->setChecked(true);
    else
        ui->radioButtonTop->setChecked(true);

    ui->checkBoxShowToolTip->setChecked( m_settings["show_tooltip"].toBool() );

    return w;
}

ItemWidget *ItemNotesLoader::transform(ItemWidget *itemWidget, const QModelIndex &index)
{
    const QString text = index.data(contentType::notes).toString();
    if ( text.isEmpty() )
        return NULL;

    return new ItemNotes( itemWidget, text,
                          m_settings["notes_at_bottom"].toBool(),
                          m_settings["icon_only"].toBool(),
            m_settings["show_tooltip"].toBool() );
}

bool ItemNotesLoader::matches(const QModelIndex &index, const QRegExp &re) const
{
    const QString text = index.data(contentType::notes).toString();
    return re.indexIn(text) != -1;
}

Q_EXPORT_PLUGIN2(itemnotes, ItemNotesLoader)
