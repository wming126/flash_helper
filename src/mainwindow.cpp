#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "chippreviewwidget.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDataStream>
#include <QCoreApplication>
#include <QDir>
#include <QThread>
#include <QStandardPaths>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QCompleter>
#include <QDebug>
#include <QInputDialog>
#include <QGroupBox>
#include <QAbstractItemView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    localSpi = new LocalSpiDriver();
    translator = new QTranslator(this);
    idleTimer = new QTimer(this);
    idleTimer->setSingleShot(true);
    connect(idleTimer, &QTimer::timeout, this, [this]() { ui->statusbar->showMessage(tr("Idle")); });

    ui->setupUi(this);
    setWindowIcon(QIcon(":/flashhelper.svg"));
    process = new QProcess(this);
    ui->progressBar->hide();
    
    previewGroup = new QGroupBox(tr("Chip Pinout Preview"), this);
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    chipPreview = new ChipPreviewWidget(this);
    previewLayout->addWidget(chipPreview);
    
    QWidget *spiContent = new QWidget(this);
    QHBoxLayout *spiMainLayout = new QHBoxLayout(spiContent);
    QVBoxLayout *spiLeftLayout = new QVBoxLayout();
    while (ui->verticalLayout_2->count() > 0) {
        QLayoutItem *item = ui->verticalLayout_2->takeAt(0);
        if (item->layout()) spiLeftLayout->addLayout(item->layout());
        else if (item->widget()) spiLeftLayout->addWidget(item->widget());
    }
    spiMainLayout->addLayout(spiLeftLayout, 2);
    spiMainLayout->addWidget(previewGroup, 1);
    ui->verticalLayout_2->addWidget(spiContent);

    QWidget *eepContent = new QWidget(this);
    QHBoxLayout *eepMainLayout = new QHBoxLayout(eepContent);
    QVBoxLayout *eepLeftLayout = new QVBoxLayout();
    while (ui->verticalLayout_eeprom->count() > 0) {
        QLayoutItem *item = ui->verticalLayout_eeprom->takeAt(0);
        if (item->layout()) eepLeftLayout->addLayout(item->layout());
        else if (item->widget()) eepLeftLayout->addWidget(item->widget());
    }
    eepMainLayout->addLayout(eepLeftLayout, 2);
    ui->verticalLayout_eeprom->addWidget(eepContent);

    ui->comboChip->setMinimumWidth(350);
    ui->comboChip->setMaxVisibleItems(20);
    ui->comboChip->view()->setMinimumWidth(400);
    ui->comboEepromChip->setMinimumWidth(350);
    ui->comboEepromChip->view()->setMinimumWidth(400);

    connect(ui->comboChip, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        chipPreview->setChipModel(text);
    });
    connect(ui->comboEepromChip, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        chipPreview->setChipModel(text);
    });
    
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this, spiMainLayout, eepMainLayout](int index) {
        if (index == 0) spiMainLayout->addWidget(this->previewGroup);
        else if (index == 1) eepMainLayout->addWidget(this->previewGroup);
        
        // Hide editor and log on "About" tab (index 4)
        bool isAbout = (index == 4);
        ui->editorContainer->setVisible(!isAbout);
        ui->textLog->setVisible(!isAbout);

        // Progress bar management
        if (index != 2) ui->progressBar->hide();
    });

    // Populate About tab details
    QString version = "v1.3.0"; // Should ideally come from a macro or build system
#ifdef APP_VERSION
    version = APP_VERSION;
