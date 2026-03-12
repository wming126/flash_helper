#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QStringListModel>
#include <QFile>
#include <QSet>
#include <QTranslator>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnBrowse_clicked();
    void on_btnDetect_clicked();
    void on_btnRead_clicked();
    void on_btnWrite_clicked();
    void on_btnErase_clicked();
    void on_btnInstallRules_clicked();
    void on_btnRemoveRules_clicked();
    void readProcessOutput();
    void processFinished(int exitCode);
    void showLogContextMenu(const QPoint &pos);
    void on_comboProgrammer_currentIndexChanged(int index);
    void on_btnEepromRead_clicked();
    void on_btnEepromWrite_clicked();
    void on_btnEepromErase_clicked();
    void on_btnSaveFile_clicked();
    void on_btnEepromBrowse_clicked();
    void on_comboLang_currentIndexChanged(int index);

private:
    Ui::MainWindow *ui;
    QProcess *process;
    QTranslator *translator;
    QString currentFile;
    QString eepromFile;
    QStringList supportedChips;

    void updateSystemStatus();
    void fetchSupportedChips();
    QString getProgrammerArgs(bool isEeprom = false);
    QString getFlashromPath();
    void log(const QString &msg, const QString &color = "white");
    void runCommand(const QString &cmd, const QStringList &args);
    void loadDataToEditor(const QByteArray &data);
    QString prepareWriteFile();
    
    // Internal state management
    enum class State { Idle, Detecting, Reading, Writing, Erasing, SmartRead, SmartWrite, EepromRead, EepromWrite, EepromErase };
    State currentState = State::Idle;
    QStringList detectedChips;
    QString accumulatedError;
    
    struct FlashInfo {
        long flashSize = 0;
        long fileSize = 0;
    };
    FlashInfo lastInfo;

    void handleSmartWrite(const QString &output);
};
#endif // MAINWINDOW_H
