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
    
    // 强制设置默认值为 Serprog (STM32)，它是列表第一个
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
    ui->comboSpeed->addItem("256 kHz", "256000");
    ui->comboSpeed->addItem("128 kHz", "128000");

    // EEPROM Programmers
    ui->comboEepromProg->addItem(tr("CH341A SPI (I2C Patched)"), "ch341a_spi");
    ui->comboEepromProg->addItem(tr("Bus Pirate"), "buspirate_spi");
    ui->comboEepromProg->addItem(tr("Serprog"), "serprog");
    ui->comboEepromProg->addItem(tr("Linux SPI"), "linux_spi");

    ui->textLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->textLog, &QWidget::customContextMenuRequested, this, &MainWindow::showLogContextMenu);

    // QSplitter 初始比例 (让控制区更紧凑)
    ui->splitterMain->setSizes({150, 450, 150});

    // 预创建数据目录
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    updateSystemStatus();
    fetchSupportedChips();
    
    ui->statusbar->showMessage(tr("Idle"));
    on_comboProgrammer_currentIndexChanged(ui->comboProgrammer->currentIndex());

    // Tab 切换联动编辑器
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

void MainWindow::fetchSupportedChips() {
    QString flashromPath = QCoreApplication::applicationDirPath() + "/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = "/usr/sbin/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = "/usr/bin/flashrom";
    
    QProcess *listProc = new QProcess(this);
    connect(listProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, listProc]() {
        QString output = listProc->readAllStandardOutput();
        QStringList lines = output.split('\n');
        supportedChips.clear();
        supportedChips << tr("Auto Detect"); // 添加自动探测选项
        
        bool start = false;
        for (const QString &line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.contains("Vendor") && trimmed.contains("Device")) { start = true; continue; }
            if (trimmed.startsWith("==") || trimmed.startsWith("--") || trimmed.startsWith("(")) continue;
            
            if (start && !trimmed.isEmpty()) {
                QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    // 排除表头和杂质
                    if (parts[0] == "Vendor" || parts[0] == "Known" || parts[0] == "OK") continue;
                    
                    QString chipModel = parts[1];
                    QString fullName = QString("%1 %2").arg(parts[0], parts[1]);
                    supportedChips << chipModel;
                    supportedChips << fullName;
                }
            }
        }
        supportedChips.removeDuplicates();
        // 排序逻辑：Auto Detect 始终在最前面，其余按字母排序
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
    QString flashromPath = QCoreApplication::applicationDirPath() + "/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = "/usr/sbin/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = "/usr/bin/flashrom";

    QProcess check;
    check.start(flashromPath, {"-p", getProgrammerArgs()});
    check.waitForFinished(1000);
    QString out = check.readAllStandardError() + check.readAllStandardOutput();

    bool permissionDenied = out.contains("Permission denied") || out.contains("Access denied");

    // 增强检测：在 LoongArch 下直接检查串口设备是否可写
    if (permissionDenied) {
        QString args = getProgrammerArgs();
        if (args.contains("serprog")) {
            if (QFileInfo("/dev/ttyACM0").isWritable() || QFileInfo("/dev/ttyUSB0").isWritable()) {
                permissionDenied = false; // 如果设备文件可写，即便 flashrom 报错也认为权限 OK
            }
        }
    }

    bool udevFile = QFile::exists("/etc/udev/rules.d/z60_flashrom.rules");
    ui->lblUdevStatus->setText(permissionDenied ? tr("Hardware Access: <font color='red'>Denied (Requires Root)</font>") : tr("Hardware Access: <font color='green'>OK (Direct Access)</font>"));
    ui->lblPolkitStatus->setText(udevFile ? tr("System Config: <font color='green'>Rules Installed</font>") : tr("System Config: <font color='red'>Not Configured</font>"));
    ui->btnRemoveRules->setEnabled(udevFile);
}

