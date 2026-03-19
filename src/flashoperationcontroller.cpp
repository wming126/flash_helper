#include "flashoperationcontroller.h"

#include <QDebug>

FlashOperationController::FlashOperationController(QObject *parent)
    : QObject(parent), m_process(new QProcess(this))
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &FlashOperationController::handleReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &FlashOperationController::handleReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &FlashOperationController::handleFinished);
}

FlashOperationController::~FlashOperationController()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished();
    }
}

void FlashOperationController::startFlashromOperation(State state, const QString &cmd, const QStringList &args)
{
    if (isBusy()) {
        emit logMessage(tr("An operation is already in progress."), "red");
        return;
    }

    m_accumulatedOutput.clear();
    m_accumulatedError.clear();
    setState(state);

    emit logMessage(tr("Executing: %1 %2").arg(cmd, args.join(" ")), "gray");
    m_process->start(cmd, args);
}

void FlashOperationController::startLocalHelperOperation(State state, const QString &helperPath, const QStringList &args)
{
    if (isBusy()) {
        emit logMessage(tr("An operation is already in progress."), "red");
        return;
    }

    m_accumulatedOutput.clear();
    m_accumulatedError.clear();
    setState(state);

    emit logMessage(tr("[Local Flash] Running: %1 %2").arg(helperPath, args.join(" ")), "cyan");
    m_process->start("pkexec", QStringList() << helperPath << args);
}

void FlashOperationController::abort()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished();
        setState(State::Idle);
    }
}

void FlashOperationController::handleReadyReadStandardOutput()
{
    const QByteArray data = m_process->readAllStandardOutput();
    const QString output = QString::fromUtf8(data);
    m_accumulatedOutput += output;
    emit outputReceived(output);
}

void FlashOperationController::handleReadyReadStandardError()
{
    const QByteArray data = m_process->readAllStandardError();
    const QString error = QString::fromUtf8(data);
    m_accumulatedError += error;
    emit errorReceived(error);
}

void FlashOperationController::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    State finishedState = m_currentState;
    setState(State::Idle);
    emit finished(exitCode, exitStatus, finishedState);
}

void FlashOperationController::setState(State newState)
{
    if (m_currentState != newState) {
        m_currentState = newState;
        emit stateChanged(m_currentState);
    }
}
