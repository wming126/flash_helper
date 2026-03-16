#ifndef HEXEDITOR_H
#define HEXEDITOR_H

#include <QAbstractScrollArea>
#include <QByteArray>
#include <QFont>
#include <QTimer>

class HexEditor : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit HexEditor(QWidget *parent = nullptr);
    
    void setData(const QByteArray &data);
    QByteArray data() const { return m_data; }
    
    bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly) { m_readOnly = readOnly; }

    QSize sizeHint() const override;

    // Navigation
    void scrollToAddress(qint64 addr);

signals:
    void dataChanged();
    void cursorPositionChanged(qint64 pos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool event(QEvent *event) override;

private slots:
    void onCursorFlash();

private:
    // Data
    QByteArray m_data;
    bool m_readOnly = false;
    
    // Cursor & Selection
    qint64 m_cursorPos = 0;
    int m_cursorNibble = 0; // 0 for high nibble, 1 for low nibble
    qint64 m_selectionStart = -1;
    qint64 m_selectionEnd = -1;
    bool m_isSelecting = false;
    bool m_asciiInputMode = false;
    
    // Layout & Metrics
    int m_charWidth;
    int m_charHeight;
    int m_lineHeight;
    int m_addrWidth;
    int m_hexWidth;
    int m_asciiWidth;
    int m_margin = 10;
    int m_gap = 20;
    const int m_bytesPerLine = 16;
    
    // Visuals
    QTimer *m_cursorTimer;
    bool m_cursorVisible = true;
    
    void updateMetrics();
    void updateScrollBars();
    qint64 posAt(const QPoint &pos) const;
    QRect cursorRect() const;
    void scrollToCursor();
    
    QString formatAddress(qint64 addr) const;
    QString formatHex(unsigned char b) const;
    QString formatAscii(unsigned char b) const;

    bool isSelected(qint64 pos) const;
};

#endif // HEXEDITOR_H
