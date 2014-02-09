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

#include "clipboardserver.h"

#include "app/remoteprocess.h"
#include "common/arguments.h"
#include "gui/clipboardbrowser.h"
#include "gui/configtabshortcuts.h"
#include "gui/configurationmanager.h"
#include "gui/mainwindow.h"
#include "item/clipboarditem.h"
#include "item/itemfactory.h"
#include "item/encrypt.h"
#include "item/serialize.h"
#include "scriptable/scriptableworker.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QSessionManager>
#include <QThread>

#ifdef NO_GLOBAL_SHORTCUTS
struct QxtGlobalShortcut {};
#else
#include "../qxt/qxtglobalshortcut.h"
#endif

namespace {

int monitorProcessId = 0;

} // namespace

ClipboardServer::ClipboardServer(int &argc, char **argv, const QString &sessionName)
    : QObject()
    , App(createPlatformNativeInterface()->createServerApplication(argc, argv), sessionName)
    , m_server(NULL)
    , m_wnd()
    , m_monitor(NULL)
    , m_checkclip(false)
    , m_lastHash(0)
    , m_ignoreNextItem(true)
    , m_shortcutActions()
    , m_clientThreads()
    , m_internalThreads()
{
    m_server = newServer( clipboardServerName(), this );
    if ( !m_server->isListening() )
        return;

    QApplication::setQuitOnLastWindowClosed(false);

    m_wnd = QSharedPointer<MainWindow>(new MainWindow);

    connect( m_server, SIGNAL(newConnection()),
             this, SLOT(newConnection()) );

    connect( QCoreApplication::instance(), SIGNAL(aboutToQuit()),
             this, SLOT(onAboutToQuit()));

    connect( qApp, SIGNAL(commitDataRequest(QSessionManager&)),
             this, SLOT(onCommitData(QSessionManager&)) );

    connect( m_wnd.data(), SIGNAL(changeClipboard(QVariantMap)),
             this, SLOT(changeClipboard(QVariantMap)) );

    connect( m_wnd.data(), SIGNAL(requestExit()),
             this, SLOT(maybeQuit()) );

    loadSettings();

    // notify window if configuration changes
    ConfigurationManager *cm = ConfigurationManager::instance();
    connect( cm, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

    // hash of the last clipboard data
    bool ok;
    m_lastHash = cm->value("_last_hash").toUInt(&ok);
    if (!ok)
        m_lastHash = 0;

    // run clipboard monitor
    startMonitoring();

    QCoreApplication::instance()->installEventFilter(this);
}

ClipboardServer::~ClipboardServer()
{
    // delete shortcuts manually
    foreach (QxtGlobalShortcut *s, m_shortcutActions.keys())
        delete s;
    m_shortcutActions.clear();
}

bool ClipboardServer::isListening() const
{
    return m_server->isListening();
}

void ClipboardServer::stopMonitoring()
{
    if (m_monitor == NULL)
        return;

    log( tr("Clipboard Monitor: Terminating") );

    m_monitor->disconnect();
    m_monitor->closeConnection();
    m_monitor->deleteLater();
    m_monitor = NULL;

    log( tr("Clipboard Monitor: Terminated") );
}

void ClipboardServer::startMonitoring()
{
    COPYQ_LOG("Starting monitor.");

    if ( m_monitor == NULL ) {
        m_ignoreNextItem = true;
        m_monitor = new RemoteProcess(this);
        connect( m_monitor, SIGNAL(newMessage(QByteArray)),
                 this, SLOT(newMonitorMessage(QByteArray)) );
        connect( m_monitor, SIGNAL(connectionError()),
                 this, SLOT(monitorConnectionError()) );
        connect( m_monitor, SIGNAL(connected()),
                 this, SLOT(loadMonitorSettings()) );

        QString name = clipboardMonitorServerName().arg(monitorProcessId);
        while ( !QLocalServer::removeServer(name) )
            name = clipboardMonitorServerName().arg(++monitorProcessId);

        m_monitor->start( name, QStringList("monitor") << name );
    }
    m_wnd->browser(0)->setAutoUpdate(true);
}

void ClipboardServer::loadMonitorSettings()
{
    if ( !isMonitoring() ) {
        COPYQ_LOG("Cannot configure monitor!");
        return;
    }

    COPYQ_LOG("Configuring monitor.");

    ConfigurationManager *cm = ConfigurationManager::instance();

    QVariantMap settings;
    settings["formats"] = cm->itemFactory()->formatsToSave();
    m_checkclip = cm->value("check_clipboard").toBool();
#ifdef COPYQ_WS_X11
    settings["copy_clipboard"] = cm->value("copy_clipboard");
    settings["copy_selection"] = cm->value("copy_selection");
    settings["check_selection"] = cm->value("check_selection");
#endif

    m_lastHash = 0;

    QByteArray settingsData;
    QDataStream settingsOut(&settingsData, QIODevice::WriteOnly);
    settingsOut << settings;

    const QVariantMap data = createDataMap(mimeApplicationSettings, settingsData);
    m_monitor->writeMessage( serializeData(data) );
}

bool ClipboardServer::isMonitoring()
{
    return m_monitor != NULL && m_monitor->isConnected();
}

void ClipboardServer::newConnection()
{
    QLocalSocket* client( m_server->nextPendingConnection() );

#ifdef COPYQ_LOG_DEBUG
    quintptr id = client->socketDescriptor();
    COPYQ_LOG( QString("New client connection %1").arg(id) );
    COPYQ_LOG( QString("%1: Receiving message from client.").arg(id) );
#endif

    Arguments args;
    QByteArray msg;

    if ( readMessage(client, &msg) ) {
        QDataStream in(msg);
        in >> args;

        COPYQ_LOG( QString("%1: Message received from client.").arg(id) );

        // try to handle command
        doCommand(args, client);
    } else {
        const QString error = client->errorString();
        log( tr("Cannot read message from client! (%1)").arg(error), LogError );
        client->deleteLater();
    }
}

void ClipboardServer::sendMessage(QLocalSocket* client, const QByteArray &message, int exitCode)
{
#ifdef COPYQ_LOG_DEBUG
    quintptr id = client->socketDescriptor();
#endif

    COPYQ_LOG( QString("%1: Sending message to client (exit code: %2).")
               .arg(id)
               .arg(exitCode) );

    if ( client->state() == QLocalSocket::ConnectedState ) {
        QByteArray msg;
        QDataStream out(&msg, QIODevice::WriteOnly);
        out << exitCode;
        out.writeRawData( message.constData(), message.length() );
        if ( !writeMessage(client, msg) ) {
            COPYQ_LOG( QString("%1: Failed to send message to client!").arg(id) );
        } else if (exitCode == CommandFinished) {
            connect(client, SIGNAL(disconnected()),
                    client, SLOT(deleteLater()));
            COPYQ_LOG( QString("%1: Disconnected from client.").arg(id) );
        } else if (exitCode == CommandExit) {
            client->flush();
            connect(client, SIGNAL(destroyed()), qApp, SLOT(quit()));
        }
        COPYQ_LOG( QString("%1: Message send to client.").arg(id) );
    } else {
        COPYQ_LOG( QString("%1: Client disconnected!").arg(id) );
    }
}

void ClipboardServer::onAboutToQuit()
{
    COPYQ_LOG("Closing server.");

    m_wnd->saveTabs();

    emit terminateClientThreads();
    m_clientThreads.waitForDone(1000);
    m_internalThreads.waitForDone(1000);

    if( isMonitoring() )
        stopMonitoring();
}

void ClipboardServer::onCommitData(QSessionManager &sessionManager)
{
    if ( sessionManager.allowsInteraction() && !askToQuit() )
        sessionManager.cancel();
    else
        m_wnd->saveTabs();
}

void ClipboardServer::maybeQuit()
{
    if (askToQuit())
        QCoreApplication::exit();
}

bool ClipboardServer::askToQuit()
{
    if ( m_clientThreads.activeThreadCount() > 0 || m_wnd->hasRunningAction() ) {
        QMessageBox messageBox( QMessageBox::Warning, tr("Cancel Active Commands"),
                                tr("Cancel active commands and exit?"), QMessageBox::NoButton,
                                m_wnd.data() );

        messageBox.addButton(tr("Cancel Exiting"), QMessageBox::RejectRole);
        messageBox.addButton(tr("Exit Anyway"), QMessageBox::AcceptRole);

        messageBox.exec();
        return messageBox.result() == QMessageBox::Accepted;
    }

    return true;
}

void ClipboardServer::newMonitorMessage(const QByteArray &message)
{
    QVariantMap data;

    if ( !deserializeData(&data, message) ) {
        log( tr("Failed to read message from monitor."), LogError );
        return;
    }

    ClipboardItem item;
    item.setData(data);

    if ( data.contains(mimeMessage) ) {
        const QByteArray bytes = data[mimeMessage].toByteArray();
        foreach( const QByteArray &line, bytes.split('\n') )
            log( QString::fromUtf8(line), LogNote );
    } else {
#ifdef COPYQ_WS_X11
        if ( data.value(mimeClipboardMode) != "selection" )
            m_wnd->clipboardChanged(item.data());
#else
        m_wnd->clipboardChanged(item.data());
#endif

        if (m_ignoreNextItem) {
            // Don't add item to list on application start.
            m_ignoreNextItem = false;
            m_lastHash = item.dataHash();
        } else if ( ownsClipboardData(data) ) {
            // Don't add item to list if any running clipboard monitor set the clipboard.
        } else if ( m_checkclip && !item.isEmpty() && m_lastHash != item.dataHash() ) {
            m_lastHash = item.dataHash();
            if ( !m_wnd->isClipboardStoringDisabled() )
                m_wnd->addToTab( item.data(), QString(), true );
        }
    }
}

void ClipboardServer::monitorConnectionError()
{
    stopMonitoring();
    startMonitoring();
}

void ClipboardServer::changeClipboard(const QVariantMap &data)
{
    if ( !isMonitoring() ) {
        COPYQ_LOG("Cannot send message to monitor!");
        return;
    }

    COPYQ_LOG("Sending message to monitor.");

    m_monitor->writeMessage( serializeData(data) );
    m_lastHash = hash(data);
}

void ClipboardServer::doCommand(const Arguments &args, QLocalSocket *client)
{
    // Worker object without parent needs to be deleted afterwards!
    // There is no parent so as it's possible to move the worker to another thread.
    ScriptableWorker *worker = new ScriptableWorker(m_wnd, args, client);

    // Delete worker after it's finished.
    connect( worker, SIGNAL(finished()),
             worker, SLOT(deleteLater()) );

    // Terminate worket at application exit.
    connect( this, SIGNAL(terminateClientThreads()),
             worker, SLOT(terminate()) );

    if (client != NULL) {
        connect(client, SIGNAL(disconnected()),
                worker, SLOT(terminate()));
        connect(worker, SIGNAL(sendMessage(QLocalSocket*,QByteArray,int)),
                this, SLOT(sendMessage(QLocalSocket*,QByteArray,int)));

        // Add client thread to pool.
        m_clientThreads.start(worker);
    } else {
        // Run internally created command immediatelly (should be fast).
        m_internalThreads.start(worker);
    }
}

void ClipboardServer::createGlobalShortcut(Actions::Id id, const QByteArray &script)
{
#ifdef NO_GLOBAL_SHORTCUTS
    Q_UNUSED(id);
    Q_UNUSED(script);
#else
    ConfigTabShortcuts *shortcuts = ConfigurationManager::instance()->tabShortcuts();
    foreach ( const QKeySequence &shortcut, shortcuts->shortcuts(id) ) {
        QxtGlobalShortcut *s = new QxtGlobalShortcut(shortcut, this);
        connect( s, SIGNAL(activated(QxtGlobalShortcut*)),
                 this, SLOT(shortcutActivated(QxtGlobalShortcut*)) );

        // Don't process global shortcuts any further.
        // FIXME: This should be set for all modal windows.
        QAction *act = new QAction(this);
        act->setShortcut(shortcut);
        act->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        act->setPriority(QAction::HighPriority);
        m_wnd->addAction(act);
        ConfigurationManager::instance()->addAction(act);
        connect( s, SIGNAL(destroyed()), act, SLOT(deleteLater()) );

        m_shortcutActions[s] = script;
    }
#endif
}

bool ClipboardServer::eventFilter(QObject *object, QEvent *ev)
{
    // Close menu on Escape key and give focus back to search edit or browser.
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyevent = static_cast<QKeyEvent *>(ev);
        if (keyevent->key() == Qt::Key_Escape) {
            QMenu *menu = qobject_cast<QMenu*>(object);
            if (menu != NULL) {
                menu->close();
                m_wnd->enterBrowseMode(m_wnd->browseMode());
            }
        }
    }
    return false;
}

