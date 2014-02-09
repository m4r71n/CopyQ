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

#include "itemencrypted.h"
#include "ui_itemencryptedsettings.h"

#include "common/common.h"
#include "common/contenttype.h"
#include "gui/icons.h"
#include "gui/iconwidget.h"
#include "item/encrypt.h"
#include "item/serialize.h"

#include <QFile>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QSettings>
#include <QtPlugin>

namespace {

const char dataFileHeader[] = "CopyQ_encrypted_tab";
const char dataFileHeaderV2[] = "CopyQ_encrypted_tab v2";

void startGpgProcess(QProcess *p, const QStringList &args)
{
    p->start("gpg", getDefaultEncryptCommandArguments() + args);
}

QByteArray readGpgOutput(const QStringList &args, const QByteArray &input = QByteArray())
{
    QProcess p;
    startGpgProcess( &p, args );
    p.write(input);
    p.closeWriteChannel();
    p.waitForFinished();
    return p.readAllStandardOutput();
}

bool keysExist()
{
    return !readGpgOutput( QStringList("--list-keys") ).isEmpty();
}

bool decryptMimeData(QVariantMap *detinationData, const QModelIndex &index)
{
    const QVariantMap data = index.data(contentType::data).toMap();
    if ( !data.contains(mimeEncryptedData) )
        return false;

    const QByteArray encryptedBytes = data.value(mimeEncryptedData).toByteArray();
    const QByteArray bytes = readGpgOutput( QStringList() << "--decrypt", encryptedBytes );

    return deserializeData(detinationData, bytes);
}

void encryptMimeData(const QVariantMap &data, const QModelIndex &index, QAbstractItemModel *model)
{
    const QByteArray bytes = serializeData(data);
    const QByteArray encryptedBytes = readGpgOutput( QStringList("--encrypt"), bytes );
    QVariantMap dataMap;
    dataMap.insert(mimeEncryptedData, encryptedBytes);
    model->setData(index, dataMap, contentType::data);
}

} // namespace

ItemEncrypted::ItemEncrypted(QWidget *parent)
    : QWidget(parent)
    , ItemWidget(this)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(6);

    // Show small icon.
    QWidget *iconWidget = new IconWidget(IconLock, this);
    layout->addWidget(iconWidget);
}

void ItemEncrypted::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    // Decrypt before editing.
    QTextEdit *textEdit = qobject_cast<QTextEdit *>(editor);
    if (textEdit != NULL) {
        QVariantMap data;
        if ( decryptMimeData(&data, index) ) {
            textEdit->setPlainText( getTextData(data, mimeText) );
            textEdit->selectAll();
        }
    }
}

void ItemEncrypted::setModelData(QWidget *editor, QAbstractItemModel *model,
                                 const QModelIndex &index) const
{
    // Encrypt after editing.
    QTextEdit *textEdit = qobject_cast<QTextEdit*>(editor);
    if (textEdit != NULL)
        encryptMimeData( createDataMap(mimeText, textEdit->toPlainText()), index, model );
}

ItemEncryptedLoader::ItemEncryptedLoader()
    : ui(NULL)
    , m_settings()
    , m_gpgProcessStatus(GpgNotRunning)
    , m_gpgProcess(NULL)
{
}

ItemEncryptedLoader::~ItemEncryptedLoader()
{
    terminateGpgProcess();
    delete ui;
}

ItemWidget *ItemEncryptedLoader::create(const QModelIndex &index, QWidget *parent) const
{
    const QVariantMap dataMap = index.data(contentType::data).toMap();
    return dataMap.contains(mimeEncryptedData) ? new ItemEncrypted(parent) : NULL;
}

QStringList ItemEncryptedLoader::formatsToSave() const
{
    return QStringList(mimeEncryptedData);
}

QVariantMap ItemEncryptedLoader::applySettings()
{
    Q_ASSERT(ui != NULL);
    m_settings.insert( "encrypt_tabs", ui->plainTextEditEncryptTabs->toPlainText().split('\n') );
    return m_settings;
}

