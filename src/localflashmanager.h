#ifndef LOCALFLASHMANAGER_H
#define LOCALFLASHMANAGER_H

#include <QObject>
#include "flashoperationcontroller.h"
#include "localspidriver.h"

class LocalFlashManager : public QObject
{
    Q_OBJECT

public:
    explicit LocalFlashManager(FlashOperationController *controller, LocalSpiDriver *spi, QObject *parent = nullptr);

    bool handleSuccessfulDetect(FlashOperationController::State finishedState);
    bool handleSuccessfulRead(FlashOperationController::State finishedState, const QString &savePath, const QString &tempFile);
    bool handleSuccessfulWrite(FlashOperationController::State finishedState);
    bool handleFailedOperation(FlashOperationController::State finishedState, int exitCode);

signals:
    void chipDetected(const QString &info, qint64 size);
    void detectUnknownSize(const QString &info);
    void logMessage(const QString &message, const QString &color = "white");
    void statusMessage(const QString &message, int timeout = 0);
    void operationFailed(const QString &message);

private:
    FlashOperationController *m_controller;
    LocalSpiDriver *m_spi;
};

#endif // LOCALFLASHMANAGER_H