#endif
    ui->label_title_about->setText(QString("FlashHelper %1").arg(version));
    
    QLabel *instructions = new QLabel(this);
    instructions->setWordWrap(true);
    instructions->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    instructions->setTextFormat(Qt::RichText);
    instructions->setText(tr("<h3>Tool Description</h3>"
                             "<p>FlashHelper is a graphical front-end for flashrom, designed for BIOS and EEPROM flashing.</p>"
                             "<h3>Usage Instructions</h3>"
                             "<ul>"
                             "<li><b>SPI/EEPROM:</b> Select programmer, chip model, and file, then click Read/Write.</li>"
                             "<li><b>Local Flash:</b> Direct hardware access for Loongson platforms (requires root).</li>"
                             "<li><b>System Setup:</b> Install udev rules to enable non-root access for USB programmers.</li>"
                             "</ul>"));
    ui->verticalLayout_about->insertWidget(4, instructions);

    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::readProcessOutput);
    connect(process, &QProcess::readyReadStandardError, this, &MainWindow::readProcessOutput);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, &MainWindow::processFinished);

    ui->comboProgrammer->addItem(tr("Serprog (STM32)"), "serprog:dev=/dev/ttyACM0:4000000");
    ui->comboProgrammer->addItem(tr("CH341A SPI"), "ch341a_spi");
    ui->comboProgrammer->addItem(tr("FT2232 SPI"), "ft2232_spi");
    ui->comboProgrammer->addItem(tr("Bus Pirate SPI"), "buspirate_spi:dev=/dev/ttyUSB0");
    ui->comboProgrammer->addItem(tr("Dediprog"), "dediprog");
    ui->comboProgrammer->addItem(tr("STLINK-V3 SPI"), "stlinkv3_spi");
    ui->comboProgrammer->addItem(tr("PICkit 2 SPI"), "pickit2_spi");
    ui->comboProgrammer->addItem(tr("DirtyJTAG SPI"), "dirtyjtag_spi");
    ui->comboProgrammer->addItem(tr("Linux SPI"), "linux_spi:dev=/dev/spidev1.0");
    ui->comboProgrammer->addItem(tr("Dummy (Test only)"), "dummy");

    QString arch = QSysInfo::currentCpuArchitecture();
    if (arch.contains("loongarch") || arch.contains("la64")) {
        ui->comboProgrammer->addItem(tr("Internal (Loongson SPI)"), "internal");
    }
    ui->comboProgrammer->setCurrentIndex(0);

    ui->comboSpeed->addItem(tr("Default"), "");
    ui->comboSpeed->addItem("36 MHz", "36000000");
    ui->comboSpeed->addItem("20 MHz", "20000000");
    ui->comboSpeed->addItem("16 MHz", "16000000");
    ui->comboSpeed->addItem("8 MHz", "8000000");
    ui->comboSpeed->addItem("4 MHz", "4000000");
    ui->comboSpeed->addItem("2 MHz", "2000000");
    ui->comboSpeed->addItem("1 MHz", "1000000");

    ui->comboEepromProg->addItem(tr("CH341A SPI (I2C Patched)"), "ch341a_spi");
    ui->comboEepromProg->addItem(tr("Bus Pirate"), "buspirate_spi");
    ui->comboEepromProg->addItem(tr("Serprog"), "serprog");
    ui->comboEepromProg->addItem(tr("Linux SPI"), "linux_spi");

    ui->comboLang->blockSignals(true);
    ui->comboLang->addItem("English", "en");
    ui->comboLang->addItem("简体中文", "zh_CN");
    QString locale = QLocale::system().name();
    if (locale.startsWith("zh")) ui->comboLang->setCurrentIndex(1);
    else ui->comboLang->setCurrentIndex(0);
    ui->comboLang->blockSignals(false);
    on_comboLang_currentIndexChanged(ui->comboLang->currentIndex());

    ui->textLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->textLog, &QWidget::customContextMenuRequested, this, &MainWindow::showLogContextMenu);
    
    updateSystemStatus();
    fetchSupportedChips();
}

MainWindow::~MainWindow() { delete localSpi; delete ui; }

QString MainWindow::getFlashromPath() {
    QString localPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("flashrom");
    if (QFile::exists(localPath)) return localPath;
    return "flashrom";
}

QString MainWindow::getWorkPath(const QString &fileName) {
    return QDir::tempPath() + "/" + fileName;
}