QWidget *ItemEncryptedLoader::createSettingsWidget(QWidget *parent)
{
    delete ui;
    ui = new Ui::ItemEncryptedSettings;
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    ui->plainTextEditEncryptTabs->setPlainText(
                m_settings.value("encrypt_tabs").toStringList().join("\n") );

    // Check if gpg application is available.
    QProcess p;
    startGpgProcess(&p, QStringList("--version"));
    p.closeWriteChannel();
    p.waitForFinished();
    if ( p.error() != QProcess::UnknownError ) {
        m_gpgProcessStatus = GpgNotInstalled;
    } else {
        KeyPairPaths keys;
        ui->labelShareInfo->setTextFormat(Qt::RichText);
        ui->labelShareInfo->setText( tr("To share encrypted items on other computer or"
                                        " session, you'll need public and secret key files:"
                                        "<ul>"
                                        "<li>%1</li>"
                                        "<li>%2<br />(Keep this secret key in a safe place.)</li>"
                                        "</ul>"
                                        )
                                     .arg( quoteString(keys.pub) )
                                     .arg( quoteString(keys.sec) )
                                     );
    }

    updateUi();

    connect( ui->pushButtonPassword, SIGNAL(clicked()),
             this, SLOT(setPassword()) );

    return w;
}

bool ItemEncryptedLoader::canLoadItems(QFile *file)
{
    QDataStream stream(file);

    QString header;
    stream >> header;

    return stream.status() == QDataStream::Ok
            && (header == dataFileHeader || header == dataFileHeaderV2);
}

bool ItemEncryptedLoader::canSaveItems(const QAbstractItemModel &model)
{
    const QString tabName = model.property("tabName").toString();

    foreach ( const QString &encryptTabName, m_settings.value("encrypt_tabs").toStringList() ) {
        QString tabName1 = tabName;

        // Ignore ampersands (usually just for underlining mnemonics) if none is specified.
        if ( !encryptTabName.contains('&') )
            tabName1.remove('&');

        // Ignore path in tab tree if none path separator is specified.
        if ( !encryptTabName.contains('/') ) {
            const int i = tabName1.lastIndexOf('/');
            tabName1.remove(0, i + 1);
        }

        if ( tabName1 == encryptTabName )
            return true;
    }

    return false;
}

bool ItemEncryptedLoader::loadItems(QAbstractItemModel *model, QFile *file)
{
    if (m_gpgProcessStatus == GpgNotInstalled)
        return false;

    // This is needed to skip header.
    if ( !canLoadItems(file) )
        return false;

    QProcess p;
    startGpgProcess( &p, QStringList("--decrypt") );

    char encryptedBytes[4096];

    QDataStream stream(file);
    while ( !stream.atEnd() ) {
        const int bytesRead = stream.readRawData(encryptedBytes, 4096);
        if (bytesRead == -1)
            return false;
        p.write(encryptedBytes, bytesRead);
    }

    p.closeWriteChannel();
    p.waitForFinished();

    const QByteArray bytes = p.readAllStandardOutput();
    if ( bytes.isEmpty() )
        return false;

    QDataStream stream2(bytes);

    quint64 maxItems = model->property("maxItems").toInt();
    quint64 length;
    stream2 >> length;
    if ( length <= 0 || stream2.status() != QDataStream::Ok )
        return false;
    length = qMin(length, maxItems) - model->rowCount();

    for ( quint64 i = 0; i < length && stream2.status() == QDataStream::Ok; ++i ) {
        if ( !model->insertRow(i) )
            return false;
        QVariantMap dataMap;
        stream2 >> dataMap;
        model->setData( model->index(i, 0), dataMap, contentType::data );
    }

    if (stream2.status() != QDataStream::Ok)
        return false;

    return true;
}

bool ItemEncryptedLoader::saveItems(const QAbstractItemModel &model, QFile *file)
{
    if (m_gpgProcessStatus == GpgNotInstalled)
        return false;

    quint64 length = model.rowCount();
    if (length == 0)
        return false; // No need to encode empty tab.

    QByteArray bytes;

    {
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_4_7);

        stream << length;

        for (quint64 i = 0; i < length && stream.status() == QDataStream::Ok; ++i) {
            QModelIndex index = model.index(i, 0);
            const QVariantMap dataMap = index.data(contentType::data).toMap();
            stream << dataMap;
        }
    }

    bytes = readGpgOutput(QStringList("--encrypt"), bytes);
    if ( bytes.isEmpty() )
        return false;

    QDataStream stream(file);
    stream << QString(dataFileHeaderV2);
    stream.writeRawData( bytes.data(), bytes.size() );

    return true;
}

bool ItemEncryptedLoader::initializeTab(QAbstractItemModel *)
{
    return true;
}

