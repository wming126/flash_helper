#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QStringListModel>
#include <QFile>

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

private:
    Ui::MainWindow *ui;
    QProcess *process;
    QString currentFile;

    void updateSystemStatus();
    QString getProgrammerArgs();
    void log(const QString &msg, const QString &color = "white");
    void runCommand(const QString &cmd, const QStringList &args);
    
    // Internal state for smart write
    enum class State { Idle, Detecting, Reading, Writing, Erasing, SmartRead, SmartWrite };
    State currentState = State::Idle;
    
    struct FlashInfo {
        long flashSize = 0;
        long fileSize = 0;
    };
    FlashInfo lastInfo;

    void handleSmartWrite(const QString &output);
};
#endif // MAINWINDOW_H
