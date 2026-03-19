#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "chippreviewwidget.h"
#include "smartmerge.h"
#include "localflashmanager.h"
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
    flashController = new FlashOperationController(this);
    localFlashManager = new LocalFlashManager(flashController, localSpi, this);

    // Flash Controller connections
    connect(flashController, &FlashOperationController::logMessage, this, &MainWindow::log);
    connect(flashController, &FlashOperationController::finished, this, [this](int exitCode, QProcess::ExitStatus, FlashOperationController::State finishedState) {
        processFinished(exitCode, finishedState);
    });
    connect(flashController, &FlashOperationController::outputReceived, this, [this](const QString &output) {
        if (output.isEmpty()) return;
        
        QRegularExpression progRe("(\\d+)%");
        auto matches = progRe.globalMatch(output);
        QString lastMatch;
        while (matches.hasNext()) { lastMatch = matches.next().captured(1); }
        if (!lastMatch.isEmpty()) { ui->progressBar->setValue(lastMatch.toInt()); }

        if (output.trimmed().size() > 4 || output.contains('\n')) { log(output, "white"); }
    });
    connect(flashController, &FlashOperationController::errorReceived, this, [this](const QString &error) {
        if (error.isEmpty()) return;

        QRegularExpression progRe("(\\d+)%");
        auto matches = progRe.globalMatch(error);
        QString lastMatch;
        while (matches.hasNext()) { lastMatch = matches.next().captured(1); }
        if (!lastMatch.isEmpty()) { ui->progressBar->setValue(lastMatch.toInt()); }

        if (error.trimmed().size() > 4 || error.contains('\n')) { log(error, "yellow"); }

        if (currentState() == State::Writing && (error.contains("expected size") || error.contains("doesn't match"))) {
            handleSmartWrite(error);
        }
    });

    // Local Flash Manager connections
    connect(localFlashManager, &LocalFlashManager::logMessage, this, &MainWindow::log);
    connect(localFlashManager, &LocalFlashManager::statusMessage, this, [this](const QString &msg, int timeout) {
        ui->statusbar->showMessage(msg, timeout);
    });
    connect(localFlashManager, &LocalFlashManager::chipDetected, this, [this](const QString &info, qint64 size) {
        ui->labelLocalChip->setText(info);
        log(tr("Detected Local Chip: %1").arg(info), "green");
        log(tr("Flash Size: %1 MB").arg(size / 1024 / 1024));
        ui->btnLocalRead->setEnabled(true);
        ui->btnLocalWrite->setEnabled(true);
    });
    connect(localFlashManager, &LocalFlashManager::detectUnknownSize, this, [this](const QString &info) {
        ui->labelLocalChip->setText(info);
        QMessageBox::warning(this, tr("Flash Size Unknown"),
                             tr("The chip ID was detected (%1), but its capacity code is unknown.\n\n"
                                "Local read/write operations are disabled for safety.")
                                 .arg(info));
        ui->btnLocalRead->setEnabled(false);
        ui->btnLocalWrite->setEnabled(false);
    });

    translator = new QTranslator(this);
    idleTimer = new QTimer(this);
    aboutVersionLabel = nullptr;
    aboutInstructionsLabel = nullptr;
    idleTimer->setSingleShot(true);
    connect(idleTimer, &QTimer::timeout, this, [this]() { ui->statusbar->showMessage(tr("Idle")); });

    ui->setupUi(this);
    setWindowIcon(QIcon(":/flashhelper.svg"));
    
    ui->progressBar->hide();
    ui->progressBar->setTextVisible(true);
    ui->progressBar->setFormat("%p%");
    setupPreviewPanels();

    ui->comboChip->setMinimumWidth(350);
    ui->comboChip->setMaxVisibleItems(20);
    ui->comboChip->view()->setMinimumWidth(400);
    ui->comboEepromChip->setMinimumWidth(350);
    ui->comboEepromChip->view()->setMinimumWidth(400);
    setupAboutPage();
    setupProgrammerOptions();

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

