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

#include "actiondialog.h"
#include "ui_actiondialog.h"

#include "common/action.h"
#include "common/command.h"
#include "common/common.h"
#include "item/serialize.h"
#include "gui/configurationmanager.h"

#include <QSettings>
#include <QFile>
#include <QMessageBox>

namespace {

void initFormatComboBox(QComboBox *combo, const QStringList &additionalFormats = QStringList())
{
    QStringList formats = QStringList() << QString() << QString(mimeText) << additionalFormats;
    formats.removeDuplicates();
    combo->clear();
    combo->addItems(formats);
}

bool wasChangedByUser(QObject *object)
{
    return object->property("UserChanged").toBool();
}

void setChangedByUser(QWidget *object)
{
    object->setProperty("UserChanged", object->hasFocus());
}

QString commandToLabel(const QString &command)
{
    QString label = command.size() > 48 ? command.left(48) + "..." : command;
    label.replace('\n', " ");
    label.replace(QRegExp("\\s\\+"), " ");
    return label;
}

int findCommand(const QComboBox &comboBox, const QVariant &itemData)
{
    for (int i = 0; i < comboBox.count(); ++i) {
        if (comboBox.itemData(i) == itemData)
            return i;
    }

    return -1;
}

} // namespace

ActionDialog::ActionDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ActionDialog)
    , m_data()
    , m_index()
    , m_capturedTexts()
    , m_currentCommandIndex(-1)
{
    ui->setupUi(this);

    QFont monoSpaced("monospace");
    ui->plainTextEditCommand->setFont(monoSpaced);
    ui->comboBoxCommands->setFont(monoSpaced);

    on_comboBoxInputFormat_currentIndexChanged(QString());
    on_comboBoxOutputFormat_editTextChanged(QString());
    loadSettings();
}

ActionDialog::~ActionDialog()
{
    delete ui;
}

void ActionDialog::setInputData(const QVariantMap &data)
{
    m_data = data;

    QString defaultFormat = ui->comboBoxInputFormat->currentText();
    initFormatComboBox(ui->comboBoxInputFormat, data.keys());
    const int index = qMax(0, ui->comboBoxInputFormat->findText(defaultFormat));
    ui->comboBoxInputFormat->setCurrentIndex(index);
}

void ActionDialog::restoreHistory()
{
    ConfigurationManager *cm = ConfigurationManager::instance();

    int maxCount = cm->value("command_history_size").toInt();
    ui->comboBoxCommands->setMaxCount(maxCount);

    QFile file( dataFilename() );
    file.open(QIODevice::ReadOnly);
    QDataStream in(&file);
    QVariant v;

    ui->comboBoxCommands->clear();
    ui->comboBoxCommands->addItem(QString());
    while( !in.atEnd() ) {
        in >> v;
        if (v.canConvert(QVariant::String)) {
            // backwards compatibility with versions up to 1.8.2
            QVariantMap values;
            values["cmd"] = v;
            ui->comboBoxCommands->addItem(commandToLabel(v.toString()), values);
        } else {
            QVariantMap values = v.value<QVariantMap>();
            ui->comboBoxCommands->addItem(commandToLabel(values["cmd"].toString()), v);
        }
    }
    ui->comboBoxCommands->setCurrentIndex(0);
}

const QString ActionDialog::dataFilename() const
{
    // do not use registry in windows
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    // .ini -> _cmds.dat
    QString datfilename = settings.fileName();
    datfilename.replace( QRegExp("\\.ini$"),QString("_cmds.dat") );

    return datfilename;
}

void ActionDialog::saveHistory()
{
    QFile file( dataFilename() );
    file.open(QIODevice::WriteOnly);
    QDataStream out(&file);

    for (int i = 1; i < ui->comboBoxCommands->count(); ++i) {
        QVariant itemData = ui->comboBoxCommands->itemData(i);
        if ( !itemData.toMap().value("cmd").toString().isEmpty() )
            out << itemData;
    }
}

