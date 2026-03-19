#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "chippreviewwidget.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDataStream>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QCompleter>
#include <QDebug>
#include <QGroupBox>
#include <QLabel>
#include <QAbstractItemView>
#include <QFileDevice>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    localSpi = new LocalSpiDriver();
    translator = new QTranslator(this);
    idleTimer = new QTimer(this);
    aboutVersionLabel = nullptr;
    aboutInstructionsLabel = nullptr;
    idleTimer->setSingleShot(true);
    connect(idleTimer, &QTimer::timeout, this, [this]() { ui->statusbar->showMessage(tr("Idle")); });

    ui->setupUi(this);
    setWindowIcon(QIcon(":/flashhelper.svg"));
    process = new QProcess(this);
    ui->progressBar->hide();
    ui->progressBar->setTextVisible(true);
    ui->progressBar->setFormat("%p%");
    
    previewGroup = new QGroupBox(tr("Chip Pinout Preview"), this);
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    chipPreview = new ChipPreviewWidget(this);
    previewLayout->addWidget(chipPreview);
    connect(chipPreview, &ChipPreviewWidget::typeChanged, this, [this](ChipPreviewWidget::PinoutType type) {
        if (type == ChipPreviewWidget::SPI_16PIN) {
            ui->tabWidget->setMaximumHeight(350);
            previewGroup->setMaximumHeight(300);
        } else {
            ui->tabWidget->setMaximumHeight(260);
            previewGroup->setMaximumHeight(200);
        }
    });
    
    QWidget *spiContent = new QWidget(this);
    QHBoxLayout *spiMainLayout = new QHBoxLayout(spiContent);
    QVBoxLayout *spiLeftLayout = new QVBoxLayout();
    while (ui->verticalLayout_2->count() > 0) {
        QLayoutItem *item = ui->verticalLayout_2->takeAt(0);
        if (item->layout()) spiLeftLayout->addLayout(item->layout());
        else if (item->widget()) spiLeftLayout->addWidget(item->widget());
    }
    spiMainLayout->addLayout(spiLeftLayout, 2);
    spiLeftLayout->addStretch();
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
    eepLeftLayout->addStretch();
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
        
        // Hide editor and log on "System Setup" (index 3) and "About" (index 4) tabs
        bool isSettingsOrAbout = (index == 3 || index == 4);
        ui->editorContainer->setVisible(!isSettingsOrAbout);
        ui->textLog->setVisible(!isSettingsOrAbout);

        updateTabHeight();

        // Progress bar management
        if (index != 2) ui->progressBar->hide();
    });

    // Populate About tab details
    QString version = "1.4.1";
#ifdef APP_VERSION
    version = APP_VERSION;
#endif
    const QString displayVersion = version.startsWith('v') ? version : QString("v%1").arg(version);
    setWindowTitle(QString("FlashHelper %1").arg(displayVersion));
    ui->label_title_about->setText("FlashHelper");

    aboutVersionLabel = new QLabel(displayVersion, this);
    aboutVersionLabel->setAlignment(Qt::AlignCenter);
    QFont versionFont = aboutVersionLabel->font();
    versionFont.setPointSize(12);
    versionFont.setBold(true);
    aboutVersionLabel->setFont(versionFont);
    ui->verticalLayout_about->insertWidget(2, aboutVersionLabel);
    
    aboutInstructionsLabel = new QLabel(this);
    aboutInstructionsLabel->setWordWrap(true);
    aboutInstructionsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    aboutInstructionsLabel->setTextFormat(Qt::RichText);
    ui->verticalLayout_about->insertWidget(4, aboutInstructionsLabel);

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

    initializeLanguageSelection();

    ui->textLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->textLog, &QWidget::customContextMenuRequested, this, &MainWindow::showLogContextMenu);
    
    updateSystemStatus();
    fetchSupportedChips();
    refreshDeviceList();

    // Finalize UI layout: give more space to Hex Editor
    ui->tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->textLog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->editorContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    ui->splitterMain->setStretchFactor(0, 0); // tabWidget
    ui->splitterMain->setStretchFactor(1, 10); // editorContainer - High priority
    ui->splitterMain->setStretchFactor(2, 0); // textLog
    
    // Initial layout pass
    updateTabHeight();
    
    // Use zero timer just to let the window system process events, or set immediately
    QTimer::singleShot(0, this, [this]() {
        ui->splitterMain->setSizes({240, 600, 100});
    });
}

