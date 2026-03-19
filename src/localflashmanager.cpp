#include "localflashmanager.h"
#include "localflashutils.h"
#include <QFile>

LocalFlashManager::LocalFlashManager(FlashOperationController *controller, LocalSpiDriver *spi, QObject *parent)
    : QObject(parent), m_controller(controller), m_spi(spi)
{
}

bool LocalFlashManager::handleSuccessfulDetect(FlashOperationController::State finishedState)
{
    if (finishedState != FlashOperationController::State::LocalDetect) return false;

    QString info;
    qint64 flashSize = 0;
    if (!LocalFlash::parseDetectOutput(m_controller->accumulatedOutput(), &info, &flashSize)) return true;

    if (flashSize > 0) {
        m_spi->setFlashSize(static_cast<uint32_t>(flashSize));
        emit chipDetected(info, flashSize);
    } else {
        m_spi->setFlashSize(0);
        emit detectUnknownSize(info);
    }
    return true;
}

bool LocalFlashManager::handleSuccessfulRead(FlashOperationController::State finishedState, const QString &savePath, const QString &tempFile)
{
    if (finishedState != FlashOperationController::State::LocalRead) return false;

    if (!QFile::exists(tempFile)) return true;

    if (!savePath.isEmpty()) {
        QFile::remove(savePath);
        if (QFile::copy(tempFile, savePath)) {
            emit logMessage(tr("Flash backup saved to: %1").arg(savePath), "green");
        } else {
            emit logMessage(tr("Failed to save flash backup to: %1").arg(savePath), "red");
        }
    }
    return true;
}

bool LocalFlashManager::handleSuccessfulWrite(FlashOperationController::State finishedState)
{
    if (finishedState != FlashOperationController::State::LocalWrite) return false;

    if (m_controller->accumulatedOutput().contains("SUCCESS")) {
        emit logMessage(tr("Local write successfully completed!"), "green");
    } else {
        emit logMessage(tr("Local write finished without a success marker."), "yellow");
        emit statusMessage(tr("Local write result is uncertain"), 5000);
    }
    return true;
}

bool LocalFlashManager::handleFailedOperation(FlashOperationController::State finishedState, int exitCode)
{
    if (finishedState != FlashOperationController::State::LocalDetect &&
        finishedState != FlashOperationController::State::LocalRead &&
        finishedState != FlashOperationController::State::LocalWrite) {
        return false;
    }

    const QString message = LocalFlash::failureMessage(exitCode);
    if (!message.isEmpty()) {
        emit logMessage(message, "red");
        emit statusMessage(message, 7000);
        emit operationFailed(message);
    }
    return true;
}