void ActionDialog::createAction()
{
    const QString cmd = ui->plainTextEditCommand->toPlainText();

    if ( cmd.isEmpty() )
        return;

    const QString format = ui->comboBoxInputFormat->currentText();
    const QString input = ( format.isEmpty() || format.toLower().startsWith(QString("text")) )
            ? ui->inputText->toPlainText() : QString();

    QByteArray bytes;
    QStringList inputFormats;
    if ( !format.isEmpty() ) {
        if ( m_index.isValid() )
            inputFormats.append(format);

        if ( !input.isEmpty() ) {
            bytes = input.toLocal8Bit();
        } else if ( !m_data.isEmpty() ) {
            if (format == mimeItems) {
                QVariantMap data2;
                inputFormats.clear();
                foreach ( const QString &format, m_data.keys() ) {
                    if ( !format.startsWith(MIME_PREFIX) ) {
                        data2.insert( format, m_data[format] );
                        if ( m_index.isValid() )
                            inputFormats.append(format);
                    }
                }
                bytes = serializeData(data2);
            } else {
                bytes = m_data.value(format).toByteArray();
            }
        }
    }

    Action *act = new Action( cmd, bytes, m_capturedTexts, inputFormats,
                              ui->comboBoxOutputFormat->currentText(),
                              ui->separatorEdit->text(),
                              ui->comboBoxOutputTab->currentText(),
                              m_index );
    emit accepted(act);

    close();
}

void ActionDialog::setCommand(const QString &cmd)
{
    ui->comboBoxCommands->setCurrentIndex(0);
    ui->plainTextEditCommand->setPlainText(cmd);
}

void ActionDialog::setSeparator(const QString &sep)
{
    ui->separatorEdit->setText(sep);
}

void ActionDialog::setInput(const QString &format)
{
    int index = ui->comboBoxInputFormat->findText(format);
    if (index == -1) {
        ui->comboBoxInputFormat->insertItem(0, format);
        index = 0;
    }
    ui->comboBoxInputFormat->setCurrentIndex(index);
}

void ActionDialog::setOutput(const QString &format)
{
    ui->comboBoxOutputFormat->setEditText(format);
}

void ActionDialog::setOutputTabs(const QStringList &tabs,
                                 const QString &currentTabName)
{
    QComboBox *w = ui->comboBoxOutputTab;
    w->clear();
    w->addItem("");
    w->addItems(tabs);
    w->setEditText(currentTabName);
}

void ActionDialog::setCapturedTexts(const QStringList &capturedTexts)
{
    m_capturedTexts = capturedTexts;
}

void ActionDialog::setOutputIndex(const QModelIndex &index)
{
    m_index = index;
}

void ActionDialog::loadSettings()
{
    initFormatComboBox(ui->comboBoxInputFormat);
    initFormatComboBox(ui->comboBoxOutputFormat);

    restoreHistory();
    ConfigurationManager::instance()->loadGeometry(this);
}

void ActionDialog::accept()
{
    QVariant itemData = createCurrentItemData();
    int i = findCommand(*ui->comboBoxCommands, itemData);
    if (i != -1)
        ui->comboBoxCommands->removeItem(i);

    const QString text = ui->plainTextEditCommand->toPlainText();
    ui->comboBoxCommands->insertItem(1, commandToLabel(text), itemData);

    saveHistory();

    QDialog::accept();
}

void ActionDialog::closeEvent(QCloseEvent *event)
{
    ConfigurationManager::instance()->saveGeometry(this);
    QDialog::closeEvent(event);
}