void MainWindow::initializeLanguageSelection() {
    ui->comboLang->blockSignals(true);
    ui->comboLang->clear();
    ui->comboLang->addItem("English", "en");
    ui->comboLang->addItem("简体中文", "zh_CN");

    QSettings settings;
    QString language = settings.value("ui/language").toString();
    if (language.isEmpty()) {
        const QString locale = QLocale::system().name();
        language = locale.startsWith("zh") ? "zh_CN" : "en";
    }

    const int languageIndex = ui->comboLang->findData(language);
    ui->comboLang->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);
    ui->comboLang->blockSignals(false);
    on_comboLang_currentIndexChanged(ui->comboLang->currentIndex());
}

void MainWindow::updateDynamicTexts() {
    ui->label_title_about->setText(tr("FlashHelper"));
    if (aboutInstructionsLabel) {
        aboutInstructionsLabel->setText(tr("<h3>Tool Description</h3>"
                                           "<p>FlashHelper is a graphical front-end for flashrom, designed for BIOS and EEPROM flashing.</p>"
                                           "<h3>Usage Instructions</h3>"
                                           "<ul>"
                                           "<li><b>SPI/EEPROM:</b> Select programmer, chip model, and file, then click Read/Write.</li>"
                                           "<li><b>Local Flash:</b> Direct hardware access for Loongson platforms (requires root).</li>"
                                           "<li><b>System Setup:</b> Install udev rules to enable non-root access for USB programmers.</li>"
                                           "</ul>"));
    }
}

void MainWindow::updateTabHeight() {
    int currentIndex = ui->tabWidget->currentIndex();
    bool isSettingsOrAbout = (currentIndex == 3 || currentIndex == 4);
    
    if (isSettingsOrAbout) {
        ui->tabWidget->setMaximumHeight(16777215); 
        ui->tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    } else {
        ui->tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        ChipPreviewWidget::PinoutType type = chipPreview->currentType();
        if (type == ChipPreviewWidget::SPI_16PIN) {
            ui->tabWidget->setMaximumHeight(350);
            previewGroup->setMaximumHeight(300);
        } else {
            ui->tabWidget->setMaximumHeight(260);
            previewGroup->setMaximumHeight(200);
        }
    }
}

MainWindow::~MainWindow() {
    cleanupWorkDir();
    delete localSpi;
    delete ui;
}

void MainWindow::refreshDeviceList() {
    ui->comboDevice->clear();
    ui->comboDevice->addItem(tr("Auto"), "auto");
    QDir devDir("/dev");
    QStringList devices = devDir.entryList({"ttyACM*", "ttyUSB*", "spidev*"}, QDir::System);
    for (const QString &dev : devices) {
        ui->comboDevice->addItem("/dev/" + dev, "/dev/" + dev);
    }
}

QString MainWindow::getFlashromPath() {
    QString localPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("flashrom");
    if (QFile::exists(localPath)) return localPath;
    return "flashrom";
}

QString MainWindow::getWorkPath(const QString &fileName) {
    return QDir(ensureWorkDir()).filePath(fileName);
}

QString MainWindow::ensureWorkDir() {
    if (workDirPath.isEmpty()) {
        const QString userName = qEnvironmentVariable("USER", "user");
        const QString basePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        workDirPath = QDir(basePath).filePath(
            QString("flashhelper-%1-%2").arg(userName, QString::number(QCoreApplication::applicationPid())));
    }

    QDir().mkpath(workDirPath);
    return workDirPath;
}

