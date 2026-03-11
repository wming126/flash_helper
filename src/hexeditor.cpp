#include "hexeditor.h"
#include <QPainter>
#include <QKeyEvent>
#include <QScrollBar>
#include <QFontMetrics>

HexEditor::HexEditor(QWidget *parent) : QAbstractScrollArea(parent) {
    QFont font("Monospace", 10);
    font.setStyleHint(QFont::TypeWriter);
    setFont(font);
    
    QFontMetrics fm(font);
    m_charWidth = fm.horizontalAdvance('0');
    m_charHeight = fm.height();
    m_lineHeight = m_charHeight + 2;
    
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setBackgroundRole(QPalette::Base);
}

void HexEditor::setData(const QByteArray &data) {
    m_data = data;
    m_modifiedIndices.clear();
    m_cursorPos = 0;
    m_highNibble = true;
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
    
    int firstLine = verticalScrollBar()->value() / m_lineHeight;
    int lastLine = (verticalScrollBar()->value() + viewport()->height()) / m_lineHeight + 1;
    
    for (int line = firstLine; line < lastLine; ++line) {
        int y = line * m_lineHeight + m_charHeight;
        if (line * m_bytesPerLine >= m_data.size()) break;
        
        // 地址区
        p.setPen(Qt::darkGray);
        p.drawText(5, y, QString("%1").arg(line * m_bytesPerLine, 8, 16, QChar('0')).toUpper());
        
        // Hex 区
        for (int i = 0; i < m_bytesPerLine; ++i) {
            int idx = line * m_bytesPerLine + i;
            if (idx >= m_data.size()) break;
            
            int x = m_addrWidth + i * (m_charWidth * 3);
            
            // 绘制光标背景
            if (idx == m_cursorPos && hasFocus()) {
                p.fillRect(x - 2, line * m_lineHeight, m_charWidth * 2 + 4, m_lineHeight, palette().highlight());
                p.setPen(palette().highlightedText().color());
            } else if (m_modifiedIndices.contains(idx)) {
                p.setPen(Qt::red);
            } else {
                p.setPen(palette().text().color());
            }
            
            p.drawText(x, y, QString("%1").arg((unsigned char)m_data[idx], 2, 16, QChar('0')).toUpper());
        }
        
        // ASCII 区
        int asciiX = m_addrWidth + m_bytesPerLine * (m_charWidth * 3) + m_gap;
        p.setPen(Qt::darkCyan);
        for (int i = 0; i < m_bytesPerLine; ++i) {
            int idx = line * m_bytesPerLine + i;
            if (idx >= m_data.size()) break;
            char c = m_data[idx];
            p.drawText(asciiX + i * m_charWidth, y, (c >= 32 && c <= 126) ? QString(c) : ".");
        }
    }
}

void HexEditor::keyPressEvent(QKeyEvent *event) {
    if (m_data.isEmpty()) return;
    
    // 导航
    if (event->key() == Qt::Key_Left) { m_cursorPos = qMax(0, m_cursorPos - 1); m_highNibble = true; }
    else if (event->key() == Qt::Key_Right) { m_cursorPos = qMin(m_data.size() - 1, m_cursorPos + 1); m_highNibble = true; }
    else if (event->key() == Qt::Key_Up) { m_cursorPos = qMax(0, m_cursorPos - m_bytesPerLine); }
    else if (event->key() == Qt::Key_Down) { m_cursorPos = qMin(m_data.size() - 1, m_cursorPos + m_bytesPerLine); }
    
    // 编辑
    QString text = event->text().toUpper();
    if (!text.isEmpty() && ((text[0] >= '0' && text[0] <= '9') || (text[0] >= 'A' && text[0] <= 'F'))) {
        int val = text[0].digitValue();
        unsigned char byte = (unsigned char)m_data[m_cursorPos];
        if (m_highNibble) {
            byte = (val << 4) | (byte & 0x0F);
            m_highNibble = false;
        } else {
            byte = (byte & 0xF0) | val;
            m_highNibble = true;
            m_modifiedIndices.insert(m_cursorPos);
            m_data[m_cursorPos] = byte; // 先写回数据
            m_cursorPos = qMin(m_data.size() - 1, m_cursorPos + 1); // 再移动光标
            emit dataChanged();
            update();
            return;
        }
        m_data[m_cursorPos] = byte;
        m_modifiedIndices.insert(m_cursorPos);
    }
    
    scrollToCursor();
    update();
}

void HexEditor::scrollToCursor() {
    int y = (m_cursorPos / m_bytesPerLine) * m_lineHeight;
    if (y < verticalScrollBar()->value()) {
        verticalScrollBar()->setValue(y);
    } else if (y + m_lineHeight > verticalScrollBar()->value() + viewport()->height()) {
        verticalScrollBar()->setValue(y - viewport()->height() + m_lineHeight);
    }
}

void HexEditor::mousePressEvent(QMouseEvent *event) {
    int idx = posAt(event->pos());
    if (idx >= 0 && idx < m_data.size()) {
        m_cursorPos = idx;
        m_highNibble = true;
        update();
    }
}

int HexEditor::posAt(const QPoint &pos) const {
    int y = pos.y() + verticalScrollBar()->value();
    int line = y / m_lineHeight;
    int x = pos.x() - m_addrWidth;
    if (x < 0) return -1;
    int col = x / (m_charWidth * 3);
    if (col >= m_bytesPerLine) return -1;
    return line * m_bytesPerLine + col;
}