void MainWindow::updateSystemStatus() {
    bool mmioAccess = QFileInfo("/dev/mem").isReadable();
    bool usbAccess = QFileInfo("/dev/ttyACM0").isWritable() || QFileInfo("/dev/ttyUSB0").isWritable();
    
    QString status;
    if (mmioAccess || usbAccess) status = tr("Hardware Access: <font color='green'>OK (Direct Access)</font>");
    else status = tr("Hardware Access: <font color='red'>Denied (Requires Root)</font>");
    
    ui->lblUdevStatus->setText(status);
    ui->lblPolkitStatus->setText(QFile::exists("/etc/udev/rules.d/z60_flashrom.rules") ? tr("System Config: <font color='green'>Rules Installed</font>") : tr("System Config: <font color='red'>Not Configured</font>"));
}

void MainWindow::fetchSupportedChips() {
    QString flashromPath = getFlashromPath();
    QProcess *listProc = new QProcess(this);
    connect(listProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, listProc]() {
        QString output = listProc->readAllStandardOutput();
        QStringList lines = output.split('\n');
        supportedChips.clear();
        supportedChips << tr("Auto Detect");
        bool inFlashSection = false;
        QString lastVendor;
        for (int i = 0; i < lines.size(); ++i) {
            QString trimmed = lines[i].trimmed();
            if (trimmed.isEmpty()) continue;
            if (trimmed.contains("Supported flash chips")) { inFlashSection = true; continue; }
            if (inFlashSection && (trimmed.startsWith("Supported ") || trimmed.startsWith("---"))) { inFlashSection = false; }
            if (trimmed.contains("Supported PCI devices")) { inFlashSection = false; break; }
            if (!inFlashSection) continue;
            if (trimmed.startsWith("==") || trimmed.startsWith("--") || trimmed.startsWith("(")) continue;
            if (trimmed.contains(":") || trimmed.contains("=") || trimmed.contains("https://")) continue;
            QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.isEmpty()) continue;
            QString first = parts[0];
            if (first == "Vendor" || first == "Device" || first == "Test" || first == "Known" || 
                first == "Size" || first == "Type" || first == "OK" || first == "Broken") continue;
            if (parts.size() >= 2) {
                lastVendor = parts[0];
                QString model = parts[1];
                supportedChips << model << QString("%1 %2").arg(lastVendor, model);
            } else if (parts.size() == 1 && !lastVendor.isEmpty()) {
                QString subModel = parts[0];
                if (subModel.length() > 2 && !subModel.contains("(")) {
                    supportedChips << subModel << QString("%1 %2").arg(lastVendor, subModel);
                }
            }
        }
        supportedChips.removeDuplicates();
        if (supportedChips.size() > 0) { QString autoStr = supportedChips.takeFirst(); supportedChips.sort(); supportedChips.prepend(autoStr); }
        QCompleter *completer = new QCompleter(supportedChips, this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        ui->comboChip->clear(); ui->comboChip->addItems(supportedChips); ui->comboChip->setCompleter(completer); ui->comboChip->setCurrentIndex(0);
        ui->comboEepromChip->clear(); ui->comboEepromChip->addItems(supportedChips); ui->comboEepromChip->setCompleter(completer); ui->comboEepromChip->setCurrentIndex(0);
        listProc->deleteLater();
    });
    listProc->start(flashromPath, {"-L"});
}