void MainWindow::clearSmartWriteArtifacts() {
    QFile::remove(getWorkPath("readx.bin"));
    QFile::remove(getWorkPath("tempx.bin"));
    QFile::remove(getWorkPath("flashrom.layout"));
    smartWritePending = false;
}

void MainWindow::cleanupWorkDir() {
    if (workDirPath.isEmpty()) return;
    QDir workDir(workDirPath);
    if (workDir.exists()) workDir.removeRecursively();
    workDirPath.clear();
}

void MainWindow::maybeResetSmartWriteArtifacts() {
    if (currentState != State::SmartRead && currentState != State::SmartWrite &&
        !(currentState == State::Reading && smartWritePending)) {
        clearSmartWriteArtifacts();
    }
}

QStringList MainWindow::applySelectedChipToArgs(QStringList args) const {
    const QString currentChip = ui->comboChip->currentText();
    const bool isAuto = (currentChip == tr("Auto Detect") || currentChip.isEmpty());
    if (isAuto || currentState == State::Detecting || args.contains("-c")) return args;

    const int programmerIndex = args.indexOf("-p");
    if (programmerIndex != -1 && programmerIndex + 1 < args.size()) {
        args.insert(programmerIndex + 2, "-c");
        args.insert(programmerIndex + 3, currentChip);
    }
    return args;
}

bool MainWindow::canRunFlashromDirectly(const QString &programPath) const {
    if (!programPath.contains("flashrom")) return false;

    const QString programmerArgs = getProgrammerArgs();
    const bool hasSerialAccess = QFileInfo("/dev/ttyACM0").isWritable() || QFileInfo("/dev/ttyUSB0").isWritable();
    return programmerArgs.contains("serprog") && hasSerialAccess;
}

QString MainWindow::preparePrivilegedExecutable(QString executablePath) const {
    if (!executablePath.contains("/.mount_")) return executablePath;

    const QString binaryName = QFileInfo(executablePath).fileName();
    const QString tempPath = QDir::tempPath() + "/" + binaryName + "_internal_" + qgetenv("USER");
    if (!QFile::exists(tempPath) || QFile::remove(tempPath)) {
        if (QFile::copy(executablePath, tempPath)) {
            QFile::setPermissions(tempPath,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                  QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                  QFileDevice::ReadOther | QFileDevice::ExeOther);
            return tempPath;
        }
    } else {
        return tempPath;
    }

    return executablePath;
}

QString MainWindow::statusMessageForState(State state) const {
    switch (state) {
        case State::Detecting:
            return tr("Detecting chip...");
        case State::Reading:
            return tr("Reading flash...");
        case State::Writing:
            return tr("Writing flash...");
        case State::Erasing:
            return tr("Erasing flash...");
        case State::SmartRead:
            return tr("Smart Merge: Reading...");
        case State::SmartWrite:
            return tr("Smart Merge: Writing...");
        case State::EepromRead:
            return tr("Reading EEPROM...");
        case State::EepromWrite:
            return tr("Writing EEPROM...");
        case State::EepromErase:
            return tr("Erasing EEPROM...");
        default:
            return tr("Processing...");
    }
}

void MainWindow::updateSystemStatus() {
    bool mmioAccess = QFileInfo("/dev/mem").isReadable();
    bool usbAccess = QFileInfo("/dev/ttyACM0").isWritable() || QFileInfo("/dev/ttyUSB0").isWritable();
    
    QString status;
    if (mmioAccess || usbAccess) status = tr("Hardware Access: <font color='green'>OK (Direct Access)</font>");
    else status = tr("Hardware Access: <font color='red'>Denied (Requires Root)</font>");
    
    ui->lblUdevStatus->setText(status);
    ui->lblPolkitStatus->setText(QFile::exists("/etc/udev/rules.d/z60_flashrom.rules") ? tr("System Config: <font color='green'>Udev Rules Installed</font>") : tr("System Config: <font color='red'>Udev Rules Not Installed</font>"));
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
    const QString helperPath = getHelperPath();
    if (helperPath.isEmpty()) {
        QMessageBox::warning(this, tr("Helper Missing"), tr("flashhelper-helper was not found."));
        return;
    }

    const int exitCode = QProcess::execute("pkexec", {helperPath, "install-rules"});
    if (exitCode == 0) {
        QMessageBox::information(this, tr("Rules Installed"),
            tr("Udev access rules have been installed.\n\n"
               "Reconnect your programmer if it was already plugged in."));
    } else {
        QMessageBox::warning(this, tr("Install Failed"),
                             tr("Failed to install udev access rules. pkexec exited with code %1.").arg(exitCode));
    }
    updateSystemStatus();
}

