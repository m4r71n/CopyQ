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

#include "itemeditor.h"

#include "common/common.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QProcess>
#include <QTemporaryFile>
#include <QTimer>
#include <stdio.h>

namespace {

QString getFileSuffixFromMime(const QString &mime)
{
    if (mime == mimeText)
        return QString(".txt");
    if (mime == "text/html")
        return QString(".html");
    if (mime == "text/xml")
        return QString(".xml");
    if (mime == "image/bmp")
        return QString(".bmp");
    if (mime == "image/jpeg")
        return QString(".jpg");
    if (mime == "image/gif")
        return QString(".png");
    if (mime == "image/gif")
        return QString(".gif");
    if (mime == "image/svg+xml" || mime == "image/x-inkscape-svg-compressed")
        return QString(".svg");
    if (mime == MIME_PREFIX "theme")
        return QString(".ini");
    return QString();
}

void printError(const QString &text)
{
    QFile f;
    f.open(stderr, QIODevice::WriteOnly);
    f.write( QObject::tr("CopyQ ERROR: %1\n").arg(text).toLocal8Bit() );
}

} // namespace

ItemEditor::ItemEditor(const QByteArray &data, const QString &mime, const QString &editor,
                       QObject *parent)
    : QObject(parent)
    , m_data(data)
    , m_mime(mime)
    , m_hash( qHash(m_data) )
    , m_editorcmd(editor)
    , m_editor(NULL)
    , m_timer( new QTimer(this) )
    , m_info()
    , m_lastmodified()
    , m_modified(false)
{
    if ( !m_editorcmd.contains("%1") )
        m_editorcmd.append(" %1");
}

ItemEditor::~ItemEditor()
{
    if (m_editor && m_editor->isOpen())
        m_editor->close();

    QString tmpPath = m_info.filePath();
    if ( !tmpPath.isEmpty() ) {
        if ( !QFile::remove(tmpPath) )
            printError( tr("Failed to remove temporary file (%1)").arg(tmpPath) );
    }
}

bool ItemEditor::start()
{
    // create temp file
    const QString tmpFileName = QString("CopyQ.XXXXXX") + getFileSuffixFromMime(m_mime);
    QString tmpPath = QDir( QDir::tempPath() ).absoluteFilePath(tmpFileName);

    QTemporaryFile tmpfile;
    tmpfile.setFileTemplate(tmpPath);
    tmpfile.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    if ( !tmpfile.open() ) {
        printError( tr("Failed to open temporary file (%1) for editing item in external editor!")
             .arg(tmpfile.fileName()) );
        return false;
    }

    // write text to temp file
    tmpfile.write(m_data);
    tmpfile.flush();

    // monitor file
    m_info.setFile( tmpfile.fileName() );
    m_lastmodified = m_info.lastModified();
    m_timer->start(500);
    connect( m_timer, SIGNAL(timeout()),
             this, SLOT(onTimer()) );

    // exec editor
    m_editor = new QProcess(this);
    connect( m_editor, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(close()) );
    const QString nativeFilePath = QDir::toNativeSeparators(m_info.filePath());
    m_editor->start( m_editorcmd.arg('"' + nativeFilePath + '"') );

    tmpfile.setAutoRemove(false);
    tmpfile.close();

    return true;
}

void ItemEditor::close()
{
    // check if file was modified before closing
    if ( m_modified || fileModified() )
        emit fileModified(m_data, m_mime);

    emit closed(this);
}

bool ItemEditor::fileModified()
{
    m_info.refresh();
    if ( m_lastmodified != m_info.lastModified() ) {
        m_lastmodified = m_info.lastModified();

        // read text
        QFile file( m_info.filePath() );
        if ( file.open(QIODevice::ReadOnly) ) {
            m_data = file.readAll();
            file.close();
        } else {
            printError( tr("Failed to read temporary file (%1)!").arg(m_info.fileName()) );
        }

        // new hash
        uint newhash = qHash(m_data);

        return newhash != m_hash;
    }

    return false;
}

void ItemEditor::onTimer()
{
    if (m_modified) {
        // Wait until file is fully overwritten.
        if ( !fileModified() ) {
            m_modified = false;
            emit fileModified(m_data, m_mime);
            m_hash = qHash(m_data);
        }
    } else {
        m_modified = fileModified();
    }
}

