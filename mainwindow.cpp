#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDataStream>
#include <QCoreApplication>
#include <QDir>

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

    ui->comboProgrammer->addItem("Serprog (STM32)", "serprog:dev=/dev/ttyACM0:4000000,spispeed=36000000");
    ui->comboProgrammer->addItem("Linux SPI", "linux_spi:dev=/dev/spidev1.0,spispeed=20000");

    updateSystemStatus();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::on_btnBrowse_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open BIOS File", "", "BIOS files (*.fd *.bin *.rom);;All files (*.*)");
    if (!fileName.isEmpty()) {
        ui->lineFile->setText(fileName);
        currentFile = fileName;
    }
}

void MainWindow::updateSystemStatus() {
    // 探测 flashrom 路径
    QString flashromPath = "/usr/sbin/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = "/usr/bin/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = QCoreApplication::applicationDirPath() + "/flashrom";

    // --- 1. 硬件权限检测 (实战演练) ---
    QProcess check;
    check.start(flashromPath, {"-p", getProgrammerArgs()});
    check.waitForFinished(1000);
    QString err = check.readAllStandardError();
    
    // 如果不包含 "Permission denied"，说明 Udev 规则已经生效，硬件已打通
    bool udevActive = !err.contains("Permission denied") && !err.isEmpty();
    
    // --- 2. 规则文件物理检查 ---
    // Udev 目录通常可读，Polkit 目录不可读，所以我们主要看 Udev 文件
    bool udevFileExist = QFile::exists("/etc/udev/rules.d/z60_flashrom.rules");

    // 更新 UI
    ui->lblUdevStatus->setText(udevActive ? "Hardware Access: <font color='green'>OK (Direct Access)</font>" : "Hardware Access: <font color='red'>Denied (Requires Root)</font>");
    
    // 只要硬件通了或者文件在，我们就认为系统规则已就绪
    ui->lblPolkitStatus->setText(udevFileExist ? "System Rules: <font color='green'>Installed & Active</font>" : "System Rules: <font color='red'>Not Found</font>");
    
    ui->btnRemoveRules->setEnabled(udevFileExist);
}

void MainWindow::on_btnInstallRules_clicked() {
    QString udevContent = "# STM32 VSerprog\\nSUBSYSTEMS==\\\"usb\\\", ATTRS{idVendor}==\\\"0483\\\", ATTRS{idProduct}==\\\"5740\\\", TAG+=\\\"uaccess\\\"\\nKERNEL==\\\"ttyACM*\\\", TAG+=\\\"uaccess\\\"\\nKERNEL==\\\"spidev*\\\", TAG+=\\\"uaccess\\\"\\n";
    QString polkitRule = "polkit.addRule(function(action, subject) { "
                         "if (action.id == \\\"org.freedesktop.policykit.exec\\\" && "
                         "(action.lookup(\\\"program\\\") == \\\"/usr/sbin/flashrom\\\" || action.lookup(\\\"program\\\") == \\\"/usr/bin/flashrom\\\") && "
                         "subject.isInGroup(\\\"sudo\\\")) { return polkit.Result.YES; } });";

    QString udevB64 = udevContent.toUtf8().toBase64();
    QString polkitB64 = polkitRule.toUtf8().toBase64();

    QString script = QString("echo '%1' | base64 -d > /etc/udev/rules.d/z60_flashrom.rules && "
                             "echo '%2' | base64 -d > /etc/polkit-1/rules.d/10-flashrom.rules && "
                             "chmod 644 /etc/udev/rules.d/z60_flashrom.rules && "
                             "udevadm control --reload-rules && udevadm trigger")
                     .arg(udevB64, polkitB64);

    log("Requesting permission to configure system...");
    QProcess::execute("pkexec", {"bash", "-c", script});
    
    // 给系统一点点时间应用规则
    QThread::msleep(500);
    updateSystemStatus();
    QMessageBox::information(this, "Success", "System configured. If Hardware Access is still red, please RE-PLUG your device!");
}

void MainWindow::on_btnRemoveRules_clicked() {
    QString script = "rm -f /etc/udev/rules.d/z60_flashrom.rules /etc/polkit-1/rules.d/10-flashrom.rules";
    QProcess::execute("pkexec", {"bash", "-c", script});
    updateSystemStatus();
}

