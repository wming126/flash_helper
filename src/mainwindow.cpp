#include "mainwindow.h"
#include "ui_mainwindow.h"
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/flashhelper.svg"));
    process = new QProcess(this);
    
    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::readProcessOutput);
    connect(process, &QProcess::readyReadStandardError, this, &MainWindow::readProcessOutput);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, &MainWindow::processFinished);

    // SPI Programmers
    ui->comboProgrammer->addItem(tr("Serprog (STM32)"), "serprog:dev=/dev/ttyACM0:4000000");
    ui->comboProgrammer->addItem(tr("CH341A SPI"), "ch341a_spi");
    ui->comboProgrammer->addItem(tr("FT2232 SPI"), "ft2232_spi");
    ui->comboProgrammer->addItem(tr("Bus Pirate SPI"), "buspirate_spi:dev=/dev/ttyUSB0");
    ui->comboProgrammer->addItem(tr("Dediprog"), "dediprog");
    ui->comboProgrammer->addItem(tr("STLINK-V3 SPI"), "stlinkv3_spi");
    ui->comboProgrammer->addItem(tr("PICkit 2 SPI"), "pickit2_spi");
    ui->comboProgrammer->addItem(tr("DirtyJTAG SPI"), "dirtyjtag_spi");
    ui->comboProgrammer->addItem(tr("Linux SPI"), "linux_spi:dev=/dev/spidev1.0");
    ui->comboProgrammer->addItem(tr("Linux MTD"), "linux_mtd");
    ui->comboProgrammer->addItem(tr("Dummy (Test only)"), "dummy");

    // LoongArch Specific
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
    ui->comboSpeed->addItem("512 kHz", "512000");

    // EEPROM Programmers
    ui->comboEepromProg->addItem(tr("CH341A SPI (I2C Patched)"), "ch341a_spi");
    ui->comboEepromProg->addItem(tr("Bus Pirate"), "buspirate_spi");
    ui->comboEepromProg->addItem(tr("Serprog"), "serprog");
    ui->comboEepromProg->addItem(tr("Linux SPI"), "linux_spi");

    ui->textLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->textLog, &QWidget::customContextMenuRequested, this, &MainWindow::showLogContextMenu);

    ui->splitterMain->setSizes({150, 450, 150});
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    updateSystemStatus();
    fetchSupportedChips();
    
    ui->statusbar->showMessage(tr("Idle"));
    on_comboProgrammer_currentIndexChanged(ui->comboProgrammer->currentIndex());

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        QString fileToLoad = (index == 1) ? eepromFile : currentFile;
        if (!fileToLoad.isEmpty() && QFile::exists(fileToLoad)) {
            QFile f(fileToLoad);
            if (f.open(QIODevice::ReadOnly)) { loadDataToEditor(f.readAll()); f.close(); }
        } else {
            loadDataToEditor(QByteArray());
        }
    });
}

MainWindow::~MainWindow() { delete ui; }

QString MainWindow::getFlashromPath() {
    // 强制只获取应用程序同级目录下的 flashrom
    // 在 AppImage 中，这指向挂载点内的 usr/bin/flashrom
    QString localPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("flashrom");
    
    if (QFile::exists(localPath)) {
        return localPath;
    }
    
    // 如果找不到内置的，不再尝试系统路径，直接记录错误
    // 这样可以确保工具的纯粹性，避免版本冲突
    qDebug() << "CRITICAL: Bundled flashrom not found at:" << localPath;
    return "flashrom"; // 这里的返回仅作为最后的保险，但在 AppImage 中 localPath 应该总是存在的
}

void MainWindow::fetchSupportedChips() {
    QString flashromPath = getFlashromPath();
    QProcess *listProc = new QProcess(this);
    connect(listProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, listProc]() {
        QString output = listProc->readAllStandardOutput();
        QStringList lines = output.split('\n');
        supportedChips.clear();
        supportedChips << tr("Auto Detect");
        
        bool start = false;
        for (const QString &line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.contains("Vendor") && trimmed.contains("Device")) { start = true; continue; }
            if (trimmed.startsWith("==") || trimmed.startsWith("--") || trimmed.startsWith("(")) continue;
            
            if (start && !trimmed.isEmpty()) {
                QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    if (parts[0] == "Vendor" || parts[0] == "Known" || parts[0] == "OK") continue;
                    QString vendor = parts[0];
                    QString chipModel = parts[1];
                    supportedChips << chipModel;
                    supportedChips << QString("%1 %2").arg(vendor, chipModel);
                }
            }
        }
        supportedChips.removeDuplicates();
        QString autoStr = supportedChips.takeFirst();
        supportedChips.sort();
        supportedChips.prepend(autoStr);
        
        QCompleter *completer = new QCompleter(supportedChips, this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        
        ui->comboChip->clear();
        ui->comboChip->addItems(supportedChips);
        ui->comboChip->setCompleter(completer);
        ui->comboChip->setCurrentIndex(0);

        ui->comboEepromChip->clear();
        ui->comboEepromChip->addItems(supportedChips);
        ui->comboEepromChip->setCompleter(completer);
        ui->comboEepromChip->setCurrentIndex(0);
        
        listProc->deleteLater();
    });
    listProc->start(flashromPath, {"-L"});
}

