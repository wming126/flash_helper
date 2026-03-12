#include "hexeditor.h"
#include <QPainter>
#include <QKeyEvent>
#include <QScrollBar>
#include <QFontMetrics>
#include <QMouseEvent>

HexEditor::HexEditor(QWidget *parent) : QAbstractScrollArea(parent) {
    QFont font("Monospace", 10);
    font.setStyleHint(QFont::TypeWriter);
    setFont(font);
    
    QFontMetrics fm(font);
    m_charWidth = fm.horizontalAdvance('0');
    m_charHeight = fm.height();
    m_lineHeight = m_charHeight + 4;
    
    setFocusPolicy(Qt::StrongFocus);
    viewport()->installEventFilter(this); // 关键：拦截 viewport 事件
    viewport()->setBackgroundRole(QPalette::Base);
    viewport()->setCursor(Qt::IBeamCursor);
}

bool HexEditor::eventFilter(QObject *obj, QEvent *event) {
    if (obj == viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            mousePressEvent(static_cast<QMouseEvent*>(event));
            return true;
        }
        if (event->type() == QEvent::KeyPress) {
            keyPressEvent(static_cast<QKeyEvent*>(event));
            return true;
        }
    }
    return QAbstractScrollArea::eventFilter(obj, event);
}

void HexEditor::setData(const QByteArray &data) {
    m_data = data;
    m_cursorPos = 0;
    updateLayout();
    update();
}

void HexEditor::updateLayout() {
    int lines = (m_data.size() + m_bytesPerLine - 1) / m_bytesPerLine;
    verticalScrollBar()->setRange(0, qMax(0, lines * m_lineHeight - viewport()->height()));
    verticalScrollBar()->setPageStep(viewport()->height());
}

void HexEditor::resizeEvent(QResizeEvent *event) {
    QAbstractScrollArea::resizeEvent(event);
    updateLayout();
}

void HexEditor::paintEvent(QPaintEvent *event) {
    QPainter p(viewport());
    p.translate(0, -verticalScrollBar()->value());
    
    if (m_data.isEmpty()) {
        p.setPen(Qt::gray);
        p.drawText(viewport()->rect(), Qt::AlignCenter, tr("No data loaded. Use 'Browse' or 'Read' to load firmware."));
        return;
    }

    int firstLine = verticalScrollBar()->value() / m_lineHeight;
    int lastLine = (verticalScrollBar()->value() + viewport()->height()) / m_lineHeight + 1;
    
    for (int line = firstLine; line < lastLine; ++line) {
        int y = line * m_lineHeight + m_charHeight;
        if (line * m_bytesPerLine >= m_data.size()) break;
        
        // 绘制地址区
        p.setPen(Qt::darkGray);
        p.drawText(5, y, QString("%1").arg(line * m_bytesPerLine, 8, 16, QChar('0')).toUpper());
        
        // 绘制 Hex 区
        for (int i = 0; i < m_bytesPerLine; ++i) {
            int idx = line * m_bytesPerLine + i;
            if (idx >= m_data.size()) break;
            int x = m_addrWidth + i * (m_charWidth * 3);
            
            if (idx == m_cursorPos) {
                p.fillRect(x - 2, line * m_lineHeight + 2, m_charWidth * 2 + 4, m_lineHeight - 4, hasFocus() ? palette().highlight() : Qt::lightGray);
                p.setPen(hasFocus() ? palette().highlightedText().color() : palette().text().color());
            } else {
                p.setPen(palette().text().color());
            }
            p.drawText(x, y, QString("%1").arg((unsigned char)m_data[idx], 2, 16, QChar('0')).toUpper());
        }
        
        // 绘制 ASCII 区
        int asciiX = m_addrWidth + m_bytesPerLine * (m_charWidth * 3) + m_gap;
        for (int i = 0; i < m_bytesPerLine; ++i) {
            int idx = line * m_bytesPerLine + i;
            if (idx >= m_data.size()) break;
            char c = m_data[idx];
            p.setPen((idx == m_cursorPos && hasFocus()) ? Qt::blue : Qt::darkCyan);
            p.drawText(asciiX + i * m_charWidth, y, (c >= 32 && c <= 126) ? QString(c) : ".");
        }
    }
}

void HexEditor::keyPressEvent(QKeyEvent *event) {
    if (m_data.isEmpty()) return;
    
    // 导航键处理
    if (event->key() == Qt::Key_Left) { 
        if (m_cursorPos > 0) m_cursorPos--;
    }
    else if (event->key() == Qt::Key_Right) { 
        if (m_cursorPos < m_data.size() - 1) m_cursorPos++;
    }
    else if (event->key() == Qt::Key_Up) { m_cursorPos = qMax(0, m_cursorPos - m_bytesPerLine); }
    else if (event->key() == Qt::Key_Down) { m_cursorPos = qMin(m_data.size() - 1, m_cursorPos + m_bytesPerLine); }
    
    scrollToCursor();
    update();
}

void HexEditor::scrollToCursor() {
    int y = (m_cursorPos / m_bytesPerLine) * m_lineHeight;
    int vValue = verticalScrollBar()->value();
    if (y < vValue) verticalScrollBar()->setValue(y);
    else if (y + m_lineHeight > vValue + viewport()->height())
        verticalScrollBar()->setValue(y - viewport()->height() + m_lineHeight);
}

void HexEditor::mousePressEvent(QMouseEvent *event) {
    viewport()->setFocus();
    int idx = posAt(event->pos());
    if (idx >= 0 && idx < m_data.size()) {
        m_cursorPos = idx;
        update();
    }
}

int HexEditor::posAt(const QPoint &pos) const {
    int y = pos.y() + verticalScrollBar()->value();
    int line = y / m_lineHeight;
    if (line < 0) return -1;
    
    int x = pos.x() - m_addrWidth;
    if (x < 0) return -1;
    
    int hexWidth = m_bytesPerLine * (m_charWidth * 3);
    if (x < hexWidth) {
        int col = x / (m_charWidth * 3);
        if (col < m_bytesPerLine) return line * m_bytesPerLine + col;
    } else {
        int asciiX = x - hexWidth - m_gap;
        if (asciiX >= 0) {
            int col = asciiX / m_charWidth;
            if (col < m_bytesPerLine) return line * m_bytesPerLine + col;
        }
    }
    return -1;
}