void MainWindow::on_btnRemoveRules_clicked() {
    const QString helperPath = getHelperPath();
    if (helperPath.isEmpty()) {
        QMessageBox::warning(this, tr("Helper Missing"), tr("flashhelper-helper was not found."));
        return;
    }

    const int exitCode = QProcess::execute("pkexec", {helperPath, "remove-rules"});
    if (exitCode == 0) {
        QMessageBox::information(this, tr("Rules Removed"),
                                 tr("Udev access rules have been removed."));
    } else {
        QMessageBox::warning(this, tr("Remove Failed"),
                             tr("Failed to remove udev access rules. pkexec exited with code %1.").arg(exitCode));
    }
    updateSystemStatus();
}

void MainWindow::runCommand(const QString &cmd, const QStringList &args) {
    if (idleTimer) idleTimer->stop();
    QStringList finalArgs = applySelectedChipToArgs(args);
    QString finalCmd = cmd;
    accumulatedError.clear();
    accumulatedOutput.clear();
    maybeResetSmartWriteArtifacts();

    if (cmd == "pkexec" && !finalArgs.isEmpty()) {
        QString executablePath = finalArgs[0];
        if (executablePath == "flashrom") executablePath = getFlashromPath();
        executablePath = preparePrivilegedExecutable(executablePath);

        if (canRunFlashromDirectly(executablePath)) {
            finalCmd = executablePath;
            finalArgs.removeFirst();
            log(tr("[Direct Access]"), "gray");
        } else {
            finalArgs[0] = executablePath;
        }
    }

    log(tr("Final Command: %1 %2").arg(finalCmd, finalArgs.join(" ")), "gray");
    ui->statusbar->showMessage(statusMessageForState(currentState));
    process->start(finalCmd, finalArgs);
}

bool MainWindow::runLocalHelper(const QStringList &args) {
    QString helperPath = getHelperPath();
    if (helperPath.isEmpty()) {
        QMessageBox::warning(this, tr("Helper Missing"), tr("flashhelper-helper was not found."));
        return false;
    }

    log(tr("[Local Flash] Running: %1 %2\n").arg(helperPath, args.join(" ")), "cyan");

    accumulatedOutput.clear();
    accumulatedError.clear();
    process->start("pkexec", QStringList() << helperPath << args);
    return true;
}