void ActionDialog::on_buttonBox_clicked(QAbstractButton* button)
{
    Command cmd;
    ConfigurationManager *cm;

    switch ( ui->buttonBox->standardButton(button) ) {
    case QDialogButtonBox::Ok:
        createAction();
        break;
    case QDialogButtonBox::Save:
        cmd.cmd = ui->plainTextEditCommand->toPlainText();
        cmd.name = commandToLabel(cmd.cmd);
        cmd.input = ui->comboBoxInputFormat->currentText();
        cmd.output = ui->comboBoxOutputFormat->currentText();
        cmd.sep = ui->separatorEdit->text();
        cmd.outputTab = ui->comboBoxOutputTab->currentText();

        cm = ConfigurationManager::instance();
        cm->addCommand(cmd);
        QMessageBox::information(
                    this, tr("Command saved"),
                    tr("Command was saved and can be accessed from item menu.\n"
                       "You can set up the command in preferences.") );
        break;
    case QDialogButtonBox::Cancel:
        close();
        break;
    default:
        break;
    }
}

void ActionDialog::on_comboBoxCommands_currentIndexChanged(int index)
{
    if ( m_currentCommandIndex >= 0 && m_currentCommandIndex < ui->comboBoxCommands->count() ) {
        QVariant itemData = createCurrentItemData();
        if (itemData != ui->comboBoxCommands->itemData(m_currentCommandIndex))
            ui->comboBoxCommands->setItemData(m_currentCommandIndex, itemData);
    }

    m_currentCommandIndex = index;

    // Restore values from history.
    QVariant v = ui->comboBoxCommands->itemData(index);
    QVariantMap values = v.value<QVariantMap>();

    ui->plainTextEditCommand->setPlainText(values.value("cmd").toString());

    // Don't automatically change values if they were edited by user.
    if ( !wasChangedByUser(ui->comboBoxInputFormat) ) {
        int i = ui->comboBoxInputFormat->findText(values.value("input").toString());
        if (i != -1)
            ui->comboBoxInputFormat->setCurrentIndex(i);
    }

    if ( !wasChangedByUser(ui->comboBoxOutputFormat) )
        ui->comboBoxOutputFormat->setEditText(values.value("output").toString());

    if ( !wasChangedByUser(ui->separatorEdit) )
        ui->separatorEdit->setText(values.value("sep").toString());

    if ( !wasChangedByUser(ui->comboBoxOutputTab) )
        ui->comboBoxOutputTab->setEditText(values.value("outputTab").toString());
}

void ActionDialog::on_comboBoxInputFormat_currentIndexChanged(const QString &format)
{
    setChangedByUser(ui->comboBoxInputFormat);

    bool show = format.toLower().startsWith(QString("text"));
    ui->inputText->setVisible(show);

    QString text;
    if ((show || format.isEmpty()) && !m_data.isEmpty() )
        text = getTextData( m_data, format.isEmpty() ? mimeText : format );
    ui->inputText->setPlainText(text);
}

void ActionDialog::on_comboBoxOutputFormat_editTextChanged(const QString &text)
{
    setChangedByUser(ui->comboBoxOutputFormat);

    bool showSeparator = text.toLower().startsWith(QString("text"));
    ui->separatorLabel->setVisible(showSeparator);
    ui->separatorEdit->setVisible(showSeparator);

    bool showOutputTab = !text.isEmpty();
    ui->labelOutputTab->setVisible(showOutputTab);
    ui->comboBoxOutputTab->setVisible(showOutputTab);
}

void ActionDialog::on_comboBoxOutputTab_editTextChanged(const QString &)
{
    setChangedByUser(ui->comboBoxOutputTab);
}

void ActionDialog::on_separatorEdit_textEdited(const QString &)
{
    setChangedByUser(ui->separatorEdit);
}

QVariant ActionDialog::createCurrentItemData()
{
    QVariantMap values;
    values["cmd"] = ui->plainTextEditCommand->toPlainText();
    values["input"] = ui->comboBoxInputFormat->currentText();
    values["output"] = ui->comboBoxOutputFormat->currentText();
    values["sep"] = ui->separatorEdit->text();
    values["outputTab"] = ui->comboBoxOutputTab->currentText();

    return values;
}
