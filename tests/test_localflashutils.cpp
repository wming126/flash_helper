#include <QtTest/QtTest>
#include "src/localflashutils.h"
#include "src/localflashshared.h"

class TestLocalFlashUtils : public QObject
{
    Q_OBJECT

private slots:
    void testParseDetectOutput_Success();
    void testParseDetectOutput_Failure();
    void testValidateImageFile();
    void testFailureMessage();
};

void TestLocalFlashUtils::testParseDetectOutput_Success()
{
    QString chipLabel;
    qint64 flashSize = 0;
    QString output = "SUCCESS: 9F 70 18"; // Winbond 25Q128 (16MB)
    bool ok = LocalFlash::parseDetectOutput(output, &chipLabel, &flashSize);

    QVERIFY(ok);
    QCOMPARE(chipLabel, QString("ID: 9F 70 18"));
    QCOMPARE(flashSize, static_cast<qint64>(16 * 1024 * 1024));
}

void TestLocalFlashUtils::testParseDetectOutput_Failure()
{
    QString chipLabel;
    qint64 flashSize = 0;
    QString output = "ERROR: No chip found";
    bool ok = LocalFlash::parseDetectOutput(output, &chipLabel, &flashSize);

    QVERIFY(!ok);
}

void TestLocalFlashUtils::testValidateImageFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString validPath = dir.path() + "/valid.bin";
    QFile validFile(validPath);
    validFile.open(QIODevice::WriteOnly);
    validFile.write(QByteArray(1024, '0'));
    validFile.close();

    // 1. Valid
    auto res = LocalFlash::validateImageFile(validPath, 1024);
    QVERIFY(res.ok);

    // 2. Mismatch size
    res = LocalFlash::validateImageFile(validPath, 2048);
    QVERIFY(!res.ok);
    QCOMPARE(res.title, QString("Image Size Mismatch"));

    // 3. Unknown flash size
    res = LocalFlash::validateImageFile(validPath, 0);
    QVERIFY(!res.ok);
    QCOMPARE(res.title, QString("Flash Size Unknown"));

    // 4. Missing file
    res = LocalFlash::validateImageFile(dir.path() + "/missing.bin", 1024);
    QVERIFY(!res.ok);
    QCOMPARE(res.title, QString("Error"));
}

void TestLocalFlashUtils::testFailureMessage()
{
    QString msg = LocalFlash::failureMessage(static_cast<int>(LocalFlash::HelperExitCode::InitFailed));
    QVERIFY(msg.contains("initialize"));

    msg = LocalFlash::failureMessage(static_cast<int>(LocalFlash::HelperExitCode::DetectFailed));
    QVERIFY(msg.contains("No local SPI flash chip"));

    msg = LocalFlash::failureMessage(999); // Unknown
    QVERIFY(msg.contains("failed"));
}

QTEST_MAIN(TestLocalFlashUtils)
#include "test_localflashutils.moc"
