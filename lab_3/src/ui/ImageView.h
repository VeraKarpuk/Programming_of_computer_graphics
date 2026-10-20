// Просмотр изображения: масштаб колесом, перетаскивание мышью, двойной щелчок — «вписать».
#pragma once
#include <QImage>
#include <QPointF>
#include <QWidget>

class ImageView : public QWidget {
    Q_OBJECT
public:
    explicit ImageView(QWidget* parent = 0);
    void setImage(const QImage& img);
    void setCaption(const QString& text);

public slots:
    void applyView(double scale, QPointF center, bool fit);  // синхронизация со вторым видом

signals:
    void viewChanged(double scale, QPointF center, bool fit);

protected:
    void paintEvent(QPaintEvent*);
    void wheelEvent(QWheelEvent*);
    void mousePressEvent(QMouseEvent*);
    void mouseMoveEvent(QMouseEvent*);
    void mouseReleaseEvent(QMouseEvent*);
    void mouseDoubleClickEvent(QMouseEvent*);

private:
    double fitScale() const;
    double scale() const;
    QPointF center() const;

    QImage m_img;
    QString m_caption;
    bool m_fit;
    double m_scale;
    QPointF m_center;
    bool m_drag;
    QPoint m_last;
};
