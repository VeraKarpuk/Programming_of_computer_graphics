#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

// Простой линейный график с несколькими сериями
class ChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChartWidget(QWidget *parent = 0);

    void setup(const QString &title, const QString &xLabel, const QStringList &names);
    void addPoint(int series, double x, double ms);
    void clearData();
    QString csv() const;

protected:
    void paintEvent(QPaintEvent *ev);

private:
    struct Series
    {
        QString name;
        QVector<QPointF> pts;
    };

    QString m_title, m_xLabel;
    QVector<Series> m_series;
};

#endif