void MainWindow::on_btnInstallRules_clicked() {
    QString udevContent = "SUBSYSTEM==\"usb\", ATTR{idVendor}==\"0483\", ATTR{idProduct}==\"5740\", MODE=\"0666\", GROUP=\"plugdev\"\n"
                          "SUBSYSTEM==\"usb\", ATTR{idVendor}==\"1a86\", ATTR{idProduct}==\"5512\", MODE=\"0666\", GROUP=\"plugdev\"\n"
                          "SUBSYSTEM==\"usb\", ATTR{idVendor}==\"1a86\", ATTR{idProduct}==\"5523\", MODE=\"0666\", GROUP=\"plugdev\"\n";
    QString polkitRule = "polkit.addRule(function(action, subject) {\n"
                         "  if (action.id == \"com.robin.flashhelper.run-flashrom\" && subject.isInGroup(\"plugdev\")) {\n"
                         "    return polkit.Result.YES;\n"
                         "  }\n"
                         "});\n";
    QString script = QString("echo \"%1\" | base64 -d > /etc/udev/rules.d/z60_flashrom.rules && "
                             "echo \"%2\" | base64 -d > /etc/polkit-1/rules.d/10-flashrom.rules && "
                             "udevadm control --reload-rules && udevadm trigger && "
                             "groupadd -f plugdev && "
                             "usermod -aG plugdev %3")
                     .arg(QString(udevContent.toUtf8().toBase64()), QString(polkitRule.toUtf8().toBase64()), qgetenv("USER"));

    if (QProcess::execute("pkexec", {"bash", "-c", script}) == 0) {
        QMessageBox::information(this, tr("Success"), tr("Rules installed successfully!"));
    }
    QThread::msleep(500);
    updateSystemStatus();
}

void MainWindow::on_btnRemoveRules_clicked() {
    QProcess::execute("pkexec", {"bash", "-c", "rm -f /etc/udev/rules.d/z60_flashrom.rules /etc/polkit-1/rules.d/10-flashrom.rules"});
    updateSystemStatus();
}

void MainWindow::runCommand(const QString &cmd, const QStringList &args) {
    if (idleTimer) idleTimer->stop();
    QStringList finalArgs = args;
    QString finalCmd = cmd;
    accumulatedError.clear();
    accumulatedOutput.clear();
    
    QString flashromPath = getFlashromPath();
    QString currentChip = ui->comboChip->currentText();
    bool isAuto = (currentChip == tr("Auto Detect") || currentChip.isEmpty());
    if (!isAuto && currentState != State::Detecting && !finalArgs.contains("-c")) {
        int pIdx = finalArgs.indexOf("-p");
        if (pIdx != -1 && pIdx + 1 < finalArgs.size()) {
            finalArgs.insert(pIdx + 2, "-c");
            finalArgs.insert(pIdx + 3, currentChip);
        }
    }

    if (cmd == "pkexec" && !finalArgs.isEmpty()) {
        QString cmdToRun = finalArgs[0];
        if (cmdToRun == "flashrom") cmdToRun = flashromPath;

        // Skip pkexec if direct access is possible (e.g. STM32 with udev rules)
        if (cmdToRun.contains("flashrom")) {
            QString progArgs = getProgrammerArgs();
            if (progArgs.contains("serprog") && (QFileInfo("/dev/ttyACM0").isWritable() || QFileInfo("/dev/ttyUSB0").isWritable())) {
                finalCmd = cmdToRun;
                finalArgs.removeFirst();
                log(tr("[Direct Access]"), "gray");
                goto start_proc;
            }
        }

        if (cmdToRun.contains("/.mount_")) {
            QString binaryName = QFileInfo(cmdToRun).fileName();
            QString tempPath = QDir::tempPath() + "/" + binaryName + "_internal_" + qgetenv("USER");
            if (!QFile::exists(tempPath) || QFile::remove(tempPath)) {
                if (QFile::copy(cmdToRun, tempPath)) {
                    QFile::setPermissions(tempPath, QFile::Permissions(0x7777));
                    cmdToRun = tempPath;
                }
            }
        }
        finalArgs[0] = cmdToRun;
    }

start_proc:
    log(tr("Final Command: %1 %2").arg(finalCmd, finalArgs.join(" ")), "gray");
    QString statusText;
    switch (currentState) {
        case State::Detecting: statusText = tr("Detecting chip..."); break;
        case State::Reading: statusText = tr("Reading flash..."); break;
        case State::Writing: statusText = tr("Writing flash..."); break;
        case State::Erasing: statusText = tr("Erasing flash..."); break;
        case State::SmartRead: statusText = tr("Smart Merge: Reading..."); break;
        case State::SmartWrite: statusText = tr("Smart Merge: Writing..."); break;
        case State::EepromRead: statusText = tr("Reading EEPROM..."); break;
        case State::EepromWrite: statusText = tr("Writing EEPROM..."); break;
        case State::EepromErase: statusText = tr("Erasing EEPROM..."); break;
        default: statusText = tr("Processing..."); break;
    }
    ui->statusbar->showMessage(statusText);
    process->start(finalCmd, finalArgs);
}