void MainWindow::setupPreviewPanels() {
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
        if (item->layout()) {
            spiLeftLayout->addLayout(item->layout());
        } else if (item->widget()) {
            spiLeftLayout->addWidget(item->widget());
        }
    }
    spiMainLayout->addLayout(spiLeftLayout, 2);
    spiLeftLayout->addStretch();
    spiMainLayout->addWidget(previewGroup, 1);
    ui->verticalLayout_2->addWidget(spiContent);

    QWidget *eepromContent = new QWidget(this);
    QHBoxLayout *eepromMainLayout = new QHBoxLayout(eepromContent);
    QVBoxLayout *eepromLeftLayout = new QVBoxLayout();
    while (ui->verticalLayout_eeprom->count() > 0) {
        QLayoutItem *item = ui->verticalLayout_eeprom->takeAt(0);
        if (item->layout()) {
            eepromLeftLayout->addLayout(item->layout());
        } else if (item->widget()) {
            eepromLeftLayout->addWidget(item->widget());
        }
    }
    eepromMainLayout->addLayout(eepromLeftLayout, 2);
    eepromLeftLayout->addStretch();
    ui->verticalLayout_eeprom->addWidget(eepromContent);

    connect(ui->comboChip, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        chipPreview->setChipModel(text);
    });
    connect(ui->comboEepromChip, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        chipPreview->setChipModel(text);
    });
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this, spiMainLayout, eepromMainLayout](int index) {
        if (index == 0) {
            spiMainLayout->addWidget(previewGroup);
        } else if (index == 1) {
            eepromMainLayout->addWidget(previewGroup);
        }

        const bool isSettingsOrAbout = (index == 3 || index == 4);
        ui->editorContainer->setVisible(!isSettingsOrAbout);
        ui->textLog->setVisible(!isSettingsOrAbout);

        updateTabHeight();
        if (index != 2) {
            ui->progressBar->hide();
        }
    });
}

void MainWindow::setupAboutPage() {
    QString version = "unknown";
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
}

void MainWindow::setupProgrammerOptions() {
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

    const QString arch = QSysInfo::currentCpuArchitecture();
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
    if (currentState() != State::SmartRead && currentState() != State::SmartWrite &&
        !(currentState() == State::Reading && smartWritePending)) {
        clearSmartWriteArtifacts();
    }
}

