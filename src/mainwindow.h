#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QStringListModel>
#include <QFile>
#include <QSet>
#include <QTranslator>
#include "localspidriver.h"
#include "localflashutils.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QLabel;

#include "flashoperationcontroller.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    using State = FlashOperationController::State;
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
    void processFinished(int exitCode, State finishedState);
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
    Ui::MainWindow *ui;
    FlashOperationController *flashController;
    QTranslator *translator;
    QTimer *idleTimer;
    QString currentFile;
    QString eepromFile;
    QString localFile;
    QStringList supportedChips;
    class ChipPreviewWidget *chipPreview;
    class QGroupBox *previewGroup;
    QLabel *aboutVersionLabel;
    QLabel *aboutInstructionsLabel;
    LocalSpiDriver *localSpi;
    class LocalFlashManager *localFlashManager;

    void lockUi(bool locked);
    void updateSystemStatus();
    void fetchSupportedChips();
    void refreshDeviceList();
    void updateTabHeight();
    void setupPreviewPanels();
    void setupAboutPage();
    void setupProgrammerOptions();
    void initializeLanguageSelection();
    void updateDynamicTexts();
    QString getProgrammerArgs(bool isEeprom = false) const;
    QString getFlashromPath();
    void log(const QString &msg, const QString &color = "white");
    void runCommand(State state, const QString &cmd, const QStringList &args);
    bool runLocalHelper(State state, const QStringList &args);
    QString getWorkPath(const QString &fileName);
    QString ensureWorkDir();
    QString getHelperPath() const;
    void loadDataToEditor(const QByteArray &data);
    bool loadFileIntoEditor(const QString &path, const QString &errorTitle, const QString &successMessage = QString());
    bool writeEditorDataToFile(const QString &path, const QString &emptyTitle, const QString &emptyMessage,
                               const QString &errorTitle, const QString &errorMessage);
    QString prepareWriteFile();
    bool startFlashromOperation(State state, const QStringList &args, bool lockTabs = false);
    void beginBusyOperation(const QString &statusMessage, bool lockTabs = false, bool showProgress = false);
    void finishBusyOperation(bool startIdleStatusTimer);
    void abortBusyOperation();
    void clearSmartWriteArtifacts();
    void cleanupWorkDir();
    void maybeResetSmartWriteArtifacts();
    QStringList applySelectedChipToArgs(QStringList args) const;
    bool canRunFlashromDirectly(const QString &programPath) const;
    QString preparePrivilegedExecutable(QString executablePath) const;
    QString statusMessageForState(State state) const;
    bool retryOperationWithDetectedChip(const QString &combinedOutput, State finishedState);
    QStringList buildRetryArgsForState(State state, const QString &chipName);
    bool handleFailedProcess(int exitCode, State finishedState);
    bool handleSuccessfulProcess(State finishedState);
    void cleanupLocalReadArtifact();
    void handleSuccessfulDetect();
    bool handleSmartReadFailure();
    bool handleSmartMergeSuccess();
    void startSmartMergeWrite();
    void loadReadResultIntoEditor();

    State currentState() const { return flashController->currentState(); }

    QStringList detectedChips;
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