void MainWindow::on_comboProgrammer_currentIndexChanged(int index) {
    QString data = ui->comboProgrammer->itemData(index).toString();
    bool supportsSpeed = data.contains("serprog") || data.contains("linux_spi");
    ui->comboSpeed->setEnabled(supportsSpeed);
    if (!supportsSpeed) ui->comboSpeed->setCurrentIndex(0);
}

void MainWindow::showLogContextMenu(const QPoint &pos) {
    QMenu menu(this);
    QAction *clearAction = menu.addAction(tr("Clear Log"));
    connect(clearAction, &QAction::triggered, ui->textLog, &QTextEdit::clear);
    menu.exec(ui->textLog->mapToGlobal(pos));
}

QString getWorkPath(const QString &name) {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + name;
}

void MainWindow::updateSystemStatus() {
    QString flashromPath = getFlashromPath();
    QProcess check;
    check.start(flashromPath, {"-p", getProgrammerArgs()});
    check.waitForFinished(1000);
    QString out = check.readAllStandardError() + check.readAllStandardOutput();

    bool permissionDenied = out.contains("Permission denied") || out.contains("Access denied");

    if (permissionDenied) {
        QString args = getProgrammerArgs();
        if (args.contains("serprog")) {
            if (QFileInfo("/dev/ttyACM0").isWritable() || QFileInfo("/dev/ttyUSB0").isWritable()) {
                permissionDenied = false;
            }
        }
    }

    bool udevFile = QFile::exists("/etc/udev/rules.d/z60_flashrom.rules");
    ui->lblUdevStatus->setText(permissionDenied ? tr("Hardware Access: <font color='red'>Denied (Requires Root)</font>") : tr("Hardware Access: <font color='green'>OK (Direct Access)</font>"));
    ui->lblPolkitStatus->setText(udevFile ? tr("System Config: <font color='green'>Rules Installed</font>") : tr("System Config: <font color='red'>Not Configured</font>"));
    ui->btnRemoveRules->setEnabled(udevFile);
}