void MainWindow::on_btnInstallRules_clicked() {
    QString udevContent = "# Flashrom Programmers\n"
                          "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"0483\", ATTRS{idProduct}==\"5740\", TAG+=\"uaccess\"\n"
                          "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"1a86\", ATTRS{idProduct}==\"5512\", TAG+=\"uaccess\"\n"
                          "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"0403\", ATTRS{idProduct}==\"6010\", TAG+=\"uaccess\"\n"
                          "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"0483\", ATTRS{idProduct}==\"dada\", TAG+=\"uaccess\"\n"
                          "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"0483\", ATTRS{idProduct}==\"374e\", TAG+=\"uaccess\"\n"
                          "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"0483\", ATTRS{idProduct}==\"374f\", TAG+=\"uaccess\"\n"
                          "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"04d8\", ATTRS{idProduct}==\"0033\", TAG+=\"uaccess\"\n"
                          "KERNEL==\"ttyACM*\", TAG+=\"uaccess\"\n"
                          "KERNEL==\"ttyUSB*\", TAG+=\"uaccess\"\n"
                          "KERNEL==\"spidev*\", TAG+=\"uaccess\"\n";

    QString polkitRule = "polkit.addRule(function(action, subject) {\n"
                         "    if (action.id == \"org.freedesktop.policykit.exec\" &&\n"
                         "        (action.lookup(\"program\") == \"/usr/sbin/flashrom\" || action.lookup(\"program\") == \"/usr/bin/flashrom\") && \n"
                         "        subject.isInGroup(\"sudo\")) { return polkit.Result.YES; } });";

    QString script = QString("echo '%1' | base64 -d > /etc/udev/rules.d/z60_flashrom.rules && "
                             "echo '%2' | base64 -d > /etc/polkit-1/rules.d/10-flashrom.rules && "
                             "chmod 644 /etc/udev/rules.d/z60_flashrom.rules && "
                             "chmod 644 /etc/polkit-1/rules.d/10-flashrom.rules && "
                             "udevadm control --reload-rules && udevadm trigger && "
                             "echo 'Checking device group...' && "
                             "DEV_GROUP=$(stat -c '%%G' /dev/ttyACM0 2>/dev/null || stat -c '%%G' /dev/ttyUSB0 2>/dev/null); "
                             "if [ -n \"$DEV_GROUP\" ] && [ \"$DEV_GROUP\" != \"root\" ]; then "
                             "  echo \"Adding user to detected group: $DEV_GROUP\"; "
                             "  usermod -aG $DEV_GROUP %3; "
                             "else "
                             "  echo 'No specific serial group detected, adding to dialout by default'; "
                             "  usermod -aG dialout %3; "
                             "fi && "
                             "usermod -aG plugdev %3 && echo 'All permissions set.'")
                     .arg(QString(udevContent.toUtf8().toBase64()), QString(polkitRule.toUtf8().toBase64()), qgetenv("USER"));

    if (QProcess::execute("pkexec", {"bash", "-c", script}) == 0) {
        QMessageBox::information(this, tr("Success"), tr("Rules installed successfully!\n\nNOTE: You may need to RE-LOGIN for group permissions to take effect."));
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
    
    // 优先级：程序同目录 -> /usr/sbin -> /usr/bin
    QString flashromPath = QCoreApplication::applicationDirPath() + "/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = "/usr/sbin/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = "/usr/bin/flashrom";

    // 核心改进：只要 UI 中选定了特定芯片，所有命令自动带上 -c 参数，防止中途报错
    QString currentChip = ui->comboChip->currentText();
    if (!currentChip.isEmpty() && currentChip != tr("Auto Detect") && !finalArgs.contains("-c")) {
        // 在 -p 参数后面插入 -c
        int pIdx = finalArgs.indexOf("-p");
        if (pIdx != -1 && pIdx + 1 < finalArgs.size()) {
            finalArgs.insert(pIdx + 2, "-c");
            finalArgs.insert(pIdx + 3, currentChip);
        }
    }

    if (cmd == "pkexec" && !args.isEmpty() && args[0] == "flashrom") {
        QString progArgs = getProgrammerArgs();
        bool isSerial = progArgs.contains("serprog") || progArgs.contains("buspirate") || progArgs.contains("ch341a");
        
        // 核心改进：如果是串口或 USB 编程器，且设备文件可写，强制跳过 pkexec
        bool forceDirect = false;
        if (isSerial) {
            if (QFileInfo("/dev/ttyACM0").isWritable() || QFileInfo("/dev/ttyUSB0").isWritable() || 
                ui->lblUdevStatus->text().contains("OK")) {
                forceDirect = true;
            }
        }

        if (forceDirect || ui->lblUdevStatus->text().contains("OK")) {
            finalCmd = flashromPath;
            finalArgs.removeFirst();
            ui->textLog->append(tr("<font color='gray'>[Direct Hardware Access Enabled]</font>"));
        } else {
            finalArgs[0] = flashromPath;
        }
    }
    
    ui->textLog->append(tr("<b>Running: %1 %2</b>").arg(finalCmd, finalArgs.join(" ")));
    process->start(finalCmd, finalArgs);
}

void MainWindow::on_btnDetect_clicked() { 
    currentState = State::Detecting; 
    ui->statusbar->showMessage(tr("Detecting chip..."));
    QStringList args = {"flashrom", "-p", getProgrammerArgs()};
    QString chip = ui->comboChip->currentText();
    if (!chip.isEmpty() && chip != tr("Auto Detect")) { args << "-c" << chip; }
    runCommand("pkexec", args); 
}
void MainWindow::on_btnRead_clicked() {
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save BIOS File"), "backup.bin", tr("BIOS files (*.bin *.fd);;All files (*.*)"));
    if (savePath.isEmpty()) return;
    currentFile = savePath;
    currentState = State::Reading;
    ui->statusbar->showMessage(tr("Reading flash..."));
    QStringList args = {"flashrom", "-p", getProgrammerArgs(), "-r", savePath};
    QString chip = ui->comboChip->currentText();
    if (!chip.isEmpty() && chip != tr("Auto Detect")) { args << "-c" << chip; }
    runCommand("pkexec", args);
}
void MainWindow::on_btnErase_clicked() { 
    if (QMessageBox::question(this, tr("Confirm"), tr("ERASE flash?")) == QMessageBox::Yes) { 
        currentState = State::Erasing; 
        ui->statusbar->showMessage(tr("Erasing flash..."));
        QStringList args = {"flashrom", "-p", getProgrammerArgs(), "-E"};
        QString chip = ui->comboChip->currentText();
        if (!chip.isEmpty() && chip != tr("Auto Detect")) { args << "-c" << chip; }
        runCommand("pkexec", args); 
    } 
}
void MainWindow::on_btnWrite_clicked() {
    QString targetFile = prepareWriteFile();
    if (targetFile.isEmpty() || !QFile::exists(targetFile)) return;
    currentState = State::Writing;
    ui->statusbar->showMessage(tr("Writing flash..."));
    QStringList args = {"flashrom", "-p", getProgrammerArgs(), "-w", targetFile};
    QString chip = ui->comboChip->currentText();
    if (!chip.isEmpty() && chip != tr("Auto Detect")) { args << "-c" << chip; }
    runCommand("pkexec", args);
}

void MainWindow::on_btnEepromBrowse_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open EEPROM File"), "", tr("Binary files (*.bin *.rom *.eep *.dat);;All files (*.*)"));
    if (!fileName.isEmpty()) {
        ui->lineEepromFile->setText(fileName);
        eepromFile = fileName;
        QFile f(fileName);
        if (f.open(QIODevice::ReadOnly)) { loadDataToEditor(f.readAll()); f.close(); }
    }
}

