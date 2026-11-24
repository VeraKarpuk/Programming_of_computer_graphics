#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QColor>
#include <QString>
#include <QWidget>
#include <vector>

struct PlotSeries {
    QString name;
    std::vector<double> x, y;
    QColor color;
};

// простой график "линии с маркерами" для отчёта
class PlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlotWidget(QWidget *parent = nullptr);
    void setData(const QString &title, const QString &xLabel, const QString &yLabel,
                 const std::vector<PlotSeries> &series);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString title_, xLabel_, yLabel_;
    std::vector<PlotSeries> series_;
};

#endif
