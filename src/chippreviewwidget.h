#ifndef CHIPPREVIEWWIDGET_H
#define CHIPPREVIEWWIDGET_H

#include <QWidget>
#include <QStringList>

class ChipPreviewWidget : public QWidget {
    Q_OBJECT
public:
    enum PinoutType {
        None,
        SPI_8PIN,   // Standard 25 series
        SPI_16PIN,  // SOP16
        I2C_8PIN,   // 24 series
        WSON8       // 8 pin but flat
    };

    explicit ChipPreviewWidget(QWidget *parent = nullptr);

    void setChipModel(const QString &model);
    void setPinoutType(PinoutType type);
    PinoutType currentType() const { return m_type; }
    QSize sizeHint() const override;

signals:
    void typeChanged(PinoutType type);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    PinoutType m_type = SPI_8PIN;
    QString m_model;
    
    void draw8Pin(QPainter &p, const QStringList &pins, bool isFlat = false);
    void draw16Pin(QPainter &p, const QStringList &pins);
    
    PinoutType detectType(const QString &model);
};

#endif // CHIPPREVIEWWIDGET_H