QString MainWindow::getHelperPath() const {
    QString helperPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("flashhelper-helper");
    if (!QFile::exists(helperPath)) helperPath = QDir::current().absoluteFilePath("flashhelper-helper");
    if (!QFile::exists(helperPath)) helperPath = "./flashhelper-helper";
    if (!QFile::exists(helperPath)) return QString();

    if (helperPath.contains("/.mount_")) {
        const QString tempPath = QDir::tempPath() + "/flashhelper-helper_internal_" + qgetenv("USER");
        if (!QFile::exists(tempPath) || QFile::remove(tempPath)) {
            if (QFile::copy(helperPath, tempPath)) {
                QFile::setPermissions(tempPath,
                                      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                      QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                      QFileDevice::ReadOther | QFileDevice::ExeOther);
                helperPath = tempPath;
            }
        } else {
            helperPath = tempPath;
        }
    }

    return helperPath;
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
    if (!output.isEmpty()) { 
        log(output, "white"); 
        accumulatedOutput += output; 

        // Parse progress like "Read: 50%" or "Erase: 20%" or "Write: 80%"
        QRegularExpression progRe("(\\d+)%");
        QRegularExpressionMatchIterator i = progRe.globalMatch(output);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            ui->progressBar->setValue(match.captured(1).toInt());
        }
    }
    if (!error.isEmpty()) { 
        log(error, "yellow"); 
        accumulatedError += error; 

        // Some flashrom versions output progress to stderr
        QRegularExpression progRe("(\\d+)%");
        QRegularExpressionMatchIterator i = progRe.globalMatch(error);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            ui->progressBar->setValue(match.captured(1).toInt());
        }
    }
    if (currentState == State::Writing && (error.contains("expected size") || error.contains("doesn't match"))) handleSmartWrite(error);
}
void MainWindow::handleSmartWrite(const QString &error) {
    QRegularExpression re("(?:file|Image) size \\((\\d+) ?B?\\).*?(?:flash chip's|expected) size \\((\\d+) ?B?\\)");
    QRegularExpressionMatch match = re.match(error);
    if (match.hasMatch()) {
        lastInfo.fileSize = match.captured(1).toLong();
        lastInfo.flashSize = match.captured(2).toLong();
        if (lastInfo.fileSize < lastInfo.flashSize) {
            smartWritePending = true;
            log(tr("Size mismatch. Reading..."), "cyan");
            currentState = State::SmartRead;
        }
    }
}

QStringList MainWindow::buildRetryArgsForState(State state, const QString &chipName) {
    QStringList args = {"flashrom", "-p", getProgrammerArgs(), "-c", chipName};
    switch (state) {
        case State::Detecting:
            break;
        case State::Reading:
            args << "-r" << currentFile;
            break;
        case State::Writing:
            args << "-w" << currentFile;
            break;
        case State::Erasing:
            args << "-E";
            break;
        case State::SmartRead:
            args << "-r" << getWorkPath("readx.bin");
            break;
        default:
            return {};
    }
    return args;
}

bool MainWindow::retryOperationWithDetectedChip(const QString &combinedOutput) {
    if (!combinedOutput.contains("Multiple flash chip definitions match") &&
        !combinedOutput.contains("Please specify which chip definition")) {
        return false;
    }

    detectedChips.clear();
    QRegularExpression re("\"([^\"]+)\"");
    QRegularExpressionMatchIterator i = re.globalMatch(combinedOutput);
    const QStringList blackList = {"serprog", "flashrom", "mapping", "stm32", "protocol"};
    while (i.hasNext()) {
        QString name = i.next().captured(1);
        bool blacklisted = false;
        for (const QString &keyword : blackList) {
            if (name.contains(keyword, Qt::CaseInsensitive)) {
                blacklisted = true;
                break;
            }
        }
        if (!blacklisted && name.length() > 3) detectedChips << name;
    }
    if (detectedChips.isEmpty()) return false;

    const QString firstChip = detectedChips.first();
    const State lastState = currentState;
    const QStringList retryArgs = buildRetryArgsForState(lastState, firstChip);
    if (retryArgs.isEmpty()) return false;

    ui->comboChip->setCurrentText(firstChip);
    ui->comboEepromChip->setCurrentText(firstChip);
    detectedChips.clear();
    accumulatedError.clear();
    accumulatedOutput.clear();
    QTimer::singleShot(300, this, [this, lastState, retryArgs]() {
        currentState = lastState;
        runCommand("pkexec", retryArgs);
    });
    return true;
}

bool MainWindow::handleSmartReadFailure() {
    if (currentState != State::SmartRead) return false;

    log(tr("Smart Merge: Step 1/3"), "cyan");
    ui->statusbar->showMessage(tr("Smart Merge: Step 1/3 (Reading)"));
    const QString targetPath = getWorkPath("readx.bin");
    QFile::remove(targetPath);
    runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-r", targetPath});
    currentState = State::Reading;
    return true;
}

