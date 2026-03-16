#include "hexeditor.h"
#include <QPainter>
#include <QKeyEvent>
#include <QScrollBar>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QDebug>

HexEditor::HexEditor(QWidget *parent) : QAbstractScrollArea(parent) {
    // Font setup
    QFont font("Monospace", 10);
    font.setStyleHint(QFont::TypeWriter);
    setFont(font);
    
    updateMetrics();
    
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setBackgroundRole(QPalette::Base);
    viewport()->setCursor(Qt::IBeamCursor);
    viewport()->setMouseTracking(true);
    
    m_cursorTimer = new QTimer(this);
    connect(m_cursorTimer, &QTimer::timeout, this, &HexEditor::onCursorFlash);
    m_cursorTimer->start(500);
}

void HexEditor::updateMetrics() {
    QFontMetrics fm(font());
    m_charWidth = fm.horizontalAdvance('0');
    m_charHeight = fm.height();
    m_lineHeight = m_charHeight + 2;
    
    m_addrWidth = 10 * m_charWidth;
    m_hexWidth = (m_bytesPerLine * 3 - 1) * m_charWidth;
    m_asciiWidth = m_bytesPerLine * m_charWidth;
    
    updateScrollBars();
}

void HexEditor::updateScrollBars() {
    int lines = (m_data.size() + m_bytesPerLine - 1) / m_bytesPerLine;
    verticalScrollBar()->setRange(0, qMax(0, lines - viewport()->height() / m_lineHeight));
    verticalScrollBar()->setPageStep(viewport()->height() / m_lineHeight);
    
    horizontalScrollBar()->setRange(0, qMax(0, (m_addrWidth + m_gap + m_hexWidth + m_gap + m_asciiWidth + m_margin * 2) - viewport()->width()));
}

QSize HexEditor::sizeHint() const {
    return QSize(600, 450); // Suggest a large height initially
}

void HexEditor::setData(const QByteArray &data) {
    m_data = data;
    m_cursorPos = 0;
    m_cursorNibble = 0;
    m_selectionStart = -1;
    m_selectionEnd = -1;
    updateScrollBars();
    viewport()->update();
}

void HexEditor::scrollToAddress(qint64 addr) {
    int line = addr / m_bytesPerLine;
    verticalScrollBar()->setValue(line);
}

void HexEditor::scrollToCursor() {
    int line = m_cursorPos / m_bytesPerLine;
    int firstVisible = verticalScrollBar()->value();
    int lastVisible = firstVisible + viewport()->height() / m_lineHeight - 1;
    
    if (line < firstVisible) {
        verticalScrollBar()->setValue(line);
    } else if (line > lastVisible) {
        verticalScrollBar()->setValue(line - (viewport()->height() / m_lineHeight - 1));
    }
}

void HexEditor::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, false);
    
    if (m_data.isEmpty()) {
        p.setPen(Qt::gray);
        p.drawText(viewport()->rect(), Qt::AlignCenter, tr("No data loaded."));
        return;
    }

    int scrollY = verticalScrollBar()->value();
    int scrollX = horizontalScrollBar()->value();
    p.translate(-scrollX, 0);

    int startLine = scrollY;
    int endLine = qMin((int)((m_data.size() + m_bytesPerLine - 1) / m_bytesPerLine), startLine + viewport()->height() / m_lineHeight + 1);

    for (int line = startLine; line < endLine; ++line) {
        int y = (line - startLine) * m_lineHeight + m_charHeight;
        qint64 lineAddr = (qint64)line * m_bytesPerLine;
        
        // 1. Draw Address
        p.setPen(Qt::darkGray);
        p.drawText(m_margin, y, formatAddress(lineAddr));
        
        // 2. Draw Hex Data
        for (int i = 0; i < m_bytesPerLine; ++i) {
            qint64 pos = lineAddr + i;
            if (pos >= m_data.size()) break;
            
            int x = m_margin + m_addrWidth + m_gap + i * (m_charWidth * 3);
            
            // Selection / Cursor Highlight
            if (isSelected(pos)) {
                p.fillRect(x - 2, (line - startLine) * m_lineHeight, m_charWidth * 2 + 4, m_lineHeight, palette().highlight());
                p.setPen(palette().highlightedText().color());
            } else if (pos == m_cursorPos && hasFocus()) {
                p.setPen(Qt::blue);
                p.drawRect(x - 2, (line - startLine) * m_lineHeight, m_charWidth * 2 + 4, m_lineHeight - 1);
            } else {
                p.setPen(palette().text().color());
            }
            
            p.drawText(x, y, formatHex((unsigned char)m_data[(int)pos]));
        }
        
        // 3. Draw ASCII Data
        int asciiXBase = m_margin + m_addrWidth + m_gap + m_hexWidth + m_gap;
        for (int i = 0; i < m_bytesPerLine; ++i) {
            qint64 pos = lineAddr + i;
            if (pos >= m_data.size()) break;
            
            int x = asciiXBase + i * m_charWidth;
            
            if (isSelected(pos)) {
                p.fillRect(x, (line - startLine) * m_lineHeight, m_charWidth, m_lineHeight, palette().highlight());
                p.setPen(palette().highlightedText().color());
            } else if (pos == m_cursorPos && hasFocus()) {
                p.setPen(Qt::blue);
                p.drawRect(x, (line - startLine) * m_lineHeight, m_charWidth, m_lineHeight - 1);
            } else {
                p.setPen(Qt::darkCyan);
            }
            
            p.drawText(x, y, formatAscii((unsigned char)m_data[(int)pos]));
        }
    }
    
    // Draw Cursor
    if (m_cursorVisible && hasFocus() && !m_data.isEmpty()) {
        int line = m_cursorPos / m_bytesPerLine;
        if (line >= startLine && line < endLine) {
            p.setPen(Qt::red);
            int curX;
            if (m_asciiInputMode) {
                int asciiXBase = m_margin + m_addrWidth + m_gap + m_hexWidth + m_gap;
                curX = asciiXBase + (m_cursorPos % m_bytesPerLine) * m_charWidth;
                p.drawLine(curX, (line - startLine) * m_lineHeight, curX, (line - startLine + 1) * m_lineHeight);
            } else {
                int hexXBase = m_margin + m_addrWidth + m_gap;
                curX = hexXBase + (m_cursorPos % m_bytesPerLine) * (m_charWidth * 3) + m_cursorNibble * m_charWidth;
                p.drawLine(curX, (line - startLine) * m_lineHeight, curX, (line - startLine + 1) * m_lineHeight);
            }
        }
    }
}