void ClipboardServer::loadSettings()
{
#ifndef NO_GLOBAL_SHORTCUTS
    foreach (QxtGlobalShortcut *s, m_shortcutActions.keys())
        delete s;
    m_shortcutActions.clear();

    createGlobalShortcut(Actions::Global_ToggleMainWindow, "toggle()");
    createGlobalShortcut(Actions::Global_ShowTray, "menu()");
    createGlobalShortcut(Actions::Global_EditClipboard, "edit(-1)");
    createGlobalShortcut(Actions::Global_EditFirstItem, "edit(0)");
    createGlobalShortcut(Actions::Global_CopySecondItem, "select(1)");
    createGlobalShortcut(Actions::Global_ShowActionDialog, "action()");
    createGlobalShortcut(Actions::Global_CreateItem, "edit()");
    createGlobalShortcut(Actions::Global_CopyNextItem, "next()");
    createGlobalShortcut(Actions::Global_CopyPreviousItem, "previous()");
    createGlobalShortcut(Actions::Global_PasteAsPlainText, "copy(clipboard()); paste()");
    createGlobalShortcut(Actions::Global_DisableClipboardStoring, "disable()");
    createGlobalShortcut(Actions::Global_EnableClipboardStoring, "enable()");
    createGlobalShortcut(Actions::Global_PasteAndCopyNext, "paste(); next();");
    createGlobalShortcut(Actions::Global_PasteAndCopyPrevious, "paste(); previous();");
#endif

    // reload clipboard monitor configuration
    if ( isMonitoring() )
        loadMonitorSettings();
}

void ClipboardServer::shortcutActivated(QxtGlobalShortcut *shortcut)
{
    Arguments args;
    args.append("eval");
    args.append(m_shortcutActions[shortcut]);
    doCommand(args);
}
