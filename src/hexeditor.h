#ifndef HEXEDITOR_H
#define HEXEDITOR_H

#include <QAbstractScrollArea>
#include <QByteArray>
#include <QFont>
#include <QSet>

class HexEditor : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit HexEditor(QWidget *parent = nullptr);
    void setData(const QByteArray &data);
    QByteArray data() const { return m_data; }
    bool isModified() const { return !m_modifiedIndices.isEmpty(); }
    void clearModified() { m_modifiedIndices.clear(); update(); }

signals:
    void dataChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QByteArray m_data;
    QSet<int> m_modifiedIndices;
    int m_cursorPos = 0;
    bool m_highNibble = true;
    
    int m_charWidth;
    int m_charHeight;
    int m_lineHeight;
    
    // 布局常量
    const int m_addrDigits = 8;
    const int m_bytesPerLine = 16;
    const int m_addrWidth = 80;
    const int m_gap = 10;

    void updateLayout();
    void scrollToCursor();
    int posAt(const QPoint &pos) const;
};

#endif // HEXEDITOR_H
