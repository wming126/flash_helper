#ifndef LOCALFLASHUTILS_H
#define LOCALFLASHUTILS_H

#include <QString>

namespace LocalFlash {

struct ValidationResult {
    bool ok = false;
    QString title;
    QString message;
};

bool parseDetectOutput(const QString &output, QString *chipLabel, qint64 *flashSize);
ValidationResult validateImageFile(const QString &path, qint64 flashSize);
QString failureMessage(int exitCode);

} // namespace LocalFlash

#endif // LOCALFLASHUTILS_H
