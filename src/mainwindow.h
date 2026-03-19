#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QStringListModel>
#include <QFile>
#include <QSet>
#include <QTranslator>
#include "localspidriver.h"

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
    
    // Local Flash Slots
    void on_btnLocalDetect_clicked();
    void on_btnLocalRead_clicked();
    void on_btnLocalWrite_clicked();
    void on_btnLocalBrowse_clicked();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class State { Idle, Detecting, Reading, Writing, Erasing, SmartRead, SmartWrite, EepromRead, EepromWrite, EepromErase, LocalDetect, LocalRead, LocalWrite };

    Ui::MainWindow *ui;
    QProcess *process;
    QTranslator *translator;
    QTimer *idleTimer;
    QString currentFile;
    QString eepromFile;
    QString localFile;
    QStringList supportedChips;
    class ChipPreviewWidget *chipPreview;
    class QGroupBox *previewGroup;
    LocalSpiDriver *localSpi;

    void lockUi(bool locked);
    void updateSystemStatus();
    void fetchSupportedChips();
    void refreshDeviceList();
    void updateTabHeight();
    QString getProgrammerArgs(bool isEeprom = false) const;
    QString getFlashromPath();
    void log(const QString &msg, const QString &color = "white");
    void runCommand(const QString &cmd, const QStringList &args);
    bool runLocalHelper(const QStringList &args);
    QString getWorkPath(const QString &fileName);
    QString ensureWorkDir();
    QString getHelperPath() const;
    void loadDataToEditor(const QByteArray &data);
    QString prepareWriteFile();
    void clearSmartWriteArtifacts();
    void cleanupWorkDir();
    void maybeResetSmartWriteArtifacts();
    QStringList applySelectedChipToArgs(QStringList args) const;
    bool canRunFlashromDirectly(const QString &programPath) const;
    QString preparePrivilegedExecutable(QString executablePath) const;
    QString statusMessageForState(State state) const;
    bool retryOperationWithDetectedChip(const QString &combinedOutput);
    QStringList buildRetryArgsForState(State state, const QString &chipName);
    bool handleFailedProcess();
    bool handleSuccessfulProcess();
    bool handleSuccessfulLocalDetect();
    bool handleSuccessfulLocalRead();
    bool handleSuccessfulLocalWrite();
    void handleSuccessfulDetect();
    bool handleSmartReadFailure();
    bool handleSmartMergeSuccess();
    void loadReadResultIntoEditor();

    State currentState = State::Idle;
    QStringList detectedChips;
    QString accumulatedError;
    QString accumulatedOutput;
    QString localSavePath;
    QString workDirPath;
    bool smartWritePending = false;

    struct FlashInfo {        long flashSize = 0;
        long fileSize = 0;
    };
    FlashInfo lastInfo;

    void handleSmartWrite(const QString &output);
};
#endif // MAINWINDOW_H