QStringList MainWindow::applySelectedChipToArgs(QStringList args) const {
    const QString currentChip = ui->comboChip->currentText();
    const bool isAuto = (currentChip == tr("Auto Detect") || currentChip.isEmpty());
    if (isAuto || currentState() == State::Detecting || args.contains("-c")) return args;

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
    const bool hasMemAccess = QFileInfo("/dev/mem").isReadable();
    
    // Allow direct access for serprog if serial port is writable, or if /dev/mem is accessible
    return (programmerArgs.contains("serprog") && hasSerialAccess) || hasMemAccess;
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

void MainWindow::runCommand(State state, const QString &cmd, const QStringList &args) {
    if (idleTimer) idleTimer->stop();
    QStringList finalArgs = applySelectedChipToArgs(args);
    QString finalCmd = cmd;
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

    ui->statusbar->showMessage(statusMessageForState(state));
    log(tr("Command: %1 %2").arg(finalCmd, finalArgs.join(" ")), "gray");
    flashController->startFlashromOperation(state, finalCmd, finalArgs);
}

bool MainWindow::runLocalHelper(State state, const QStringList &args) {
    QString helperPath = getHelperPath();
    if (helperPath.isEmpty()) {
        QMessageBox::warning(this, tr("Helper Missing"), tr("flashhelper-helper was not found."));
        return false;
    }

    flashController->startLocalHelperOperation(state, helperPath, args);
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

void MainWindow::on_btnDetect_clicked() {
    startFlashromOperation(State::Detecting, {"flashrom", "-p", getProgrammerArgs()});
}
void MainWindow::on_btnRead_clicked() {
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save BIOS"), "backup.bin", tr("Binary (*.bin *.fd);;All (*.*)"));
    if (savePath.isEmpty()) return;
    currentFile = savePath;
    startFlashromOperation(State::Reading, {"flashrom", "-p", getProgrammerArgs(), "-r", savePath});
}
void MainWindow::on_btnWrite_clicked() {
    startFlashromOperation(State::Writing, {"flashrom", "-p", getProgrammerArgs(), "-w", currentFile});
}

void MainWindow::on_btnErase_clicked() {
    if (QMessageBox::question(this, tr("Confirm"), tr("ERASE?")) == QMessageBox::Yes) {
        startFlashromOperation(State::Erasing, {"flashrom", "-p", getProgrammerArgs(), "-E"});
    }
}

void MainWindow::on_btnEepromBrowse_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open"), "", tr("Binary (*.*)"));
    if (fileName.isEmpty()) return;

    ui->lineEepromFile->setText(fileName);
    eepromFile = fileName;
    loadFileIntoEditor(fileName, tr("Open Failed"), tr("Loaded EEPROM file: %1").arg(fileName));
}
void MainWindow::on_btnEepromRead_clicked() {
    QString chip = ui->comboEepromChip->currentText();
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save"), "eeprom.bin", tr("Binary (*.*)"));
    if (savePath.isEmpty()) return;
    eepromFile = savePath; currentFile = savePath;
    QStringList args = {"flashrom", "-p", getProgrammerArgs(true), "-r", savePath};
    if (chip != tr("Auto Detect")) args << "-c" << chip;
    startFlashromOperation(State::EepromRead, args);
}
void MainWindow::on_btnEepromWrite_clicked() {
    QString chip = ui->comboEepromChip->currentText();
    QString targetFile = prepareWriteFile();
    if (targetFile.isEmpty()) return;
    QStringList args = {"flashrom", "-p", getProgrammerArgs(true), "-w", targetFile};
    if (chip != tr("Auto Detect")) args << "-c" << chip;
    startFlashromOperation(State::EepromWrite, args);
}
void MainWindow::on_btnEepromErase_clicked() {
    QString chip = ui->comboEepromChip->currentText();
    if (QMessageBox::question(this, tr("Confirm"), tr("ERASE?")) == QMessageBox::Yes) {
        QStringList args = {"flashrom", "-p", getProgrammerArgs(true), "-E"};
        if (chip != tr("Auto Detect")) args << "-c" << chip;
        startFlashromOperation(State::EepromErase, args);
    }
}

void MainWindow::handleSmartWrite(const QString &error) {
    QRegularExpression re("(?:file|Image) size \\((\\d+) ?B?\\).*?(?:flash chip's|expected) size \\((\\d+) ?B?\\)");
    QRegularExpressionMatch match = re.match(error);
    if (match.hasMatch()) {
        lastInfo.fileSize = match.captured(1).toLong();
        lastInfo.flashSize = match.captured(2).toLong();
        if (lastInfo.fileSize < lastInfo.flashSize) {
            smartWritePending = true;
            log(tr("Size mismatch. Preparing Smart Merge..."), "cyan");
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

bool MainWindow::retryOperationWithDetectedChip(const QString &combinedOutput, State finishedState) {
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
    const QStringList retryArgs = buildRetryArgsForState(finishedState, firstChip);
    if (retryArgs.isEmpty()) return false;

    ui->comboChip->setCurrentText(firstChip);
    ui->comboEepromChip->setCurrentText(firstChip);
    detectedChips.clear();
    
    // Clear and keep busy
    beginBusyOperation(tr("Retrying with chip %1...").arg(firstChip), true);

    QTimer::singleShot(300, this, [this, finishedState, retryArgs]() {
        runCommand(finishedState, "pkexec", retryArgs);
    });
    return true;
}

bool MainWindow::handleSmartReadFailure() {
    if (!smartWritePending) return false;

    log(tr("Smart Merge: Step 1/3 (Reading original flash)"), "cyan");
    ui->statusbar->showMessage(tr("Smart Merge: Step 1/3 (Reading)"));
    const QString targetPath = getWorkPath("readx.bin");
    QFile::remove(targetPath);
    runCommand(State::SmartRead, "pkexec", {"flashrom", "-p", getProgrammerArgs(), "-r", targetPath});
    return true;
}

bool MainWindow::handleFailedProcess(int exitCode, State finishedState) {
    const QString combined = flashController->accumulatedOutput() + "\n" + flashController->accumulatedError();
    if (retryOperationWithDetectedChip(combined, finishedState)) return true;
    if (handleSmartReadFailure()) return true;
    if (localFlashManager->handleFailedOperation(finishedState, exitCode)) return false;

    if (!flashController->accumulatedOutput().contains("SUCCESS")) {
        log(tr("Failed"), "red");
        ui->statusbar->showMessage(tr("Operation Failed"), 5000);
    }
    clearSmartWriteArtifacts();
    return false;
}

void MainWindow::cleanupLocalReadArtifact() {
    QFile::remove(getWorkPath("flash_read.bin"));
}

void MainWindow::handleSuccessfulDetect() {
    if (currentState() != State::Detecting) return;

    QRegularExpression re("Found [^ ]+ flash chip \"([^\"]+)\"");
    QRegularExpressionMatch match = re.match(flashController->accumulatedError());
    if (!match.hasMatch()) return;

    const QString chipName = match.captured(1);
    ui->comboChip->setCurrentText(chipName);
    ui->comboEepromChip->setCurrentText(chipName);
    log(tr("Detected chip updated to preview: %1").arg(chipName), "cyan");
}

bool MainWindow::handleSmartMergeSuccess() {
    if (currentState() != State::Reading || !smartWritePending) return false;

    log(tr("Step 2/3: Merging"), "cyan");
    ui->statusbar->showMessage(tr("Smart Merge: Step 2/3 (Merging)"));

    QString errorMessage;
    if (!SmartMerge::preparePartialWrite(getWorkPath("readx.bin"),
                                         currentFile,
                                         getWorkPath("tempx.bin"),
                                         getWorkPath("flashrom.layout"),
                                         lastInfo.fileSize,
                                         &errorMessage)) {
        if (!errorMessage.isEmpty()) log(errorMessage, "red");
        clearSmartWriteArtifacts();
        return false;
    }

    startSmartMergeWrite();
    return true;
}

void MainWindow::startSmartMergeWrite() {
    runCommand(State::SmartWrite, "pkexec", {"flashrom", "-p", getProgrammerArgs(), "-l", getWorkPath("flashrom.layout"), "-i", "flashzone", "-w", getWorkPath("tempx.bin")});
}

void MainWindow::loadReadResultIntoEditor() {
    if (currentState() != State::Reading && currentState() != State::EepromRead) return;

    const QString path = (currentState() == State::EepromRead) ? eepromFile : currentFile;
    loadFileIntoEditor(path, tr("Readback Failed"));
}

bool MainWindow::handleSuccessfulProcess(State finishedState) {
    if (localFlashManager->handleSuccessfulDetect(finishedState)) return false;
    if (localFlashManager->handleSuccessfulRead(finishedState, localSavePath, getWorkPath("flash_read.bin"))) {
        QFile f(getWorkPath("flash_read.bin"));
        if (f.open(QIODevice::ReadOnly)) {
            loadDataToEditor(f.readAll());
            f.close();
            log(tr("Local read finished and loaded into editor."), "green");
        }
        cleanupLocalReadArtifact();
        return false;
    }
    if (localFlashManager->handleSuccessfulWrite(finishedState)) return false;

    handleSuccessfulDetect();

    if (handleSmartMergeSuccess()) return true;
    if (finishedState == State::SmartWrite) {
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

void MainWindow::processFinished(int exitCode, State finishedState) {
    bool stayBusy = false;
    if (exitCode != 0) {
        stayBusy = handleFailedProcess(exitCode, finishedState);
    } else {
        stayBusy = handleSuccessfulProcess(finishedState);
    }

    if (!stayBusy) {
        finishBusyOperation(true);
    }
}

void MainWindow::loadDataToEditor(const QByteArray &data) { ui->hexEditor->setData(data); }
bool MainWindow::loadFileIntoEditor(const QString &path, const QString &errorTitle, const QString &successMessage) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, errorTitle, tr("Failed to open file: %1").arg(path));
        return false;
    }

    loadDataToEditor(f.readAll());
    f.close();
    if (!successMessage.isEmpty()) log(successMessage, "green");
    return true;
}

bool MainWindow::writeEditorDataToFile(const QString &path, const QString &emptyTitle, const QString &emptyMessage,
                                       const QString &errorTitle, const QString &errorMessage) {
    const QByteArray data = ui->hexEditor->data();
    if (data.isEmpty()) {
        QMessageBox::warning(this, emptyTitle, emptyMessage);
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) {
        QMessageBox::warning(this, errorTitle, errorMessage);
        return false;
    }

    f.close();
    return true;
}

void MainWindow::beginBusyOperation(const QString &statusMessage, bool lockTabs, bool showProgress) {
    if (lockTabs) lockUi(true);
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    if (showProgress) {
        ui->progressBar->show();
    } else {
        ui->progressBar->hide();
    }
    ui->statusbar->showMessage(statusMessage);
}

void MainWindow::finishBusyOperation(bool startIdleStatusTimer) {
    lockUi(false);
    ui->progressBar->hide();
    if (startIdleStatusTimer && idleTimer) {
        idleTimer->start(5000);
    }
}

void MainWindow::abortBusyOperation() {
    finishBusyOperation(false);
}

bool MainWindow::startFlashromOperation(State state, const QStringList &args, bool lockTabs) {
    // Progress bar disabled per user request
    beginBusyOperation(statusMessageForState(state), lockTabs, false);
    runCommand(state, "pkexec", args);
    return true;
}

QString MainWindow::prepareWriteFile() {
    const QString tempPath = getWorkPath("flash_buffer.bin");
    if (writeEditorDataToFile(tempPath,
                              tr("No Data"),
                              tr("There is no data in the editor to write."),
                              tr("Write Buffer Failed"),
                              tr("Failed to prepare the temporary write buffer."))) {
        return tempPath;
    }
    return QString();
}
void MainWindow::on_btnSaveFile_clicked() {
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save Binary File"), currentFile, tr("Binary (*.bin *.fd);;All (*.*)"));
    if (savePath.isEmpty()) return;

    if (!writeEditorDataToFile(savePath,
                               tr("No Data"),
                               tr("There is no data in the editor to save."),
                               tr("Save Failed"),
                               tr("Failed to save file: %1").arg(savePath))) {
        return;
    }

    currentFile = savePath;
    log(tr("File saved: %1").arg(savePath), "green");
}
void MainWindow::on_btnBrowse_clicked() { 
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open BIOS"), "", tr("Binary (*.bin *.fd);;All (*.*)")); 
    if (fileName.isEmpty()) return;

    ui->lineFile->setText(fileName);
    currentFile = fileName;
    loadFileIntoEditor(fileName, tr("Open Failed"), tr("Loaded BIOS file: %1").arg(fileName));
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
    if (fileName.isEmpty()) return;

    localFile = fileName; 
    ui->editLocalFile->setText(localFile); 
    loadFileIntoEditor(fileName, tr("Open Failed"), tr("Selected local file: %1").arg(localFile));
}
void MainWindow::on_btnLocalDetect_clicked() {
    beginBusyOperation(statusMessageForState(State::LocalDetect), true);
    if (!runLocalHelper(State::LocalDetect, {"detect"})) {
        abortBusyOperation();
    }
}
void MainWindow::on_btnLocalRead_clicked() { 
    if (localSpi->getFlashSize() == 0) {
        QMessageBox::warning(this, tr("Flash Size Unknown"),
                             tr("Unable to determine local flash size. Detect the chip first."));
        return;
    }

    localSavePath = QFileDialog::getSaveFileName(this, tr("Save BIOS Backup"), "", tr("Binary Files (*.bin *.rom);;All Files (*)"));
    if (localSavePath.isEmpty()) return;

    beginBusyOperation(tr("Starting local read (%1 MB)...").arg(localSpi->getFlashSize() / 1024 / 1024), true, true);
    if (!runLocalHelper(State::LocalRead, {"read", getWorkPath("flash_read.bin"), QString::number(localSpi->getFlashSize())})) {
        abortBusyOperation();
    }
}
void MainWindow::on_btnLocalWrite_clicked() {
    const LocalFlash::ValidationResult validation =
        LocalFlash::validateImageFile(localFile, localSpi->getFlashSize());
    if (!validation.ok) {
        QMessageBox::warning(this, validation.title, validation.message);
        return;
    }

    beginBusyOperation(tr("Starting local erase & write..."), true, true);
    if (!runLocalHelper(State::LocalWrite, {"write", localFile, QString::number(localSpi->getFlashSize())})) {
        abortBusyOperation();
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
    if (currentState() == State::LocalWrite || currentState() == State::LocalRead) {
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
