#include "chippreviewwidget.h"
#include <QPainter>
#include <QPaintEvent>

ChipPreviewWidget::ChipPreviewWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(180, 200);
}

void ChipPreviewWidget::setChipModel(const QString &model) {
    m_model = model;
    m_type = detectType(model);
    update();
}

void ChipPreviewWidget::setPinoutType(PinoutType type) {
    m_type = type;
    update();
}

ChipPreviewWidget::PinoutType ChipPreviewWidget::detectType(const QString &model) {
    if (model.isEmpty() || model.contains("Auto Detect")) return SPI_8PIN;
    
    QString m = model.toUpper();
    if (m.contains("25") || m.contains("26") || m.contains("SPI")) {
        if (m.contains("SOP16") || m.contains("16P") || m.contains("SOIC16")) return SPI_16PIN;
        if (m.contains("WSON") || m.contains("DFN") || m.contains("QFN")) return WSON8;
        return SPI_8PIN;
    }
    if (m.contains("24") || m.contains("I2C") || m.contains("EEPROM")) return I2C_8PIN;
    
    return SPI_8PIN; // Default to common
}

QSize ChipPreviewWidget::sizeHint() const {
    return QSize(180, 200);
}

void ChipPreviewWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    // Draw Background
    p.fillRect(rect(), palette().window());
    
    // Pin Definitions
    QStringList pins;
    switch (m_type) {
        case SPI_8PIN:
        case WSON8:
            pins << "CS" << "SO/IO1" << "WP/IO2" << "GND" << "VCC" << "HOLD/IO3" << "CLK" << "SI/IO0";
            draw8Pin(p, pins, m_type == WSON8);
            break;
        case SPI_16PIN:
            pins << "HOLD/IO3" << "VCC" << "NC" << "NC" << "NC" << "NC" << "CS" << "SO/IO1"
                 << "SI/IO0" << "CLK" << "NC" << "NC" << "NC" << "NC" << "WP/IO2" << "GND";
            draw16Pin(p, pins);
            break;
        case I2C_8PIN:
            pins << "A0" << "A1" << "A2" << "GND" << "VCC" << "WP" << "SCL" << "SDA";
            draw8Pin(p, pins);
            break;
        default:
            p.drawText(rect(), Qt::AlignCenter, "No Pinout Info");
            break;
    }
    
    // Draw Title
    p.setPen(palette().text().color());
    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    QString title = (m_type == WSON8) ? "WSON8 (8-Pin Flat)" : 
                    (m_type == SPI_16PIN) ? "SOP16 (16-Pin)" : "SOP8 / DIP8";
    p.drawText(QRect(0, height() - 30, width(), 20), Qt::AlignCenter, title);
}

void ChipPreviewWidget::draw8Pin(QPainter &p, const QStringList &pins, bool isFlat) {
    int w = 60;
    int h = 80;
    int centerX = width() / 2;
    int centerY = height() / 2 - 10;
    
    QRect body(centerX - w/2, centerY - h/2, w, h);
    
    // Draw Chip Body
    p.setBrush(QColor(40, 40, 40));
    p.setPen(QPen(Qt::gray, 2));
    p.drawRect(body);
    
    // Draw Notch/Dot
    p.setBrush(Qt::black);
    p.drawEllipse(body.topLeft() + QPoint(10, 10), 4, 4);
    
    // Draw Pins
    int pinSpacing = h / 4;
    int pinW = isFlat ? 4 : 15;
    int pinH = 8;
    
    QFont pinFont = font();
    pinFont.setPointSize(8);
    p.setFont(pinFont);
    
    for (int i = 0; i < 4; ++i) {
        // Left side (1-4)
        int py = body.top() + pinSpacing/2 + i * pinSpacing - pinH/2;
        QRect leftPin(body.left() - pinW, py, pinW, pinH);
        p.setBrush(Qt::lightGray);
        p.drawRect(leftPin);
        
        p.setPen(Qt::blue);
        p.drawText(leftPin.right() + 5, leftPin.center().y() + 4, QString::number(i + 1));
        p.setPen(palette().text().color());
        p.drawText(QRect(0, leftPin.top() - 5, body.left() - 20, 20), Qt::AlignRight | Qt::AlignVCenter, pins[i]);
        
        // Right side (8-5)
        int rIdx = 7 - i;
        QRect rightPin(body.right(), py, pinW, pinH);
        p.setBrush(Qt::lightGray);
        p.drawRect(rightPin);
        
        p.setPen(Qt::blue);
        p.drawText(rightPin.left() - 12, rightPin.center().y() + 4, QString::number(rIdx + 1));
        p.setPen(palette().text().color());
        p.drawText(QRect(body.right() + 20, rightPin.top() - 5, width() - body.right() - 20, 20), Qt::AlignLeft | Qt::AlignVCenter, pins[rIdx]);
    }
}

void ChipPreviewWidget::draw16Pin(QPainter &p, const QStringList &pins) {
    int w = 60;
    int h = 140;
    int centerX = width() / 2;
    int centerY = height() / 2 - 10;
    
    QRect body(centerX - w/2, centerY - h/2, w, h);
    p.setBrush(QColor(40, 40, 40));
    p.setPen(QPen(Qt::gray, 2));
    p.drawRect(body);
    p.setBrush(Qt::black);
    p.drawEllipse(body.topLeft() + QPoint(8, 8), 4, 4);
    
    int pinSpacing = h / 8;
    int pinW = 12;
    int pinH = 6;
    
    QFont pinFont = font();
    pinFont.setPointSize(7);
    p.setFont(pinFont);
    
    for (int i = 0; i < 8; ++i) {
        int py = body.top() + pinSpacing/2 + i * pinSpacing - pinH/2;
        
        // Left (1-8)
        QRect lp(body.left() - pinW, py, pinW, pinH);
        p.setBrush(Qt::lightGray); p.drawRect(lp);
        p.setPen(Qt::blue); p.drawText(lp.right() + 2, lp.center().y() + 3, QString::number(i + 1));
        p.setPen(palette().text().color());
        p.drawText(QRect(0, lp.top() - 7, body.left() - 15, 20), Qt::AlignRight | Qt::AlignVCenter, pins[i]);
        
        // Right (16-9)
        int rIdx = 15 - i;
        QRect rp(body.right(), py, pinW, pinH);
        p.setBrush(Qt::lightGray); p.drawRect(rp);
        p.setPen(Qt::blue); p.drawText(rp.left() - 10, rp.center().y() + 3, QString::number(rIdx + 1));
        p.setPen(palette().text().color());
        p.drawText(QRect(body.right() + 15, rp.top() - 7, width() - body.right() - 15, 20), Qt::AlignLeft | Qt::AlignVCenter, pins[rIdx]);
    }
}
