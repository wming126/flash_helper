#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDataStream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    process = new QProcess(this);
    
    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::readProcessOutput);
    connect(process, &QProcess::readyReadStandardError, this, &MainWindow::readProcessOutput);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            [=](int exitCode, QProcess::ExitStatus exitStatus){ processFinished(exitCode); });

    ui->comboProgrammer->addItem("Serprog (STM32)", "serprog:dev=/dev/ttyACM0:4000000,spispeed=36000000");
    ui->comboProgrammer->addItem("Linux SPI", "linux_spi:dev=/dev/spidev1.0,spispeed=20000");
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::on_btnBrowse_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open BIOS File", "", "BIOS files (*.fd *.bin *.rom);;All files (*.*)");
    if (!fileName.isEmpty()) {
        ui->lineFile->setText(fileName);
        currentFile = fileName;
    }
}

QString MainWindow::getProgrammerArgs() {
    return ui->comboProgrammer->currentData().toString();
}

void MainWindow::log(const QString &msg, const QString &color) {
    ui->textLog->append(QString("<font color=\"%1\">%2</font>").arg(color, msg.toHtmlEscaped()));
}

void MainWindow::runCommand(const QString &cmd, const QStringList &args) {
    QStringList finalArgs = args;
    QString finalCmd = cmd;
    
    // 如果调用的是 pkexec，确保后续程序使用完整路径
    if (cmd == "pkexec" && !args.isEmpty() && args[0] == "flashrom") {
        QString fullPath = "/usr/sbin/flashrom";
        if (!QFile::exists(fullPath)) fullPath = "/usr/bin/flashrom";
        finalArgs[0] = fullPath;
    }
    
    ui->textLog->append(QString("<b>Running: %1 %2</b>").arg(finalCmd, finalArgs.join(" ")));
    process->start(finalCmd, finalArgs);
}

void MainWindow::on_btnDetect_clicked() {
    currentState = State::Detecting;
    runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs()});
}

void MainWindow::on_btnRead_clicked() {
    QString savePath = QFileDialog::getSaveFileName(this, "Save BIOS File", "backup.bin", "BIOS files (*.bin *.fd);;All files (*.*)");
    if (savePath.isEmpty()) return;
    currentState = State::Reading;
    runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-r", savePath});
}

void MainWindow::on_btnErase_clicked() {
    if (QMessageBox::question(this, "Confirm", "Are you sure you want to ERASE the flash?") != QMessageBox::Yes) return;
    currentState = State::Erasing;
    runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-E"});
}

void MainWindow::on_btnWrite_clicked() {
    currentFile = ui->lineFile->text();
    if (!QFile::exists(currentFile)) {
        QMessageBox::critical(this, "Error", "File not found!");
        return;
    }
    currentState = State::Writing;
    runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-w", currentFile});
}

void MainWindow::readProcessOutput() {
    QString output = process->readAllStandardOutput();
    QString error = process->readAllStandardError();
    if (!output.isEmpty()) log(output, "white");
    if (!error.isEmpty()) log(error, "yellow");
    
    if (currentState == State::Writing && error.contains("doesn't match the")) {
        handleSmartWrite(error);
    }
}

void MainWindow::handleSmartWrite(const QString &output) {
    // 支持两种格式:
    // 1. file size (123 bytes) ... flash chip's size (456 bytes)
    // 2. Image size (123 B) doesn't match the expected size (456 B)
    QRegularExpression re("(?:file|Image) size \\((\\d+) ?B?\\).*?(?:flash chip's|expected) size \\((\\d+) ?B?\\)");
    QRegularExpressionMatch match = re.match(output);
    if (match.hasMatch()) {
        lastInfo.fileSize = match.captured(1).toLong();
        lastInfo.flashSize = match.captured(2).toLong();
        
        if (lastInfo.fileSize >= lastInfo.flashSize) {
            log("Error: File is larger than flash capacity!", "red");
            process->kill();
            return;
        }
        
        log(QString("Size mismatch: File(%1) Flash(%2). Starting smart merge...").arg(lastInfo.fileSize).arg(lastInfo.flashSize), "cyan");
        currentState = State::SmartRead;
        // 注意：不在这里调用 process->kill()，因为 process 可能已经自然结束或我们需要它触发 finished 信号
    }
}

void MainWindow::processFinished(int exitCode) {
    if (exitCode == 0) {
        if (currentState == State::SmartRead) {
            log("Read finished, merging files...", "cyan");
            // Perform merge: currentFile + tail of /tmp/readx.bin
            QFile flashFile("/tmp/readx.bin");
            QFile newFile(currentFile);
            QFile outFile("/tmp/tempx.bin");
            
            if (flashFile.open(QIODevice::ReadOnly) && newFile.open(QIODevice::ReadOnly) && outFile.open(QIODevice::WriteOnly)) {
                outFile.write(newFile.readAll());
                flashFile.seek(lastInfo.fileSize);
                outFile.write(flashFile.readAll());
                
                flashFile.close(); newFile.close(); outFile.close();
                
                // Create layout
                QFile layout("/tmp/flashrom.layout");
                if (layout.open(QIODevice::WriteOnly)) {
                    QString layoutContent = QString("00000000:%1 flashzone").arg(lastInfo.fileSize - 1, 8, 16, QChar('0'));
                    layout.write(layoutContent.toUtf8());
                    layout.close();
                    
                    log("Merge complete. Writing to flashzone...", "cyan");
                    currentState = State::SmartWrite;
                    runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-l", "/tmp/flashrom.layout", "-i", "flashzone", "-w", "/tmp/tempx.bin"});
                    return;
                }
            }
            log("Failed to merge files!", "red");
        } else {
            log("Operation Successful", "green");
        }
    } else {
        if (currentState == State::Writing) {
            // Already handled by smart write trigger
        } else if (currentState == State::SmartRead) {
             // Fallback to reading for smart write
             runCommand("pkexec", {"flashrom", "-p", getProgrammerArgs(), "-r", "/tmp/readx.bin"});
             return;
        } else {
            log("Operation Failed", "red");
        }
    }
    currentState = State::Idle;
}