bool MainWindow::handleFailedProcess() {
    const QString combined = accumulatedOutput + "\n" + accumulatedError;
    if (retryOperationWithDetectedChip(combined)) return true;
    if (handleSmartReadFailure()) return true;

    if (!accumulatedOutput.contains("SUCCESS")) {
        log(tr("Failed"), "red");
        ui->statusbar->showMessage(tr("Operation Failed"), 5000);
    }
    clearSmartWriteArtifacts();
    return false;
}

bool MainWindow::handleSuccessfulLocalDetect() {
    if (currentState != State::LocalDetect) return false;

    QRegularExpression re("SUCCESS: ([0-9A-F]+) ([0-9A-F]+) ([0-9A-F]+)");
    QRegularExpressionMatch match = re.match(accumulatedOutput);
    if (!match.hasMatch()) return true;

    const QString info = QString("ID: %1 %2 %3").arg(match.captured(1), match.captured(2), match.captured(3));
    ui->labelLocalChip->setText(info);
    log(tr("Detected Local Chip: %1").arg(info), "green");
    const uint8_t c = match.captured(3).toInt(nullptr, 16);
    if (c >= 0x13 && c <= 0x21) {
        localSpi->setFlashSize(1 << c);
        log(tr("Flash Size: %1 MB").arg(localSpi->getFlashSize() / 1024 / 1024));
    }
    return true;
}

bool MainWindow::handleSuccessfulLocalRead() {
    if (currentState != State::LocalRead) return false;

    const QString tempFile = getWorkPath("flash_read.bin");
    if (!QFile::exists(tempFile)) return true;

    if (!localSavePath.isEmpty()) {
        QFile::remove(localSavePath);
        if (QFile::copy(tempFile, localSavePath)) {
            log(tr("Flash backup saved to: %1").arg(localSavePath), "green");
        } else {
            log(tr("Failed to save backup to: %1").arg(localSavePath), "red");
        }
    }

    QFile f(tempFile);
    if (f.open(QIODevice::ReadOnly)) {
        loadDataToEditor(f.readAll());
        f.close();
        log(tr("Local read finished and loaded into editor."), "green");
    }
    return true;
}

bool MainWindow::handleSuccessfulLocalWrite() {
    if (currentState != State::LocalWrite) return false;

    if (accumulatedOutput.contains("SUCCESS")) {
        log(tr("Local write successfully completed!"), "green");
    } else {
        log(tr("Local write finished without a success marker."), "yellow");
        ui->statusbar->showMessage(tr("Local write result is uncertain"), 5000);
    }
    return true;
}

void MainWindow::handleSuccessfulDetect() {
    if (currentState != State::Detecting) return;

    QRegularExpression re("Found [^ ]+ flash chip \"([^\"]+)\"");
    QRegularExpressionMatch match = re.match(accumulatedError);
    if (!match.hasMatch()) return;

    const QString chipName = match.captured(1);
    ui->comboChip->setCurrentText(chipName);
    ui->comboEepromChip->setCurrentText(chipName);
    log(tr("Detected chip updated to preview: %1").arg(chipName), "cyan");
}

bool MainWindow::handleSmartMergeSuccess() {
    if (currentState != State::Reading || !smartWritePending) return false;

    log(tr("Step 2/3: Merging"), "cyan");
    ui->statusbar->showMessage(tr("Smart Merge: Step 2/3 (Merging)"));

    QFile flashFile(getWorkPath("readx.bin"));
    QFile newFile(currentFile);
    QFile outFile(getWorkPath("tempx.bin"));
    if (!flashFile.open(QIODevice::ReadOnly) || !newFile.open(QIODevice::ReadOnly) || !outFile.open(QIODevice::WriteOnly)) {
        clearSmartWriteArtifacts();
        return false;
    }

    outFile.write(newFile.readAll());
    flashFile.seek(lastInfo.fileSize);
    outFile.write(flashFile.readAll());
    flashFile.close();
    newFile.close();
    outFile.close();

    QFile layout(getWorkPath("flashrom.layout"));
    if (!layout.open(QIODevice::WriteOnly)) {
        clearSmartWriteArtifacts();
        return false;
    }

    layout.write(QString("00000000:%1 flashzone").arg(lastInfo.fileSize - 1, 8, 16, QChar('0')).toUtf8());
    layout.close();
    currentState = State::SmartWrite;
    runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-l", getWorkPath("flashrom.layout"), "-i", "flashzone", "-w", getWorkPath("tempx.bin")});
    return true;
}

