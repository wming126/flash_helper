#include "localflashutils.h"

#include "localflashshared.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace LocalFlash {

bool parseDetectOutput(const QString &output, QString *chipLabel, qint64 *flashSize) {
    QRegularExpression re("SUCCESS: ([0-9A-F]+) ([0-9A-F]+) ([0-9A-F]+)");
    const QRegularExpressionMatch match = re.match(output);
    if (!match.hasMatch()) {
        return false;
    }

    if (chipLabel) {
        *chipLabel = QString("ID: %1 %2 %3").arg(match.captured(1), match.captured(2), match.captured(3));
    }
    if (flashSize) {
        const uint8_t capacityCode = match.captured(3).toUInt(nullptr, 16);
        *flashSize = static_cast<qint64>(flashSizeFromCapacityCode(capacityCode));
    }
    return true;
}

ValidationResult validateImageFile(const QString &path, qint64 flashSize) {
    if (path.isEmpty()) {
        return {false, QObject::tr("Error"), QObject::tr("Please select a file first.")};
    }

    if (flashSize <= 0) {
        return {false,
                QObject::tr("Flash Size Unknown"),
                QObject::tr("Unable to determine local flash size. Detect the chip first.")};
    }

    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || fileInfo.size() <= 0) {
        return {false,
                QObject::tr("Error"),
                QObject::tr("The selected local file is missing or empty.")};
    }

    if (fileInfo.size() != flashSize) {
        return {false,
                QObject::tr("Image Size Mismatch"),
                QObject::tr("The selected image is %1 bytes, but the detected flash size is %2 bytes.\n\n"
                            "Local flashing requires an exact size match.")
                    .arg(fileInfo.size())
                    .arg(flashSize)};
    }

    return {true, QString(), QString()};
}

QString failureMessage(int exitCode) {
    switch (static_cast<HelperExitCode>(exitCode)) {
        case HelperExitCode::InitFailed:
            return QObject::tr("Failed to initialize the local SPI driver. Check root access and platform support.");
        case HelperExitCode::DetectFailed:
            return QObject::tr("No local SPI flash chip was detected.");
        case HelperExitCode::InputOpenFailed:
            return QObject::tr("Failed to open the selected local image file.");
        case HelperExitCode::ReadFailed:
            return QObject::tr("Failed to read the local SPI flash contents.");
        case HelperExitCode::OutputOpenFailed:
            return QObject::tr("Failed to open the temporary backup file for writing.");
        case HelperExitCode::OutputWriteFailed:
            return QObject::tr("Failed to save the temporary backup data.");
        case HelperExitCode::InputReadFailed:
            return QObject::tr("Failed to read the selected local image file.");
        case HelperExitCode::EraseFailed:
            return QObject::tr("Failed to erase the local SPI flash.");
        case HelperExitCode::WriteFailed:
            return QObject::tr("Failed to write the local SPI flash.");
        case HelperExitCode::InvalidExpectedSize:
            return QObject::tr("The helper received an invalid expected flash size.");
        case HelperExitCode::EmptyImage:
            return QObject::tr("The selected local image is empty.");
        case HelperExitCode::ImageSizeMismatch:
            return QObject::tr("The selected local image size does not match the detected flash size.");
        default:
            return QObject::tr("Local flash operation failed.");
    }
}

} // namespace LocalFlash