void MainWindow::on_btnEepromRead_clicked() {
    QString chip = ui->comboEepromChip->currentText();
    if (chip.isEmpty()) { QMessageBox::warning(this, tr("Input Required"), tr("Please select or enter chip model")); return; }
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save EEPROM File"), "eeprom.bin", tr("Binary files (*.bin *.eep);;All files (*.*)"));
    if (savePath.isEmpty()) return;
    eepromFile = savePath;
    currentFile = savePath;
    currentState = State::EepromRead;
    ui->statusbar->showMessage(tr("Reading EEPROM..."));
    QStringList args = {"flashrom", "-p", getProgrammerArgs(true), "-r", savePath};
    if (chip != tr("Auto Detect")) { args << "-c" << chip; }
    runCommand("pkexec", args);
}

void MainWindow::on_btnEepromWrite_clicked() {
    QString chip = ui->comboEepromChip->currentText();
    if (chip.isEmpty()) { QMessageBox::warning(this, tr("Input Required"), tr("Please select or enter chip model")); return; }
    QString targetFile = prepareWriteFile();
    if (targetFile.isEmpty()) return;
    currentState = State::EepromWrite;
    ui->statusbar->showMessage(tr("Writing EEPROM..."));
    QStringList args = {"flashrom", "-p", getProgrammerArgs(true), "-w", targetFile};
    if (chip != tr("Auto Detect")) { args << "-c" << chip; }
    runCommand("pkexec", args);
}

void MainWindow::on_btnEepromErase_clicked() {
    QString chip = ui->comboEepromChip->currentText();
    if (chip.isEmpty()) { QMessageBox::warning(this, tr("Input Required"), tr("Please select or enter chip model")); return; }
    if (QMessageBox::question(this, tr("Confirm"), tr("ERASE EEPROM?")) == QMessageBox::Yes) {
        currentState = State::EepromErase;
        ui->statusbar->showMessage(tr("Erasing EEPROM..."));
        QStringList args = {"flashrom", "-p", getProgrammerArgs(true), "-E"};
        if (chip != tr("Auto Detect")) { args << "-c" << chip; }
        runCommand("pkexec", args);
    }
}

