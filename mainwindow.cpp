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

    // 预创建数据目录
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    updateSystemStatus();
}

MainWindow::~MainWindow() { delete ui; }

// 安全的工作路径（位于用户家目录，避免权限冲突）
QString getWorkPath(const QString &name) {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + name;
}

void MainWindow::updateSystemStatus() {
    QString flashromPath = "/usr/sbin/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = "/usr/bin/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = QCoreApplication::applicationDirPath() + "/flashrom";

    QProcess check;
    check.start(flashromPath, {"-p", getProgrammerArgs()});
    check.waitForFinished(1000);
    QString out = check.readAllStandardError() + check.readAllStandardOutput();
    
    bool permissionDenied = out.contains("Permission denied") || out.contains("Access denied");
    bool udevFile = QFile::exists("/etc/udev/rules.d/z60_flashrom.rules");

    ui->lblUdevStatus->setText(permissionDenied ? "Hardware Access: <font color='red'>Denied (Requires Root)</font>" : "Hardware Access: <font color='green'>OK (Direct Access)</font>");
    ui->lblPolkitStatus->setText(udevFile ? "System Config: <font color='green'>Rules Installed</font>" : "System Config: <font color='red'>Not Configured</font>");
    ui->btnRemoveRules->setEnabled(udevFile);
}

void MainWindow::on_btnInstallRules_clicked() {
    QString udevContent = "# STM32 VSerprog\n"
                          "SUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"0483\", ATTRS{idProduct}==\"5740\", TAG+=\"uaccess\"\n"
                          "KERNEL==\"ttyACM*\", TAG+=\"uaccess\"\n"
                          "KERNEL==\"spidev*\", TAG+=\"uaccess\"\n";
    QString polkitRule = "polkit.addRule(function(action, subject) { "
                         "if (action.id == \"org.freedesktop.policykit.exec\" && "
                         "(action.lookup(\"program\") == \"/usr/sbin/flashrom\" || action.lookup(\"program\") == \"/usr/bin/flashrom\") && "
                         "subject.isInGroup(\"sudo\")) { return polkit.Result.YES; } });";

    QString script = QString("echo '%1' | base64 -d > /etc/udev/rules.d/z60_flashrom.rules && "
                             "echo '%2' | base64 -d > /etc/polkit-1/rules.d/10-flashrom.rules && "
                             "chmod 644 /etc/udev/rules.d/z60_flashrom.rules && "
                             "chmod 644 /etc/polkit-1/rules.d/10-flashrom.rules && "
                             "udevadm control --reload-rules && udevadm trigger")
                     .arg(QString(udevContent.toUtf8().toBase64()), QString(polkitRule.toUtf8().toBase64()));

    QProcess::execute("pkexec", {"bash", "-c", script});
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
    QString flashromPath = "/usr/sbin/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = "/usr/bin/flashrom";
    if (!QFile::exists(flashromPath)) flashromPath = QCoreApplication::applicationDirPath() + "/flashrom";

    if (cmd == "pkexec" && !args.isEmpty() && args[0] == "flashrom") {
        if (ui->lblUdevStatus->text().contains("OK")) {
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

void MainWindow::handleSmartWrite(const QString &error) {
    QRegularExpression re("(?:file|Image) size \\((\\d+) ?B?\\).*?(?:flash chip's|expected) size \\((\\d+) ?B?\\)");
    QRegularExpressionMatch match = re.match(error);
    if (match.hasMatch()) {
        lastInfo.fileSize = match.captured(1).toLong();
        lastInfo.flashSize = match.captured(2).toLong();
        if (lastInfo.fileSize < lastInfo.flashSize) {
            log("Size mismatch detected. Transitions to read mode.", "cyan");
            currentState = State::SmartRead;
        }
    }
}

void MainWindow::processFinished(int exitCode) {
    if (exitCode != 0) {
        // 如果是在 Writing 失败后进入的 SmartRead 阶段，触发读取
        if (currentState == State::SmartRead) {
            log("Smart Merge Flow: Step 1/3 - Reading current flash content...", "cyan");
            // 修正路径：使用安全的 getWorkPath
            QString targetPath = getWorkPath("readx.bin");
            QFile::remove(targetPath); // 确保没有残留
            runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-r", targetPath});
            currentState = State::Reading; // 暂时设为 Reading 以等待读取完成
            return;
        }
        
        log("Operation Failed", "red");
        currentState = State::Idle;
        return;
    }

    // exitCode == 0 的逻辑
    if (currentState == State::Reading && QFile::exists(getWorkPath("readx.bin"))) {
        // 如果当前是 Reading 状态，且我们预期的备份文件存在，说明 Step 1 结束了
        log("Step 2/3: Merging files...", "cyan");
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
                log("Step 3/3: Writing to flashzone...", "cyan");
                currentState = State::SmartWrite;
                runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-l", getWorkPath("flashrom.layout"), "-i", "flashzone", "-w", getWorkPath("tempx.bin")});
                return;
            }
        }
        log("Merge failed during file processing!", "red");
    } else if (currentState == State::SmartWrite) {
        log("Smart Write Successful!", "green");
        QFile::remove(getWorkPath("readx.bin")); QFile::remove(getWorkPath("tempx.bin")); QFile::remove(getWorkPath("flashrom.layout"));
    } else {
        log("Operation Successful", "green");
    }
    currentState = State::Idle;
}

void MainWindow::on_btnBrowse_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open BIOS File", "", "BIOS files (*.fd *.bin *.rom);;All files (*.*)");
    if (!fileName.isEmpty()) {
        ui->lineFile->setText(fileName);
        currentFile = fileName;
    }
}

void MainWindow::log(const QString &msg, const QString &color) { ui->textLog->append(QString("<font color=\"%1\">%2</font>").arg(color, msg.toHtmlEscaped())); }
QString MainWindow::getProgrammerArgs() { return ui->comboProgrammer->currentData().toString(); }