void MainWindow::runCommand(const QString &cmd, const QStringList &args) {
    QStringList finalArgs = args;
    QString finalCmd = cmd;
    
    QString flashromPath = "/usr/sbin/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = "/usr/bin/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = QCoreApplication::applicationDirPath() + "/flashrom";

    if (cmd == "pkexec" && !args.isEmpty() && args[0] == "flashrom") {
        // 实战检测是否能免密运行
        QProcess check;
        check.start(flashromPath, {"-p", getProgrammerArgs()});
        check.waitForFinished(500);
        if (!check.readAllStandardError().contains("Permission denied")) {
            finalCmd = flashromPath;
            finalArgs.removeFirst();
            ui->textLog->append("<font color='gray'>[Direct Hardware Access]</font>");
        } else {
            finalArgs[0] = flashromPath;
        }
    }
    
    ui->textLog->append(QString("<b>Running: %1 %2</b>").arg(finalCmd, finalArgs.join(" ")));
    process->start(finalCmd, finalArgs);
}

void MainWindow::on_btnDetect_clicked() { currentState = State::Detecting; runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs()}); }
void MainWindow::on_btnRead_clicked() {
    QString savePath = QFileDialog::getSaveFileName(this, "Save BIOS File", "backup.bin", "BIOS files (*.bin *.fd);;All files (*.*)");
    if (savePath.isEmpty()) return;
    currentState = State::Reading;
    runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-r", savePath});
}
void MainWindow::on_btnErase_clicked() { if (QMessageBox::question(this, "Confirm", "ERASE flash?") == QMessageBox::Yes) { currentState = State::Erasing; runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-E"}); } }
void MainWindow::on_btnWrite_clicked() {
    currentFile = ui->lineFile->text();
    if (!QFile::exists(currentFile)) return;
    currentState = State::Writing;
    runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-w", currentFile});
}

void MainWindow::readProcessOutput() {
    QString output = process->readAllStandardOutput();
    QString error = process->readAllStandardError();
    if (!output.isEmpty()) log(output, "white");
    if (!error.isEmpty()) log(error, "yellow");
    if (currentState == State::Writing && error.contains("expected size")) handleSmartWrite(error);
}

void MainWindow::handleSmartWrite(const QString &output) {
    QRegularExpression re("(?:file|Image) size \\((\\d+) ?B?\\).*?(?:flash chip's|expected) size \\((\\d+) ?B?\\)");
    QRegularExpressionMatch match = re.match(output);
    if (match.hasMatch()) {
        lastInfo.fileSize = match.captured(1).toLong();
        lastInfo.flashSize = match.captured(2).toLong();
        if (lastInfo.fileSize < lastInfo.flashSize) {
            log("Size mismatch! Starting smart merge flow...", "cyan");
            currentState = State::SmartRead;
        }
    }
}

void MainWindow::processFinished(int exitCode) {
    if (exitCode != 0) {
        if (currentState == State::SmartRead) {
            log("Step 1/3: Reading current flash backup...", "cyan");
            runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-r", "/tmp/readx.bin"});
            currentState = State::SmartRead; // 保持状态以在下次 finished 时触发合并
            return;
        }
        log("Operation Failed", "red");
        currentState = State::Idle;
        return;
    }

    if (currentState == State::SmartRead) {
        log("Step 2/3: Merging files...", "cyan");
        QFile flashFile("/tmp/readx.bin");
        QFile newFile(currentFile);
        QFile outFile("/tmp/tempx.bin");
        if (flashFile.open(QIODevice::ReadOnly) && newFile.open(QIODevice::ReadOnly) && outFile.open(QIODevice::WriteOnly)) {
            outFile.write(newFile.readAll());
            flashFile.seek(lastInfo.fileSize);
            outFile.write(flashFile.readAll());
            flashFile.close(); newFile.close(); outFile.close();
            
            QFile layout("/tmp/flashrom.layout");
            if (layout.open(QIODevice::WriteOnly)) {
                layout.write(QString("00000000:%1 flashzone").arg(lastInfo.fileSize - 1, 8, 16, QChar('0')).toUtf8());
                layout.close();
                log("Step 3/3: Writing to flashzone...", "cyan");
                currentState = State::SmartWrite;
                runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-l", "/tmp/flashrom.layout", "-i", "flashzone", "-w", "/tmp/tempx.bin"});
                return;
            }
        }
        log("Merge failed!", "red");
    } else if (currentState == State::SmartWrite) {
        log("Smart Write Successful!", "green");
        QFile::remove("/tmp/readx.bin");
        QFile::remove("/tmp/tempx.bin");
        QFile::remove("/tmp/flashrom.layout");
    } else {
        log("Operation Successful", "green");
    }
    currentState = State::Idle;
}

void MainWindow::log(const QString &msg, const QString &color) { ui->textLog->append(QString("<font color=\"%1\">%2</font>").arg(color, msg.toHtmlEscaped())); }
QString MainWindow::getProgrammerArgs() { return ui->comboProgrammer->currentData().toString(); }