void ItemEncryptedLoader::setPassword()
{
    if (m_gpgProcessStatus == GpgGeneratingKeys)
        return;

    if (m_gpgProcess != NULL) {
        terminateGpgProcess();
        return;
    }

    if ( !keysExist() ) {
        // Generate keys if they don't exist.
        const KeyPairPaths keys;
        m_gpgProcessStatus = GpgGeneratingKeys;
        m_gpgProcess = new QProcess(this);
        startGpgProcess( m_gpgProcess, QStringList() << "--batch" << "--gen-key" );
        m_gpgProcess->write( "\nKey-Type: RSA"
                 "\nKey-Usage: encrypt"
                 "\nKey-Length: 2048"
                 "\nName-Real: copyq"
                 "\n%secring " + keys.sec.toUtf8() +
                 "\n%pubring " + keys.pub.toUtf8() +
                 "\n%commit"
                 "\n" );
        m_gpgProcess->closeWriteChannel();
    } else {
        // Change password.
        m_gpgProcessStatus = GpgChangingPassword;
        m_gpgProcess = new QProcess(this);
        startGpgProcess( m_gpgProcess, QStringList() << "--edit-key" << "copyq" << "passwd" << "save");
    }

    m_gpgProcess->waitForStarted();
    if ( m_gpgProcess->state() == QProcess::NotRunning ) {
        onGpgProcessFinished( m_gpgProcess->exitCode(), m_gpgProcess->exitStatus() );
    } else {
        connect( m_gpgProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
                 this, SLOT(onGpgProcessFinished(int,QProcess::ExitStatus)) );
        updateUi();
    }
}

void ItemEncryptedLoader::terminateGpgProcess()
{
    if (m_gpgProcess == NULL)
        return;
    QProcess *p = m_gpgProcess;
    m_gpgProcess = NULL;
    p->terminate();
    p->waitForFinished();
    p->deleteLater();
    m_gpgProcessStatus = GpgNotRunning;
    updateUi();
}

void ItemEncryptedLoader::onGpgProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString error;

    if (m_gpgProcess != NULL) {
        if (ui != NULL) {
            error = tr("Error: %1");
            if (exitStatus != QProcess::NormalExit)
                error = error.arg(m_gpgProcess->errorString());
            else if (exitCode != 0)
                error = error.arg(QString::fromUtf8(m_gpgProcess->readAllStandardError()));
            else if ( m_gpgProcess->error() != QProcess::UnknownError )
                error = error.arg(m_gpgProcess->errorString());
            else if ( !keysExist() )
                error = error.arg( tr("Failed to generate keys.") );
            else
                error.clear();
        }

        m_gpgProcess->deleteLater();
        m_gpgProcess = NULL;
    }

    GpgProcessStatus oldStatus = m_gpgProcessStatus;
    m_gpgProcessStatus = GpgNotRunning;

    if ( oldStatus == GpgGeneratingKeys && error.isEmpty() ) {
        setPassword();
    } else {
        updateUi();
        ui->labelInfo->setText( error.isEmpty() ? tr("Done") : error );
    }
}

void ItemEncryptedLoader::updateUi()
{
    if (ui == NULL)
        return;

    if (m_gpgProcessStatus == GpgNotInstalled) {
        ui->labelInfo->setText("To use item encryption, install"
                               " <a href=\"http://www.gnupg.org/\">GnuPG</a>"
                               " application and restart CopyQ.");
        ui->pushButtonPassword->hide();
        ui->groupBoxEncryptTabs->hide();
        ui->groupBoxShareInfo->hide();
    } else if (m_gpgProcessStatus == GpgGeneratingKeys) {
        ui->labelInfo->setText( tr("Creating new keys (this may take a few minutes)...") );
        ui->pushButtonPassword->setText( tr("Cancel") );
    } else if (m_gpgProcessStatus == GpgChangingPassword) {
        ui->labelInfo->setText( tr("Setting new password...") );
        ui->pushButtonPassword->setText( tr("Cancel") );
    } else if ( !keysExist() ) {
        ui->labelInfo->setText( tr("Encryption keys <strong>must be generated</strong>"
                                   " before item encryption can be used.") );
        ui->pushButtonPassword->setText( tr("Generate New Keys...") );
    } else {
        ui->pushButtonPassword->setText( tr("Change Password...") );
    }
}

Q_EXPORT_PLUGIN2(itemencrypted, ItemEncryptedLoader)