void HexEditor::keyPressEvent(QKeyEvent *event) {
    if (m_data.isEmpty()) return;

    // Navigation
    if (event->key() == Qt::Key_Left) {
        if (m_cursorNibble == 1) { m_cursorNibble = 0; }
        else if (m_cursorPos > 0) { m_cursorPos--; m_cursorNibble = 1; }
    } else if (event->key() == Qt::Key_Right) {
        if (m_cursorNibble == 0) { m_cursorNibble = 1; }
        else if (m_cursorPos < m_data.size() - 1) { m_cursorPos++; m_cursorNibble = 0; }
    } else if (event->key() == Qt::Key_Up) {
        m_cursorPos = qMax(0LL, m_cursorPos - m_bytesPerLine);
    } else if (event->key() == Qt::Key_Down) {
        m_cursorPos = qMin((qint64)m_data.size() - 1, m_cursorPos + m_bytesPerLine);
    } else if (event->key() == Qt::Key_PageUp) {
        m_cursorPos = qMax(0LL, m_cursorPos - m_bytesPerLine * (viewport()->height() / m_lineHeight));
    } else if (event->key() == Qt::Key_PageDown) {
        m_cursorPos = qMin((qint64)m_data.size() - 1, m_cursorPos + m_bytesPerLine * (viewport()->height() / m_lineHeight));
    } else if (event->key() == Qt::Key_Home) {
        m_cursorPos = (m_cursorPos / m_bytesPerLine) * m_bytesPerLine;
    } else if (event->key() == Qt::Key_End) {
        m_cursorPos = qMin((qint64)m_data.size() - 1, (m_cursorPos / m_bytesPerLine) * m_bytesPerLine + m_bytesPerLine - 1);
    }
    // Switch between Hex and ASCII
    else if (event->key() == Qt::Key_Tab) {
        m_asciiInputMode = !m_asciiInputMode;
        m_cursorNibble = 0;
    }
    // Hex Editing
    else if (!m_readOnly && !m_asciiInputMode && ((event->text().length() > 0 && QString("0123456789abcdefABCDEF").contains(event->text()[0])))) {
        bool ok;
        int val = event->text().toInt(&ok, 16);
        if (ok) {
            unsigned char b = (unsigned char)m_data[(int)m_cursorPos];
            if (m_cursorNibble == 0) {
                b = (b & 0x0F) | (val << 4);
                m_data[(int)m_cursorPos] = (char)b;
                m_cursorNibble = 1;
            } else {
                b = (b & 0xF0) | val;
                m_data[(int)m_cursorPos] = (char)b;
                if (m_cursorPos < (qint64)m_data.size() - 1) {
                    m_cursorPos++;
                    m_cursorNibble = 0;
                }
            }
            emit dataChanged();
        }
    }
    // ASCII Editing
    else if (!m_readOnly && m_asciiInputMode && event->text().length() > 0 && event->text()[0].isPrint()) {
        m_data[(int)m_cursorPos] = (char)event->text()[0].toLatin1();
        if (m_cursorPos < (qint64)m_data.size() - 1) m_cursorPos++;
        emit dataChanged();
    }
    // Copy
    else if (event->modifiers() & Qt::ControlModifier && event->key() == Qt::Key_C) {
        if (m_selectionStart != -1) {
            qint64 start = qMin(m_selectionStart, m_selectionEnd);
            qint64 len = qAbs(m_selectionStart - m_selectionEnd) + 1;
            QApplication::clipboard()->setText(m_data.mid((int)start, (int)len).toHex(' ').toUpper());
        }
    }
    
    m_cursorVisible = true;
    m_cursorTimer->start(500);
    scrollToCursor();
    viewport()->update();
    emit cursorPositionChanged(m_cursorPos);
}

