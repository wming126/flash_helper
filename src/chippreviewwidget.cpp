#include "chippreviewwidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>

ChipPreviewWidget::ChipPreviewWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(220, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setToolTip(tr("Click to manually cycle package types (SOP8 -> SOP16 -> I2C -> WSON8)"));
}

void ChipPreviewWidget::setChipModel(const QString &model) {
    if (model == m_model) return;
    m_model = model;
    m_type = detectType(model);
    update();
}

void ChipPreviewWidget::setPinoutType(PinoutType type) {
    m_type = type;
    update();
}

void ChipPreviewWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        int next = (int)m_type + 1;
        if (next > (int)WSON8) next = (int)SPI_8PIN;
        m_type = (PinoutType)next;
        update();
    }
}

ChipPreviewWidget::PinoutType ChipPreviewWidget::detectType(const QString &model) {
    QString m = model.toUpper();
    if (m.isEmpty() || m.contains("AUTO DETECT")) return SPI_8PIN;
    
    if (m.contains("24") || m.contains("I2C") || m.contains("EEPROM")) return I2C_8PIN;
    if (m.contains("25") || m.contains("26") || m.contains("SPI") || m.contains("FLASH")) {
        if (m.contains("SOP16") || m.contains("16P") || m.contains("SOIC16")) return SPI_16PIN;
        if (m.contains("WSON") || m.contains("DFN") || m.contains("QFN") || m.contains("8X6") || m.contains("6X5")) return WSON8;
        if (m.contains("256") || m.contains("512") || m.contains("1G") || m.contains("2G")) return SPI_16PIN;
        return SPI_8PIN;
    }
    return SPI_8PIN;
}

QSize ChipPreviewWidget::sizeHint() const {
    return QSize(220, 240);
}

void ChipPreviewWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    p.setPen(QPen(Qt::lightGray, 1, Qt::DashLine));
    p.setBrush(palette().window());
    p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 8, 8);
    
    QStringList pins;
    switch (m_type) {
        case SPI_8PIN:
        case WSON8:
            pins << "CS" << "SO" << "WP" << "GND" << "VCC" << "HOLD" << "CLK" << "SI";
            draw8Pin(p, pins, m_type == WSON8);
            break;
        case SPI_16PIN:
            pins << "HOLD" << "VCC" << "NC" << "NC" << "NC" << "NC" << "CS" << "SO"
                 << "SI" << "CLK" << "NC" << "NC" << "NC" << "NC" << "WP" << "GND";
            draw16Pin(p, pins);
            break;
        case I2C_8PIN:
            pins << "A0" << "A1" << "A2" << "GND" << "VCC" << "WP" << "SCL" << "SDA";
            draw8Pin(p, pins);
            break;
        default: break;
    }
    
    p.setPen(palette().text().color());
    QFont f = font();
    f.setBold(true);
    f.setPointSize(9);
    p.setFont(f);
    QString title;
    if (m_type == WSON8) title = tr("WSON8 (Flat)");
    else if (m_type == SPI_16PIN) title = tr("SOP16 (16-Pin)");
    else if (m_type == I2C_8PIN) title = tr("I2C SOP8");
    else title = tr("SPI SOP8");
    
    p.drawText(QRect(0, height() - 25, width(), 20), Qt::AlignCenter, title);
}

void ChipPreviewWidget::draw8Pin(QPainter &p, const QStringList &pins, bool isFlat) {
    int w = 70; int h = 90;
    int centerX = width() / 2; int centerY = height() / 2 - 10;
    QRect body(centerX - w/2, centerY - h/2, w, h);
    
    p.setBrush(QColor(25, 25, 25)); p.setPen(QPen(Qt::gray, 2)); p.drawRect(body);
    p.setBrush(Qt::black); p.drawEllipse(body.topLeft() + QPoint(10, 10), 5, 5);
    
    p.setPen(QColor(180, 180, 180, 200));
    QFont modelFont = font(); modelFont.setPointSize(7); p.setFont(modelFont);
    QString label = m_model.isEmpty() ? tr("CHIP") : m_model.split(" ").last();
    if (label.length() > 10) label = label.left(8) + "..";
    p.drawText(body, Qt::AlignCenter, label);

    int pinSpacing = h / 4; int pinW = isFlat ? 6 : 18; int pinH = 10;
    for (int i = 0; i < 4; ++i) {
        int py = body.top() + pinSpacing/2 + i * pinSpacing - pinH/2;
        QRect lp(body.left() - pinW, py, pinW, pinH);
        p.setBrush(QColor(180, 180, 180)); p.drawRect(lp);
        p.setPen(Qt::cyan); p.drawText(lp.right() + 3, lp.center().y() + 4, QString::number(i + 1));
        p.setPen(palette().text().color());
        p.drawText(QRect(5, lp.top() - 5, body.left() - 15, 20), Qt::AlignRight | Qt::AlignVCenter, pins[i]);
        
        int rIdx = 7 - i;
        QRect rp(body.right(), py, pinW, pinH);
        p.setBrush(QColor(180, 180, 180)); p.drawRect(rp);
        p.setPen(Qt::cyan); p.drawText(rp.left() - 10, rp.center().y() + 4, QString::number(rIdx + 1));
        p.setPen(palette().text().color());
        p.drawText(QRect(body.right() + 15, rp.top() - 5, width() - body.right() - 20, 20), Qt::AlignLeft | Qt::AlignVCenter, pins[rIdx]);
    }
}

void ChipPreviewWidget::draw16Pin(QPainter &p, const QStringList &pins) {
    int w = 70; int h = 160;
    int centerX = width() / 2; int centerY = height() / 2 - 10;
    QRect body(centerX - w/2, centerY - h/2, w, h);
    p.setBrush(QColor(25, 25, 25)); p.setPen(QPen(Qt::gray, 2)); p.drawRect(body);
    p.setBrush(Qt::black); p.drawEllipse(body.topLeft() + QPoint(8, 8), 5, 5);
    
    p.setPen(QColor(180, 180, 180, 200));
    QFont modelFont = font(); modelFont.setPointSize(7); p.setFont(modelFont);
    QString label = m_model.isEmpty() ? tr("CHIP") : m_model.split(" ").last();
    if (label.length() > 10) label = label.left(8) + "..";
    p.drawText(body, Qt::AlignCenter, label);

    int pinSpacing = h / 8; int pinW = 15; int pinH = 8;
    for (int i = 0; i < 8; ++i) {
        int py = body.top() + pinSpacing/2 + i * pinSpacing - pinH/2;
        QRect lp(body.left() - pinW, py, pinW, pinH);
        p.setBrush(QColor(180, 180, 180)); p.drawRect(lp);
        p.setPen(Qt::cyan); p.drawText(lp.right() + 2, lp.center().y() + 3, QString::number(i + 1));
        p.setPen(palette().text().color());
        p.drawText(QRect(5, lp.top() - 7, body.left() - 12, 20), Qt::AlignRight | Qt::AlignVCenter, pins[i]);
        
        int rIdx = 15 - i;
        QRect rp(body.right(), py, pinW, pinH);
        p.setBrush(QColor(180, 180, 180)); p.drawRect(rp);
        p.setPen(Qt::cyan); p.drawText(rp.left() - 10, rp.center().y() + 3, QString::number(rIdx + 1));
        p.setPen(palette().text().color());
        p.drawText(QRect(body.right() + 12, rp.top() - 7, width() - body.right() - 15, 20), Qt::AlignLeft | Qt::AlignVCenter, pins[rIdx]);
    }
}