bool MainWindow::runLocalHelper(const QStringList &args) {
    QString helperPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("flashhelper-helper");
    if (!QFile::exists(helperPath)) helperPath = QDir::current().absoluteFilePath("flashhelper-helper");
    if (!QFile::exists(helperPath)) helperPath = "./flashhelper-helper";
    
    if (helperPath.contains("/.mount_")) {
        QString tempPath = QDir::tempPath() + "/flashhelper-helper_internal_" + qgetenv("USER");
        if (!QFile::exists(tempPath) || QFile::remove(tempPath)) {
            if (QFile::copy(helperPath, tempPath)) {
                QFile::setPermissions(tempPath, QFile::Permissions(0x7777));
                helperPath = tempPath;
            }
        }
    }

    bool ok;
    QInputDialog dialog(this);
    dialog.setWindowTitle(tr("Authorization"));
    dialog.setLabelText(tr("Local SPI hardware access requires root privileges.\nPlease enter your password:"));
    dialog.setTextEchoMode(QLineEdit::Password);
    dialog.setOkButtonText(tr("OK"));
    dialog.setCancelButtonText(tr("Cancel"));
    dialog.setMinimumWidth(450); // Increased width
    
    ok = dialog.exec();
    QString pass = dialog.textValue();
    if (!ok || pass.isEmpty()) return false;

    accumulatedOutput.clear();
    accumulatedError.clear();
    process->start("sudo", QStringList() << "-S" << helperPath << args);
    process->write(pass.toUtf8() + "\n");
    return true;
}