void HexEditor::mousePressEvent(QMouseEvent *event) {
    if (m_data.isEmpty()) return;
    
    setFocus();
    qint64 pos = posAt(event->pos());
    if (pos >= 0) {
        m_cursorPos = pos;
        m_cursorNibble = 0;
        
        // Determine if ASCII or Hex area was clicked
        int scrollX = horizontalScrollBar()->value();
        int x = event->pos().x() + scrollX;
        int hexStart = m_margin + m_addrWidth + m_gap;
        int asciiStart = hexStart + m_hexWidth + m_gap;
        
        if (x >= asciiStart) {
            m_asciiInputMode = true;
        } else if (x >= hexStart) {
            m_asciiInputMode = false;
            int colX = x - hexStart;
            int byteCol = (m_cursorPos % m_bytesPerLine);
            int localX = colX - byteCol * (m_charWidth * 3);
            if (localX > m_charWidth) m_cursorNibble = 1;
        }
        
        if (event->modifiers() & Qt::ShiftModifier && m_selectionStart != -1) {
            m_selectionEnd = pos;
        } else {
            m_selectionStart = pos;
            m_selectionEnd = pos;
        }
        m_isSelecting = true;
    }
    viewport()->update();
    emit cursorPositionChanged(m_cursorPos);
}

void HexEditor::mouseMoveEvent(QMouseEvent *event) {
    if (m_isSelecting) {
        qint64 pos = posAt(event->pos());
        if (pos >= 0) {
            m_selectionEnd = pos;
            m_cursorPos = pos;
            viewport()->update();
        }
    }
}

void HexEditor::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    m_isSelecting = false;
    if (m_selectionStart == m_selectionEnd) {
        m_selectionStart = -1;
        m_selectionEnd = -1;
    }
}

qint64 HexEditor::posAt(const QPoint &pos) const {
    int scrollY = verticalScrollBar()->value();
    int scrollX = horizontalScrollBar()->value();
    
    int line = pos.y() / m_lineHeight + scrollY;
    int x = pos.x() + scrollX;
    
    int hexStart = m_margin + m_addrWidth + m_gap;
    int hexEnd = hexStart + m_hexWidth;
    int asciiStart = hexStart + m_hexWidth + m_gap;
    int asciiEnd = asciiStart + m_asciiWidth;
    
    qint64 lineAddr = (qint64)line * m_bytesPerLine;
    if (lineAddr >= m_data.size()) return -1;
    
    if (x >= hexStart && x < hexEnd) {
        int col = (x - hexStart) / (m_charWidth * 3);
        qint64 finalPos = lineAddr + col;
        return (finalPos < m_data.size()) ? finalPos : m_data.size() - 1;
    } else if (x >= asciiStart && x < asciiEnd) {
        int col = (x - asciiStart) / m_charWidth;
        qint64 finalPos = lineAddr + col;
        return (finalPos < m_data.size()) ? finalPos : m_data.size() - 1;
    }
    
    return -1;
}

void HexEditor::resizeEvent(QResizeEvent *event) {
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
}

void HexEditor::onCursorFlash() {
    m_cursorVisible = !m_cursorVisible;
    viewport()->update();
}

void HexEditor::focusInEvent(QFocusEvent *event) {
    Q_UNUSED(event);
    m_cursorTimer->start(500);
    viewport()->update();
}

void HexEditor::focusOutEvent(QFocusEvent *event) {
    Q_UNUSED(event);
    m_cursorTimer->stop();
    m_cursorVisible = false;
    viewport()->update();
}

bool HexEditor::event(QEvent *event) {
    return QAbstractScrollArea::event(event);
}

QString HexEditor::formatAddress(qint64 addr) const {
    return QString("%1").arg(addr, 8, 16, QChar('0')).toUpper();
}

QString HexEditor::formatHex(unsigned char b) const {
    return QString("%1").arg(b, 2, 16, QChar('0')).toUpper();
}

QString HexEditor::formatAscii(unsigned char b) const {
    if (b >= 32 && b <= 126) return QString((char)b);
    return ".";
}

bool HexEditor::isSelected(qint64 pos) const {
    if (m_selectionStart == -1) return false;
    qint64 start = qMin(m_selectionStart, m_selectionEnd);
    qint64 end = qMax(m_selectionStart, m_selectionEnd);
    return pos >= start && pos <= end;
}