void MainWindow::readProcessOutput() {
    QString output = process->readAllStandardOutput();
    QString error = process->readAllStandardError();
    if (!output.isEmpty()) {
        log(output, "white");
        accumulatedError += output; // 同时累积 stdout
    }
    if (!error.isEmpty()) {
        log(error, "yellow");
        accumulatedError += error; // 同时累积 stderr
    }
    // 触发智能刷写的逻辑：文件大小不匹配
    if (currentState == State::Writing && (error.contains("expected size") || error.contains("doesn't match"))) {
        handleSmartWrite(error);
    }
}

void MainWindow::handleSmartWrite(const QString &error) {
    QRegularExpression re("(?:file|Image) size \\((\\d+) ?B?\\).*?(?:flash chip's|expected) size \\((\\d+) ?B?\\)");
    QRegularExpressionMatch match = re.match(error);
    if (match.hasMatch()) {
        lastInfo.fileSize = match.captured(1).toLong();
        lastInfo.flashSize = match.captured(2).toLong();
        if (lastInfo.fileSize < lastInfo.flashSize) {
            log(tr("Size mismatch detected. Transitions to read mode."), "cyan");
            ui->statusbar->showMessage(tr("Smart Merge: Size mismatch, preparing..."));
            currentState = State::SmartRead;
        }
    }
}

void MainWindow::processFinished(int exitCode) {
    auto restoreIdle = [this]() {
        QTimer::singleShot(5000, this, [this]() {
            if (currentState == State::Idle) { ui->statusbar->showMessage(tr("Idle")); }
        });
    };

    if (exitCode != 0) {
        // 在任务结束报错时，精准解析芯片歧义
        if (accumulatedError.contains("Multiple flash chip definitions match") || accumulatedError.contains("Please specify which chip definition")) {
            detectedChips.clear();
            // 提取所有双引号内容
            QRegularExpression re("\"([^\"]+)\"");
            QRegularExpressionMatchIterator i = re.globalMatch(accumulatedError);
            QStringList blackList = {"serprog", "flashrom", "mapping", "stm32", "protocol", "none"};
            
            while (i.hasNext()) {
                QString name = i.next().captured(1);
                bool isBlacklisted = false;
                for (const QString &key : blackList) {
                    if (name.contains(key, Qt::CaseInsensitive)) { isBlacklisted = true; break; }
                }
                if (!isBlacklisted && name.length() > 3) {
                    detectedChips << name;
                }
            }

            if (!detectedChips.isEmpty()) {
                QString firstChip = detectedChips.first();
                log(tr("Conflict Found! Auto-selecting: %1").arg(firstChip), "cyan");
                
                ui->comboChip->setCurrentText(firstChip);
                ui->comboEepromChip->setCurrentText(firstChip);
                
                State lastState = currentState;
                detectedChips.clear();
                accumulatedError.clear();
                
                // 延迟重试以确保 UI 更新，且必须严格检查重试目标
                QTimer::singleShot(300, this, [this, lastState, firstChip]() {
                    QStringList args = {"flashrom", "-p", getProgrammerArgs(), "-c", firstChip};
                    if (lastState == State::Detecting) { /* keep args */ }
                    else if (lastState == State::Reading) { args << "-r" << currentFile; }
                    else if (lastState == State::Writing) { args << "-w" << currentFile; }
                    else if (lastState == State::Erasing) { args << "-E"; }
                    else if (lastState == State::SmartRead) { 
                        // 如果是智能合并中的读取，目标必须是临时文件！
                        args << "-r" << getWorkPath("readx.bin"); 
                    }
                    else return;
                    
                    log(tr("Auto-retrying last operation with chip: %1").arg(firstChip), "cyan");
                    currentState = lastState;
                    runCommand("pkexec", args);
                });
                return;
            }
        }

        if (currentState == State::SmartRead) {
            log(tr("Smart Merge Flow: Step 1/3 - Reading current flash content..."), "cyan");
            ui->statusbar->showMessage(tr("Smart Merge: Step 1/3 - Reading..."));
            QString targetPath = getWorkPath("readx.bin");
            QFile::remove(targetPath);
            runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-r", targetPath});
            currentState = State::Reading;
            return;
        }
        log(tr("Operation Failed"), "red");
        ui->statusbar->showMessage(tr("Operation Failed"), 5000);
        currentState = State::Idle;
        restoreIdle();
        return;
    }

    if (currentState == State::Reading && QFile::exists(getWorkPath("readx.bin"))) {
        log(tr("Step 2/3: Merging files..."), "cyan");
        ui->statusbar->showMessage(tr("Smart Merge: Step 2/3 - Merging..."));
        QFile flashFile(getWorkPath("readx.bin"));
        QFile newFile(currentFile);
        QFile outFile(getWorkPath("tempx.bin"));
        if (flashFile.open(QIODevice::ReadOnly) && newFile.open(QIODevice::ReadOnly) && outFile.open(QIODevice::WriteOnly)) {
            outFile.write(newFile.readAll());
            flashFile.seek(lastInfo.fileSize);
            outFile.write(flashFile.readAll());
            flashFile.close(); newFile.close(); outFile.close();
            
            QFile layout(getWorkPath("flashrom.layout"));
            if (layout.open(QIODevice::WriteOnly)) {
                layout.write(QString("00000000:%1 flashzone").arg(lastInfo.fileSize - 1, 8, 16, QChar('0')).toUtf8());
                layout.close();
                log(tr("Step 3/3: Writing to flashzone..."), "cyan");
                ui->statusbar->showMessage(tr("Smart Merge: Step 3/3 - Writing..."));
                currentState = State::SmartWrite;
                runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-l", getWorkPath("flashrom.layout"), "-i", "flashzone", "-w", getWorkPath("tempx.bin")});
                return;
            }
        }
        log(tr("Merge failed during file processing!"), "red");
        ui->statusbar->showMessage(tr("Smart Merge Failed"), 5000);
    } else if (currentState == State::SmartWrite) {
        log(tr("Smart Write Successful!"), "green");
        ui->statusbar->showMessage(tr("Smart Write Successful!"), 5000);
        QFile::remove(getWorkPath("readx.bin")); QFile::remove(getWorkPath("tempx.bin")); QFile::remove(getWorkPath("flashrom.layout"));
    } else {
        log(tr("Operation Successful"), "green");
        ui->statusbar->showMessage(tr("Operation Successful"), 5000);
        if (currentState == State::Reading || currentState == State::EepromRead) {
            QFile f((currentState == State::EepromRead) ? eepromFile : currentFile);
            if (f.open(QIODevice::ReadOnly)) { loadDataToEditor(f.readAll()); f.close(); }
        }
    }
    currentState = State::Idle;
    restoreIdle();
}

