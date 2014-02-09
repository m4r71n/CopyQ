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

#include "itemtext.h"
#include "ui_itemtextsettings.h"

#include "common/contenttype.h"

#include <QContextMenuEvent>
#include <QModelIndex>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QtPlugin>

namespace {

// Limit number of characters for performance reasons.
const int defaultMaxBytes = 100*1024;

const char optionUseRichText[] = "use_rich_text";
const char optionMaximumLines[] = "max_lines";
const char optionMaximumHeight[] = "max_height";

bool getRichText(const QModelIndex &index, QString *text)
{
    if ( index.data(contentType::hasHtml).toBool() ) {
        *text = index.data(contentType::html).toString();
        return true;
    }

    const QVariantMap dataMap = index.data(contentType::data).toMap();
    if ( !dataMap.contains("text/richtext") )
        return false;

    const QByteArray data = dataMap["text/richtext"].toByteArray();
    *text = QString::fromUtf8(data);

    // Remove trailing null character.
    if ( text->endsWith(QChar(0)) )
        text->resize(text->size() - 1);

    return true;
}

bool getText(const QModelIndex &index, QString *text)
{
    if ( index.data(contentType::hasText).toBool() ) {
        *text = index.data(contentType::text).toString();
        return true;
    }
    return false;
}

} // namespace

ItemText::ItemText(const QString &text, bool isRichText, int maxLines, int maximumHeight, QWidget *parent)
    : QTextEdit(parent)
    , ItemWidget(this)
    , m_textDocument()
    , m_copyOnMouseUp(false)
    , m_maximumHeight(maximumHeight)
{
    m_textDocument.setDefaultFont(font());

    setReadOnly(true);
    setUndoRedoEnabled(false);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameStyle(QFrame::NoFrame);

    setContextMenuPolicy(Qt::NoContextMenu);

    // Selecting text copies it to clipboard.
    connect( this, SIGNAL(selectionChanged()), SLOT(onSelectionChanged()) );

    if (isRichText)
        m_textDocument.setHtml( text.left(defaultMaxBytes) );
    else
        m_textDocument.setPlainText( text.left(defaultMaxBytes) );

    m_textDocument.setDocumentMargin(0);

    setProperty("CopyQ_no_style", isRichText);

    if (maxLines > 0) {
        QTextBlock block = m_textDocument.findBlockByLineNumber(maxLines);
        if (block.isValid()) {
            QTextCursor tc(&m_textDocument);
            tc.setPosition(block.position() - 1);
            tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            tc.removeSelectedText();
            tc.insertHtml( " &nbsp;"
                           "<span style='background:rgba(0,0,0,30);border-radius:4px'>"
                           "&nbsp;&hellip;&nbsp;"
                           "</span>");
        }
    }

    setDocument(&m_textDocument);
}

void ItemText::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    QList<QTextEdit::ExtraSelection> selections;

    if ( !re.isEmpty() ) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground( highlightPalette.base() );
        selection.format.setForeground( highlightPalette.text() );
        selection.format.setFont(highlightFont);

        QTextCursor cur = m_textDocument.find(re);
        int a = cur.position();
        while ( !cur.isNull() ) {
            if ( cur.hasSelection() ) {
                selection.cursor = cur;
                selections.append(selection);
            } else {
                cur.movePosition(QTextCursor::NextCharacter);
            }
            cur = m_textDocument.find(re, cur);
            int b = cur.position();
            if (a == b) {
                cur.movePosition(QTextCursor::NextCharacter);
                cur = m_textDocument.find(re, cur);
                b = cur.position();
                if (a == b) break;
            }
            a = b;
        }
    }

    setExtraSelections(selections);

    update();
}

void ItemText::updateSize(const QSize &maximumSize)
{
    const int w = maximumSize.width();
    const int scrollBarWidth = verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0;
    setMaximumHeight( maximumSize.height() );
    setFixedWidth(w);
    m_textDocument.setTextWidth(w - scrollBarWidth);

    const int h = m_textDocument.size().height();
    setFixedHeight(0 < m_maximumHeight && m_maximumHeight < h ? m_maximumHeight : h);
}

void ItemText::mousePressEvent(QMouseEvent *e)
{
    setTextCursor( cursorForPosition(e->pos()) );
    QTextEdit::mousePressEvent(e);
}

void ItemText::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_copyOnMouseUp) {
        m_copyOnMouseUp = false;
        if ( textCursor().hasSelection() )
            copy();
    } else {
        QTextEdit::mouseReleaseEvent(e);
    }
}

void ItemText::onSelectionChanged()
{
    m_copyOnMouseUp = true;
}

ItemTextLoader::ItemTextLoader()
    : ui(NULL)
{
}

ItemTextLoader::~ItemTextLoader()
{
    delete ui;
}

ItemWidget *ItemTextLoader::create(const QModelIndex &index, QWidget *parent) const
{
    QString text;
    bool isRichText = m_settings.value(optionUseRichText, true).toBool()
            && getRichText(index, &text);

    if ( !isRichText && !getText(index, &text) )
        return NULL;

    const int maxLines = m_settings.value(optionMaximumLines, 0).toInt();
    const int maxHeight = m_settings.value(optionMaximumHeight, 0).toInt();
    return new ItemText(text, isRichText, maxLines, maxHeight, parent);
}

QStringList ItemTextLoader::formatsToSave() const
{
    return m_settings.value(optionUseRichText, true).toBool()
            ? QStringList("text/plain") << QString("text/html") << QString("text/richtext")
            : QStringList("text/plain");
}

QVariantMap ItemTextLoader::applySettings()
{
    Q_ASSERT(ui != NULL);
    m_settings[optionUseRichText] = ui->checkBoxUseRichText->isChecked();
    m_settings[optionMaximumLines] = ui->spinBoxMaxLines->value();
    m_settings[optionMaximumHeight] = ui->spinBoxMaxHeight->value();
    return m_settings;
}

QWidget *ItemTextLoader::createSettingsWidget(QWidget *parent)
{
    delete ui;
    ui = new Ui::ItemTextSettings;
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);
    ui->checkBoxUseRichText->setChecked( m_settings.value(optionUseRichText, true).toBool() );
    ui->spinBoxMaxLines->setValue( m_settings.value(optionMaximumLines, 0).toInt() );
    ui->spinBoxMaxHeight->setValue( m_settings.value(optionMaximumHeight, 0).toInt() );
    return w;
}

Q_EXPORT_PLUGIN2(itemtext, ItemTextLoader)
