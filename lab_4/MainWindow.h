#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "VirtualGrid.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QThread;
class Benchmark;
class ChartWidget;
class GridPainter;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);

protected:
    void closeEvent(QCloseEvent *ev);

private slots:
    void draw();
    void clearGrid();
    void algorithmChanged();
    void gridSizeChanged();
    void zoomStep(int direction);
    void picked(int x, int y, int button);
    void hovered(int x, int y, bool inside);
    void startBenchmark();
    void stopBenchmark();
    void saveResults();
    void benchPoint(int chart, int series, double x, double ms);
    void benchSummary(const QString &text);
    void benchFinished();

private:
    QWidget *buildDrawTab();
    QWidget *buildBenchTab();

    VirtualGrid m_grid;
    GridPainter *m_painter;
    QPlainTextEdit *m_trace;
    QComboBox *m_algo;
    QGroupBox *m_paramBox;
    QLabel *m_lab[8];
    QSpinBox *m_val[8];
    QCheckBox *m_clearBox;
    QSpinBox *m_wSpin, *m_hSpin, *m_zoomSpin;
    QLabel *m_range;

    QSpinBox *m_linesSpin, *m_shapesSpin, *m_repSpin;
    QPushButton *m_startBtn, *m_stopBtn, *m_saveBtn;
    QProgressBar *m_bar;
    QPlainTextEdit *m_summary;
    ChartWidget *m_charts[4];
    QThread *m_thread;
    Benchmark *m_bench;
};

#endif