void MainWindow::on_btnDetect_clicked() { currentState = State::Detecting; runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs()}); }
void MainWindow::on_btnRead_clicked() {
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save BIOS"), "backup.bin", tr("Binary (*.bin *.fd);;All (*.*)"));
    if (savePath.isEmpty()) return;
    currentFile = savePath; currentState = State::Reading;
    runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-r", savePath});
}
void MainWindow::on_btnWrite_clicked() { currentState = State::Writing; runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-w", currentFile}); }
void MainWindow::on_btnErase_clicked() { if (QMessageBox::question(this, tr("Confirm"), tr("ERASE?")) == QMessageBox::Yes) { currentState = State::Erasing; runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-E"}); } }

void MainWindow::on_btnEepromBrowse_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open"), "", tr("Binary (*.*)"));
    if (!fileName.isEmpty()) { ui->lineEepromFile->setText(fileName); eepromFile = fileName; QFile f(fileName); if (f.open(QIODevice::ReadOnly)) { loadDataToEditor(f.readAll()); f.close(); } }
}
void MainWindow::on_btnEepromRead_clicked() {
    QString chip = ui->comboEepromChip->currentText();
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save"), "eeprom.bin", tr("Binary (*.*)"));
    if (savePath.isEmpty()) return;
    eepromFile = savePath; currentFile = savePath; currentState = State::EepromRead;
    QStringList args = {"flashrom", "-p", getProgrammerArgs(true), "-r", savePath};
    if (chip != tr("Auto Detect")) args << "-c" << chip;
    runCommand("pkexec", args);
}
void MainWindow::on_btnEepromWrite_clicked() {
    QString chip = ui->comboEepromChip->currentText();
    QString targetFile = prepareWriteFile();
    if (targetFile.isEmpty()) return;
    currentState = State::EepromWrite;
    QStringList args = {"flashrom", "-p", getProgrammerArgs(true), "-w", targetFile};
    if (chip != tr("Auto Detect")) args << "-c" << chip;
    runCommand("pkexec", args);
}
void MainWindow::on_btnEepromErase_clicked() {
    QString chip = ui->comboEepromChip->currentText();
    if (QMessageBox::question(this, tr("Confirm"), tr("ERASE?")) == QMessageBox::Yes) {
        currentState = State::EepromErase;
        QStringList args = {"flashrom", "-p", getProgrammerArgs(true), "-E"};
        if (chip != tr("Auto Detect")) args << "-c" << chip;
        runCommand("pkexec", args);
    }
}

void MainWindow::readProcessOutput() {
    QString output = process->readAllStandardOutput(); 
    QString error = process->readAllStandardError();
    if (!output.isEmpty()) { log(output, "white"); accumulatedOutput += output; }
    if (!error.isEmpty()) { log(error, "yellow"); accumulatedError += error; }
    if (currentState == State::Writing && (error.contains("expected size") || error.contains("doesn't match"))) handleSmartWrite(error);
}

void MainWindow::handleSmartWrite(const QString &error) {
    QRegularExpression re("(?:file|Image) size \\((\\d+) ?B?\\).*?(?:flash chip's|expected) size \\((\\d+) ?B?\\)");
    QRegularExpressionMatch match = re.match(error);
    if (match.hasMatch()) { lastInfo.fileSize = match.captured(1).toLong(); lastInfo.flashSize = match.captured(2).toLong(); if (lastInfo.fileSize < lastInfo.flashSize) { log(tr("Size mismatch. Reading..."), "cyan"); currentState = State::SmartRead; } }
}

void MainWindow::processFinished(int exitCode) {
    QString combined = accumulatedOutput + "\n" + accumulatedError;
    if (exitCode != 0) {
        if (combined.contains("Multiple flash chip definitions match") || combined.contains("Please specify which chip definition")) {
            detectedChips.clear(); QRegularExpression re("\"([^\"]+)\""); QRegularExpressionMatchIterator i = re.globalMatch(combined);
            QStringList blackList = {"serprog", "flashrom", "mapping", "stm32", "protocol"};
            while (i.hasNext()) { QString name = i.next().captured(1); bool black = false; for (const QString &k : blackList) { if (name.contains(k, Qt::CaseInsensitive)) black = true; } if (!black && name.length() > 3) detectedChips << name; }
            if (!detectedChips.isEmpty()) {
                QString firstChip = detectedChips.first(); ui->comboChip->setCurrentText(firstChip); ui->comboEepromChip->setCurrentText(firstChip);
                State lastState = currentState; detectedChips.clear(); accumulatedError.clear(); accumulatedOutput.clear();
                QTimer::singleShot(300, this, [this, lastState, firstChip]() {
                    QStringList args = {"flashrom", "-p", getProgrammerArgs(), "-c", firstChip};
                    if (lastState == State::Detecting) {} else if (lastState == State::Reading) args << "-r" << currentFile; else if (lastState == State::Writing) args << "-w" << currentFile; else if (lastState == State::Erasing) args << "-E"; else if (lastState == State::SmartRead) args << "-r" << getWorkPath("readx.bin"); else return;
                    currentState = lastState; runCommand("pkexec", args);
                });
                return;
            }
        }
        if (currentState == State::SmartRead) { 
            log(tr("Smart Merge: Step 1/3"), "cyan"); 
            ui->statusbar->showMessage(tr("Smart Merge: Step 1/3 (Reading)"));
            QString targetPath = getWorkPath("readx.bin"); 
            QFile::remove(targetPath); 
            runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-r", targetPath}); 
            currentState = State::Reading; 
            return; 
        }
        if (!accumulatedOutput.contains("SUCCESS")) {
            log(tr("Failed"), "red");
            ui->statusbar->showMessage(tr("Operation Failed"), 5000);
        }
    } else {
        if (currentState == State::LocalDetect) {
            QRegularExpression re("SUCCESS: ([0-9A-F]+) ([0-9A-F]+) ([0-9A-F]+)");
            QRegularExpressionMatch match = re.match(accumulatedOutput);
            if (match.hasMatch()) {
                QString info = QString("ID: %1 %2 %3").arg(match.captured(1), match.captured(2), match.captured(3));
                ui->labelLocalChip->setText(info);
                log(tr("Detected Local Chip: %1").arg(info), "green");
                uint8_t c = match.captured(3).toInt(nullptr, 16);
                if (c >= 0x13 && c <= 0x21) { localSpi->setFlashSize(1 << c); log(tr("Flash Size: %1 MB").arg(localSpi->getFlashSize() / 1024 / 1024)); }
            }
        }
        else if (currentState == State::LocalRead) {
            QString tempFile = getWorkPath("flash_read.bin");
            QFile f(tempFile); if (f.open(QIODevice::ReadOnly)) { loadDataToEditor(f.readAll()); f.close(); log(tr("Local read finished."), "green"); }
        }
        else if (currentState == State::LocalWrite) { log(tr("Local write successfully completed!"), "green"); }
        else if (currentState == State::Detecting) {
            QRegularExpression re("Found [^ ]+ flash chip \"([^\"]+)\"");
            QRegularExpressionMatch match = re.match(accumulatedError);
            if (match.hasMatch()) {
                QString chipName = match.captured(1);
                ui->comboChip->setCurrentText(chipName);
                ui->comboEepromChip->setCurrentText(chipName);
                log(tr("Detected chip updated to preview: %1").arg(chipName), "cyan");
            }
        }

        if (currentState == State::Reading && QFile::exists(getWorkPath("readx.bin"))) {
            log(tr("Step 2/3: Merging"), "cyan");
            ui->statusbar->showMessage(tr("Smart Merge: Step 2/3 (Merging)"));
            QFile flashFile(getWorkPath("readx.bin")); QFile newFile(currentFile); QFile outFile(getWorkPath("tempx.bin"));
            if (flashFile.open(QIODevice::ReadOnly) && newFile.open(QIODevice::ReadOnly) && outFile.open(QIODevice::WriteOnly)) {
                outFile.write(newFile.readAll()); flashFile.seek(lastInfo.fileSize); outFile.write(flashFile.readAll()); flashFile.close(); newFile.close(); outFile.close();
                QFile layout(getWorkPath("flashrom.layout")); if (layout.open(QIODevice::WriteOnly)) { layout.write(QString("00000000:%1 flashzone").arg(lastInfo.fileSize - 1, 8, 16, QChar('0')).toUtf8()); layout.close(); currentState = State::SmartWrite; runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-l", getWorkPath("flashrom.layout"), "-i", "flashzone", "-w", getWorkPath("tempx.bin")}); return; }
            }
        } else if (currentState == State::SmartWrite) { 
            log(tr("Successful!"), "green"); 
            ui->statusbar->showMessage(tr("Smart Merge Successful"), 5000);
            QFile::remove(getWorkPath("readx.bin")); QFile::remove(getWorkPath("tempx.bin")); QFile::remove(getWorkPath("flashrom.layout")); 
        } else { 
            log(tr("Successful"), "green"); 
            ui->statusbar->showMessage(tr("Operation Successful"), 5000);
            if (currentState == State::Reading || currentState == State::EepromRead) { 
                QFile f((currentState == State::EepromRead) ? eepromFile : currentFile); 
                if (f.open(QIODevice::ReadOnly)) { loadDataToEditor(f.readAll()); f.close(); } 
            } 
        }
    }
    currentState = State::Idle; 
    ui->progressBar->hide();
    if (idleTimer) idleTimer->start(5000);
}

void MainWindow::loadDataToEditor(const QByteArray &data) { ui->hexEditor->setData(data); }
QString MainWindow::prepareWriteFile() {
    QByteArray data = ui->hexEditor->data();
    if (data.isEmpty()) return QString();
    QString tempPath = getWorkPath("flash_buffer.bin");
    QFile f(tempPath); if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); return tempPath; }
    return QString();
}
void MainWindow::on_btnSaveFile_clicked() {
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save Binary File"), currentFile, tr("Binary (*.bin *.fd);;All (*.*)"));
    if (savePath.isEmpty()) return;
    QFile f(savePath); if (f.open(QIODevice::WriteOnly)) { f.write(ui->hexEditor->data()); f.close(); log(tr("File saved: %1").arg(savePath), "green"); }
}
void MainWindow::on_btnBrowse_clicked() { 
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open BIOS"), "", tr("Binary (*.bin *.fd);;All (*.*)")); 
    if (!fileName.isEmpty()) { ui->lineFile->setText(fileName); currentFile = fileName; QFile f(fileName); if (f.open(QIODevice::ReadOnly)) { loadDataToEditor(f.readAll()); f.close(); } } 
}
void MainWindow::log(const QString &msg, const QString &color) { ui->textLog->append(QString("<font color=\"%1\">%2</font>").arg(color, msg.toHtmlEscaped())); ui->textLog->ensureCursorVisible(); }
QString MainWindow::getProgrammerArgs(bool isEeprom) {
    if (isEeprom) return ui->comboEepromProg->currentData().toString();
    QString base = ui->comboProgrammer->currentData().toString(); QString speed = ui->comboSpeed->currentData().toString();
    if (base.contains("serprog")) { QString dev = "/dev/ttyACM0"; if (!QFileInfo("/dev/ttyACM0").exists()) dev = "/dev/ttyUSB0"; base = "serprog:dev=" + dev + ":4000000"; if (!speed.isEmpty()) base += ",spispeed=" + speed; }
    else if (base.contains("linux_spi")) { base = "linux_spi:dev=/dev/spidev1.0"; if (!speed.isEmpty()) base += ",spispeed=" + speed; }
    return base;
}

void MainWindow::on_comboLang_currentIndexChanged(int index) {
    QString lang = ui->comboLang->itemData(index).toString();
    qApp->removeTranslator(translator);
    bool loaded = false;
    if (lang == "zh_CN") loaded = translator->load(":/i18n/FlashHelper_zh_CN.qm");
    if (loaded) qApp->installTranslator(translator);
    ui->retranslateUi(this);
    if (previewGroup) previewGroup->setTitle(tr("Chip Pinout Preview"));
    ui->statusbar->showMessage(tr("Idle"));
    updateSystemStatus();
    fetchSupportedChips();
}

void MainWindow::on_btnLocalBrowse_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open BIOS File"), "", tr("Binary Files (*.bin *.rom);;All Files (*)"));
    if (!fileName.isEmpty()) { localFile = fileName; ui->editLocalFile->setText(localFile); log(tr("Selected local file: %1").arg(localFile)); }
}
void MainWindow::on_btnLocalDetect_clicked() { currentState = State::LocalDetect; if (!runLocalHelper({"detect"})) currentState = State::Idle; }
void MainWindow::on_btnLocalRead_clicked() { 
    currentState = State::LocalRead; 
    ui->progressBar->setRange(0, 0); // Indeterminate
    ui->progressBar->show();
    ui->statusbar->showMessage(tr("Starting local read (%1 MB)...").arg(localSpi->getFlashSize() / 1024 / 1024));
    if (!runLocalHelper({"read", getWorkPath("flash_read.bin"), QString::number(localSpi->getFlashSize())})) {
        currentState = State::Idle;
        ui->progressBar->hide();
    }
}
void MainWindow::on_btnLocalWrite_clicked() { 
    if (localFile.isEmpty()) { QMessageBox::warning(this, tr("Error"), tr("Please select a file first.")); return; }
    currentState = State::LocalWrite; 
    ui->progressBar->setRange(0, 0); // Indeterminate
    ui->progressBar->show();
    ui->statusbar->showMessage(tr("Starting local erase & write..."));
    if (!runLocalHelper({"write", localFile})) {
        currentState = State::Idle;
        ui->progressBar->hide();
    }
}

void MainWindow::showLogContextMenu(const QPoint &pos) {
    QMenu menu(this);
    QAction *clearAction = menu.addAction(tr("Clear Log"));
    connect(clearAction, &QAction::triggered, ui->textLog, &QTextEdit::clear);
    menu.exec(ui->textLog->mapToGlobal(pos));
}
void MainWindow::on_comboProgrammer_currentIndexChanged(int) {}
