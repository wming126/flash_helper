#include "smartmerge.h"

#include <QByteArray>
#include <QChar>
#include <QFile>
#include <QObject>

namespace SmartMerge {

namespace {

bool writeLayoutFile(const QString &layoutPath, qint64 imageSize, QString *errorMessage) {
    QFile layout(layoutPath);
    if (!layout.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = QObject::tr("Failed to create the Smart Merge layout file.");
        return false;
    }

    const QByteArray layoutData =
        QString("00000000:%1 flashzone").arg(imageSize - 1, 8, 16, QChar('0')).toUtf8();
    if (layout.write(layoutData) != layoutData.size()) {
        if (errorMessage) *errorMessage = QObject::tr("Failed to write the Smart Merge layout file.");
        return false;
    }

    layout.close();
    return true;
}

} // namespace

bool preparePartialWrite(const QString &readbackPath,
                         const QString &inputPath,
                         const QString &mergedPath,
                         const QString &layoutPath,
                         qint64 imageSize,
                         QString *errorMessage) {
    QFile flashFile(readbackPath);
    QFile newFile(inputPath);
    QFile outFile(mergedPath);
    if (!flashFile.open(QIODevice::ReadOnly) ||
        !newFile.open(QIODevice::ReadOnly) ||
        !outFile.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = QObject::tr("Failed to open Smart Merge working files.");
        return false;
    }

    const QByteArray newImage = newFile.readAll();
    if (outFile.write(newImage) != newImage.size()) {
        if (errorMessage) *errorMessage = QObject::tr("Failed to write the Smart Merge image prefix.");
        return false;
    }

    flashFile.seek(imageSize);
    const QByteArray preservedTail = flashFile.readAll();
    if (outFile.write(preservedTail) != preservedTail.size()) {
        if (errorMessage) *errorMessage = QObject::tr("Failed to append the preserved flash tail.");
        return false;
    }

    flashFile.close();
    newFile.close();
    outFile.close();
    return writeLayoutFile(layoutPath, imageSize, errorMessage);
}

} // namespace SmartMerge