void MainWindow::loadDataToEditor(const QByteArray &data) { ui->hexEditor->setData(data); }

QString MainWindow::prepareWriteFile() {
    return (ui->tabWidget->currentIndex() == 1) ? eepromFile : currentFile;
}

void MainWindow::on_btnSaveFile_clicked() {
    log(tr("Save functionality removed. Viewing only mode active."), "yellow");
}

void MainWindow::on_btnBrowse_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open BIOS File"), "", tr("BIOS files (*.fd *.bin *.rom);;All files (*.*)"));
    if (!fileName.isEmpty()) {
        ui->lineFile->setText(fileName);
        currentFile = fileName;
        QFile f(fileName);
        if (f.open(QIODevice::ReadOnly)) { loadDataToEditor(f.readAll()); f.close(); }
    }
}

void MainWindow::log(const QString &msg, const QString &color) { ui->textLog->append(QString("<font color=\"%1\">%2</font>").arg(color, msg.toHtmlEscaped())); }
QString MainWindow::getProgrammerArgs(bool isEeprom) {
    if (isEeprom) return ui->comboEepromProg->currentData().toString();
    QString base = ui->comboProgrammer->currentData().toString();
    QString speed = ui->comboSpeed->currentData().toString();
    
    if (base == "serprog") {
        QString dev;
        if (QFile::exists("/dev/ttyACM0")) dev = "/dev/ttyACM0";
        else if (QFile::exists("/dev/ttyUSB0")) dev = "/dev/ttyUSB0";
        
        if (!dev.isEmpty()) base += ":dev=" + dev;
        if (!speed.isEmpty()) base += ",spispeed=" + speed;
        // 如果是 STM32 下载器，建议在 serprog 后加上波特率，这里默认假设为 115200 或固件指定的速率
        // 脚本中常用的是 4000000
        if (!dev.isEmpty()) base.replace(":dev=", ":dev=" + dev + ":4000000");
    } else if (base == "linux_spi") {
        if (QFile::exists("/dev/spidev1.0")) base += ":dev=/dev/spidev1.0";
        if (!speed.isEmpty()) base += ",spispeed=" + speed;
    }
    
    return base;
}