void MainWindow::loadReadResultIntoEditor() {
    if (currentState != State::Reading && currentState != State::EepromRead) return;

    QFile f((currentState == State::EepromRead) ? eepromFile : currentFile);
    if (f.open(QIODevice::ReadOnly)) {
        loadDataToEditor(f.readAll());
        f.close();
    }
}

bool MainWindow::handleSuccessfulProcess() {
    handleSuccessfulLocalDetect();
    handleSuccessfulLocalRead();
    handleSuccessfulLocalWrite();
    handleSuccessfulDetect();

    if (handleSmartMergeSuccess()) return true;
    if (currentState == State::SmartWrite) {
        log(tr("Successful!"), "green");
        ui->statusbar->showMessage(tr("Smart Merge Successful"), 5000);
        clearSmartWriteArtifacts();
        return false;
    }

    log(tr("Successful"), "green");
    ui->statusbar->showMessage(tr("Operation Successful"), 5000);
    loadReadResultIntoEditor();
    return false;
}

void MainWindow::processFinished(int exitCode) {
    lockUi(false);
    if (exitCode != 0) {
        if (handleFailedProcess()) return;
    } else {
        if (handleSuccessfulProcess()) return;
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
void MainWindow::log(const QString &msg, const QString &color) {
    if (msg.isEmpty()) return;
    ui->textLog->moveCursor(QTextCursor::End);
    
    // Use HTML to set color and ensure characters are properly escaped
    QString escapedMsg = msg.toHtmlEscaped().replace("\n", "<br>");
    QString html = QString("<font color=\"%1\">%2</font><br>").arg(color, escapedMsg);
    
    ui->textLog->insertHtml(html);
    ui->textLog->ensureCursorVisible();
}
QString MainWindow::getProgrammerArgs(bool isEeprom) const {
    if (isEeprom) return ui->comboEepromProg->currentData().toString();
    QString base = ui->comboProgrammer->currentData().toString();
    QString speed = ui->comboSpeed->currentData().toString();
    
    QString device;
    if (ui->comboDevice->currentData().toString() == "auto" || ui->comboDevice->currentText().isEmpty()) {
        device = "";
    } else {
        device = ui->comboDevice->currentText();
    }

    if (base.contains("serprog")) {
        if (device.isEmpty()) { device = "/dev/ttyACM0"; if (!QFileInfo("/dev/ttyACM0").exists()) device = "/dev/ttyUSB0"; }
        base = "serprog:dev=" + device + ":4000000";
        if (!speed.isEmpty()) base += ",spispeed=" + speed;
    } else if (base.contains("linux_spi")) {
        if (device.isEmpty()) device = "/dev/spidev1.0";
        base = "linux_spi:dev=" + device;
        if (!speed.isEmpty()) base += ",spispeed=" + speed;
    } else if (base.contains("buspirate_spi")) {
        if (device.isEmpty()) device = "/dev/ttyUSB0";
        base = "buspirate_spi:dev=" + device;
        if (!speed.isEmpty()) base += ",spispeed=" + speed;
    }
    return base;
}

void MainWindow::on_comboLang_currentIndexChanged(int index) {
    const QString lang = ui->comboLang->itemData(index).toString();
    QSettings settings;
    settings.setValue("ui/language", lang);

    qApp->removeTranslator(translator);
    bool loaded = false;
    if (lang == "zh_CN") loaded = translator->load(":/i18n/FlashHelper_zh_CN.qm");
    if (loaded) qApp->installTranslator(translator);
    ui->retranslateUi(this);
    updateDynamicTexts();
    if (previewGroup) previewGroup->setTitle(tr("Chip Pinout Preview"));
    ui->statusbar->showMessage(tr("Idle"));
    updateSystemStatus();
    fetchSupportedChips();
}

void MainWindow::on_btnLocalBrowse_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open BIOS File"), "", tr("Binary Files (*.bin *.rom);;All Files (*)"));
    if (!fileName.isEmpty()) { 
        localFile = fileName; 
        ui->editLocalFile->setText(localFile); 
        log(tr("Selected local file: %1").arg(localFile)); 
        QFile f(fileName); if (f.open(QIODevice::ReadOnly)) { loadDataToEditor(f.readAll()); f.close(); }
    }
}
void MainWindow::on_btnLocalDetect_clicked() { currentState = State::LocalDetect; if (!runLocalHelper({"detect"})) currentState = State::Idle; }
void MainWindow::on_btnLocalRead_clicked() { 
    if (localSpi->getFlashSize() == 0) {
        QMessageBox::warning(this, tr("Flash Size Unknown"),
                             tr("Unable to determine local flash size. Detect the chip first."));
        return;
    }

    localSavePath = QFileDialog::getSaveFileName(this, tr("Save BIOS Backup"), "", tr("Binary Files (*.bin *.rom);;All Files (*)"));
    if (localSavePath.isEmpty()) return;

    currentState = State::LocalRead; 
    lockUi(true);
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    ui->progressBar->show();
    ui->statusbar->showMessage(tr("Starting local read (%1 MB)...").arg(localSpi->getFlashSize() / 1024 / 1024));
    if (!runLocalHelper({"read", getWorkPath("flash_read.bin"), QString::number(localSpi->getFlashSize())})) {
        currentState = State::Idle;
        lockUi(false);
        ui->progressBar->hide();
    }
}
void MainWindow::on_btnLocalWrite_clicked() { 
    if (localFile.isEmpty()) { QMessageBox::warning(this, tr("Error"), tr("Please select a file first.")); return; }
    QFileInfo fileInfo(localFile);
    if (!fileInfo.exists() || fileInfo.size() <= 0) {
        QMessageBox::warning(this, tr("Error"), tr("The selected local file is missing or empty."));
        return;
    }
    currentState = State::LocalWrite; 
    lockUi(true);
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    ui->progressBar->show();
    ui->statusbar->showMessage(tr("Starting local erase & write..."));
    if (!runLocalHelper({"write", localFile})) {
        currentState = State::Idle;
        lockUi(false);
        ui->progressBar->hide();
    }
}

void MainWindow::showLogContextMenu(const QPoint &pos) {
    QMenu menu(this);
    QAction *clearAction = menu.addAction(tr("Clear Log"));
    connect(clearAction, &QAction::triggered, ui->textLog, &QTextEdit::clear);
    menu.exec(ui->textLog->mapToGlobal(pos));
}
void MainWindow::on_comboProgrammer_currentIndexChanged(int) {
    refreshDeviceList();
}

void MainWindow::lockUi(bool locked) {
    ui->tabWidget->setEnabled(!locked);
    // Explicitly hide tab bar to make it truly locked visually
    if (locked) {
        ui->tabWidget->setStyleSheet("QTabBar::tab { height: 0px; width: 0px; margin: 0px; padding: 0px; border: none; }");
    } else {
        ui->tabWidget->setStyleSheet("");
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (currentState == State::LocalWrite || currentState == State::LocalRead) {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setWindowTitle(tr("CRITICAL WARNING"));
        msgBox.setText(tr("A local BIOS update is currently in progress!"));
        msgBox.setInformativeText(tr("If you close the application now, your BIOS might be left in an incomplete state, "
                                     "and your computer WILL NOT BOOT next time.\n\n"
                                     "Are you absolutely sure you want to risk bricking your machine?"));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        
        if (msgBox.exec() == QMessageBox::Yes) {
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}
