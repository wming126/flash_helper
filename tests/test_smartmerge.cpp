#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include "src/smartmerge.h"

class TestSmartMerge : public QObject
{
    Q_OBJECT

private slots:
    void testShortImageMergeSuccess();
    void testMissingReadbackFile();
    void testNoTailBytesCase();
};

void TestSmartMerge::testShortImageMergeSuccess()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString readbackPath = dir.path() + "/readback.bin";
    QString inputPath = dir.path() + "/input.bin";
    QString mergedPath = dir.path() + "/merged.bin";
    QString layoutPath = dir.path() + "/layout.txt";

    // 1. Create readback (8 bytes)
    QFile readback(readbackPath);
    QVERIFY(readback.open(QIODevice::WriteOnly));
    readback.write(QByteArray(8, 'R'));
    readback.close();

    // 2. Create input (4 bytes)
    QFile input(inputPath);
    QVERIFY(input.open(QIODevice::WriteOnly));
    input.write(QByteArray(4, 'I'));
    input.close();

    // 3. Merge: 4 bytes from input + 4 bytes tail from readback
    QString error;
    bool ok = SmartMerge::preparePartialWrite(readbackPath, inputPath, mergedPath, layoutPath, 4, &error);
    QVERIFY(ok);
    QVERIFY(error.isEmpty());

    // 4. Verify merged
    QFile merged(mergedPath);
    QVERIFY(merged.open(QIODevice::ReadOnly));
    QByteArray data = merged.readAll();
    QCOMPARE(data.size(), 8);
    QCOMPARE(data.mid(0, 4), QByteArray(4, 'I'));
    QCOMPARE(data.mid(4, 4), QByteArray(4, 'R'));
    merged.close();

    // 5. Verify layout
    QFile layout(layoutPath);
    QVERIFY(layout.open(QIODevice::ReadOnly));
    QByteArray layoutData = layout.readAll();
    QCOMPARE(layoutData, QByteArray("00000000:00000003 flashzone"));
}

void TestSmartMerge::testMissingReadbackFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString readbackPath = dir.path() + "/missing.bin";
    QString inputPath = dir.path() + "/input.bin";
    QString mergedPath = dir.path() + "/merged.bin";
    QString layoutPath = dir.path() + "/layout.txt";

    QFile input(inputPath);
    QVERIFY(input.open(QIODevice::WriteOnly));
    input.write("some data");
    input.close();

    QString error;
    bool ok = SmartMerge::preparePartialWrite(readbackPath, inputPath, mergedPath, layoutPath, 9, &error);
    QVERIFY(!ok);
    QVERIFY(!error.isEmpty());
}

void TestSmartMerge::testNoTailBytesCase()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString readbackPath = dir.path() + "/readback.bin";
    QString inputPath = dir.path() + "/input.bin";
    QString mergedPath = dir.path() + "/merged.bin";
    QString layoutPath = dir.path() + "/layout.txt";

    QFile readback(readbackPath);
    QVERIFY(readback.open(QIODevice::WriteOnly));
    readback.write(QByteArray(4, 'R'));
    readback.close();

    QFile input(inputPath);
    QVERIFY(input.open(QIODevice::WriteOnly));
    input.write(QByteArray(4, 'I'));
    input.close();

    QString error;
    bool ok = SmartMerge::preparePartialWrite(readbackPath, inputPath, mergedPath, layoutPath, 4, &error);
    QVERIFY(ok);

    QFile merged(mergedPath);
    QVERIFY(merged.open(QIODevice::ReadOnly));
    QByteArray data = merged.readAll();
    QCOMPARE(data, QByteArray(4, 'I'));
}

QTEST_MAIN(TestSmartMerge)
#include "test_smartmerge.moc"