void MainWindow::on_btnInstallRules_clicked() {
    QString udevContent = "# Flashrom Programmers\nKERNEL==\"ttyACM*\", TAG+=\"uaccess\"\nKERNEL==\"ttyUSB*\", TAG+=\"uaccess\"\nKERNEL==\"spidev*\", TAG+=\"uaccess\"\n";
    QString polkitRule = "polkit.addRule(function(action, subject) {\n    if (action.id == \"org.freedesktop.policykit.exec\" && subject.isInGroup(\"sudo\")) { return polkit.Result.YES; } });";

    QString script = QString("echo '%1' | base64 -d > /etc/udev/rules.d/z60_flashrom.rules && "
                             "echo '%2' | base64 -d > /etc/polkit-1/rules.d/10-flashrom.rules && "
                             "udevadm control --reload-rules && udevadm trigger && "
                             "DEV_GROUP=$(stat -c '%%G' /dev/ttyACM0 2>/dev/null || stat -c '%%G' /dev/ttyUSB0 2>/dev/null); "
                             "if [ -n \"$DEV_GROUP\" ] && [ \"$DEV_GROUP\" != \"root\" ]; then usermod -aG $DEV_GROUP %3; fi && "
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
    QStringList finalArgs = args;
    QString finalCmd = cmd;
    accumulatedError.clear();
    
    // 探测当前使用的绝对路径
    QString flashromPath = getFlashromPath();

    // 自动注入 -c 参数
    QString currentChip = ui->comboChip->currentText();
    if (!currentChip.isEmpty() && currentChip != tr("Auto Detect") && !finalArgs.contains("-c")) {
        int pIdx = finalArgs.indexOf("-p");
        if (pIdx != -1 && pIdx + 1 < finalArgs.size()) {
            finalArgs.insert(pIdx + 2, "-c");
            finalArgs.insert(pIdx + 3, currentChip);
        }
    }

    if (cmd == "pkexec" && !args.isEmpty() && args[0] == "flashrom") {
        QString progArgs = getProgrammerArgs();
        bool forceDirect = false;
        if (progArgs.contains("serprog") && (QFileInfo("/dev/ttyACM0").isWritable() || ui->lblUdevStatus->text().contains("OK"))) {
            forceDirect = true;
        }

        if (forceDirect || ui->lblUdevStatus->text().contains("OK")) {
            finalCmd = flashromPath;
            finalArgs.removeFirst();
            log(tr("[Direct Access]"), "gray");
        } else {
            // 核心修复：如果 flashrom 在 AppImage 挂载点内，pkexec 无法访问，复制到 /tmp
            if (flashromPath.contains("/.mount_")) {
                QString tempFlashrom = QDir::tempPath() + "/flashrom_internal_" + qgetenv("USER");
                if (!QFile::exists(tempFlashrom) || QFile::remove(tempFlashrom)) {
                    if (QFile::copy(flashromPath, tempFlashrom)) {
                        QFile::setPermissions(tempFlashrom, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | 
                                                      QFile::ReadGroup | QFile::ExeGroup | 
                                                      QFile::ReadOther | QFile::ExeOther);
                        flashromPath = tempFlashrom;
                        log(tr("Prepared temp flashrom for pkexec: %1").arg(tempFlashrom), "gray");
                    } else {
                        log(tr("ERROR: Failed to copy flashrom to %1").arg(tempFlashrom), "red");
                    }
                } else {
                    log(tr("ERROR: Failed to remove old temp flashrom at %1").arg(tempFlashrom), "red");
                }
            }
            finalArgs[0] = flashromPath;
        }
    }
    
    // 强制打印诊断信息
    log(tr("Flashrom Path: %1").arg(flashromPath), "gray");
    log(tr("Final Command: %1 %2").arg(finalCmd, finalArgs.join(" ")), "gray");
    
    process->start(finalCmd, finalArgs);
}

void MainWindow::on_btnDetect_clicked() { currentState = State::Detecting; runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs()}); }
void MainWindow::on_btnRead_clicked() {
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save BIOS"), "backup.bin", tr("Binary (*.bin *.fd);;All (*.*)"));
    if (savePath.isEmpty()) return;
    currentFile = savePath; currentState = State::Reading;
    runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-r", savePath});
}
void MainWindow::on_btnErase_clicked() { if (QMessageBox::question(this, tr("Confirm"), tr("ERASE?")) == QMessageBox::Yes) { currentState = State::Erasing; runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-E"}); } }
void MainWindow::on_btnWrite_clicked() {
    QString targetFile = prepareWriteFile();
    if (targetFile.isEmpty()) return;
    currentState = State::Writing; runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-w", targetFile});
}

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
    if (QMessageBox::question(this, tr("Confirm"), tr("ERASE?")) == QMessageBox::Yes) { currentState = State::EepromErase; QStringList args = {"flashrom", "-p", getProgrammerArgs(true), "-E"}; if (chip != tr("Auto Detect")) args << "-c" << chip; runCommand("pkexec", args); }
}

void MainWindow::readProcessOutput() {
    QString output = process->readAllStandardOutput(); QString error = process->readAllStandardError();
    if (!output.isEmpty()) { log(output, "white"); accumulatedError += output; }
    if (!error.isEmpty()) { log(error, "yellow"); accumulatedError += error; }
    if (currentState == State::Writing && (error.contains("expected size") || error.contains("doesn't match"))) handleSmartWrite(error);
}

void MainWindow::handleSmartWrite(const QString &error) {
    QRegularExpression re("(?:file|Image) size \\((\\d+) ?B?\\).*?(?:flash chip's|expected) size \\((\\d+) ?B?\\)");
    QRegularExpressionMatch match = re.match(error);
    if (match.hasMatch()) { lastInfo.fileSize = match.captured(1).toLong(); lastInfo.flashSize = match.captured(2).toLong(); if (lastInfo.fileSize < lastInfo.flashSize) { log(tr("Size mismatch. Reading..."), "cyan"); currentState = State::SmartRead; } }
}

void MainWindow::processFinished(int exitCode) {
    if (exitCode != 0) {
        if (accumulatedError.contains("Multiple flash chip definitions match") || accumulatedError.contains("Please specify which chip definition")) {
            detectedChips.clear(); QRegularExpression re("\"([^\"]+)\""); QRegularExpressionMatchIterator i = re.globalMatch(accumulatedError);
            QStringList blackList = {"serprog", "flashrom", "mapping", "stm32", "protocol"};
            while (i.hasNext()) { QString name = i.next().captured(1); bool black = false; for (const QString &k : blackList) { if (name.contains(k, Qt::CaseInsensitive)) black = true; } if (!black && name.length() > 3) detectedChips << name; }
            if (!detectedChips.isEmpty()) {
                QString firstChip = detectedChips.first(); ui->comboChip->setCurrentText(firstChip); ui->comboEepromChip->setCurrentText(firstChip);
                State lastState = currentState; detectedChips.clear(); accumulatedError.clear();
                QTimer::singleShot(300, this, [this, lastState, firstChip]() {
                    QStringList args = {"flashrom", "-p", getProgrammerArgs(), "-c", firstChip};
                    if (lastState == State::Detecting) {} else if (lastState == State::Reading) args << "-r" << currentFile; else if (lastState == State::Writing) args << "-w" << currentFile; else if (lastState == State::Erasing) args << "-E"; else if (lastState == State::SmartRead) args << "-r" << getWorkPath("readx.bin"); else return;
                    currentState = lastState; runCommand("pkexec", args);
                });
                return;
            }
        }
        if (currentState == State::SmartRead) { log(tr("Smart Merge: Step 1/3"), "cyan"); QString targetPath = getWorkPath("readx.bin"); QFile::remove(targetPath); runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-r", targetPath}); currentState = State::Reading; return; }
        log(tr("Failed"), "red");
    } else {
        if (currentState == State::Reading && QFile::exists(getWorkPath("readx.bin"))) {
            log(tr("Step 2/3: Merging"), "cyan");
            QFile flashFile(getWorkPath("readx.bin")); QFile newFile(currentFile); QFile outFile(getWorkPath("tempx.bin"));
            if (flashFile.open(QIODevice::ReadOnly) && newFile.open(QIODevice::ReadOnly) && outFile.open(QIODevice::WriteOnly)) {
                outFile.write(newFile.readAll()); flashFile.seek(lastInfo.fileSize); outFile.write(flashFile.readAll()); flashFile.close(); newFile.close(); outFile.close();
                QFile layout(getWorkPath("flashrom.layout")); if (layout.open(QIODevice::WriteOnly)) { layout.write(QString("00000000:%1 flashzone").arg(lastInfo.fileSize - 1, 8, 16, QChar('0')).toUtf8()); layout.close(); currentState = State::SmartWrite; runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-l", getWorkPath("flashrom.layout"), "-i", "flashzone", "-w", getWorkPath("tempx.bin")}); return; }
            }
        } else if (currentState == State::SmartWrite) { log(tr("Successful!"), "green"); QFile::remove(getWorkPath("readx.bin")); QFile::remove(getWorkPath("tempx.bin")); QFile::remove(getWorkPath("flashrom.layout")); }
        else { log(tr("Successful"), "green"); if (currentState == State::Reading || currentState == State::EepromRead) { QFile f((currentState == State::EepromRead) ? eepromFile : currentFile); if (f.open(QIODevice::ReadOnly)) { loadDataToEditor(f.readAll()); f.close(); } } }
    }
    currentState = State::Idle; QTimer::singleShot(5000, this, [this]() { ui->statusbar->showMessage(tr("Idle")); });
}

void MainWindow::loadDataToEditor(const QByteArray &data) { ui->hexEditor->setData(data); }
QString MainWindow::prepareWriteFile() { return (ui->tabWidget->currentIndex() == 1) ? eepromFile : currentFile; }
void MainWindow::on_btnSaveFile_clicked() { log(tr("Viewer mode only."), "yellow"); }
void MainWindow::on_btnBrowse_clicked() { QString fileName = QFileDialog::getOpenFileName(this, tr("Open BIOS"), "", tr("Binary (*.bin *.fd);;All (*.*)")); if (!fileName.isEmpty()) { ui->lineFile->setText(fileName); currentFile = fileName; QFile f(fileName); if (f.open(QIODevice::ReadOnly)) { loadDataToEditor(f.readAll()); f.close(); } } }
void MainWindow::log(const QString &msg, const QString &color) { ui->textLog->append(QString("<font color=\"%1\">%2</font>").arg(color, msg.toHtmlEscaped())); }
QString MainWindow::getProgrammerArgs(bool isEeprom) {
    if (isEeprom) return ui->comboEepromProg->currentData().toString();
    QString base = ui->comboProgrammer->currentData().toString(); QString speed = ui->comboSpeed->currentData().toString();
    if (base.contains("serprog")) { QString dev = "/dev/ttyACM0"; if (!QFile::exists(dev)) dev = "/dev/ttyUSB0"; base = "serprog:dev=" + dev + ":4000000"; if (!speed.isEmpty()) base += ",spispeed=" + speed; }
    else if (base.contains("linux_spi")) { base = "linux_spi:dev=/dev/spidev1.0"; if (!speed.isEmpty()) base += ",spispeed=" + speed; }
    return base;
}
