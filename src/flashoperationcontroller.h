#ifndef FLASHOPERATIONCONTROLLER_H
#define FLASHOPERATIONCONTROLLER_H

#include <QObject>
#include <QProcess>
#include <QStringList>

class FlashOperationController : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Detecting,
        Reading,
        Writing,
        Erasing,
        SmartRead,
        SmartWrite,
        EepromRead,
        EepromWrite,
        EepromErase,
        LocalDetect,
        LocalRead,
        LocalWrite
    };

    explicit FlashOperationController(QObject *parent = nullptr);
    ~FlashOperationController();

    State currentState() const { return m_currentState; }
    bool isBusy() const { return m_currentState != State::Idle; }

    void startFlashromOperation(State state, const QString &cmd, const QStringList &args);
    void startLocalHelperOperation(State state, const QString &helperPath, const QStringList &args);
    void abort();

    QString accumulatedOutput() const { return m_accumulatedOutput; }
    QString accumulatedError() const { return m_accumulatedError; }

signals:
    void stateChanged(State newState);
    void outputReceived(const QString &output);
    void errorReceived(const QString &error);
    void finished(int exitCode, QProcess::ExitStatus exitStatus, State finishedState);
    void logMessage(const QString &message, const QString &color = "white");

private slots:
    void handleReadyReadStandardOutput();
    void handleReadyReadStandardError();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess *m_process;
    State m_currentState = State::Idle;
    QString m_accumulatedOutput;
    QString m_accumulatedError;

    void setState(State newState);
};

#endif // FLASHOPERATIONCONTROLLER_H
