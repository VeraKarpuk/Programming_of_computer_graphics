#ifndef BENCHMARKDIALOG_H
#define BENCHMARKDIALOG_H

#include <QDialog>

class QSpinBox;
class QProgressBar;
class QPushButton;
class QTabWidget;
class QPlainTextEdit;
class PlotWidget;

// измерения для отчёта: время отсечения от глубины дерева и от плотности данных
class BenchmarkDialog : public QDialog {
    Q_OBJECT
public:
    explicit BenchmarkDialog(QWidget *parent = nullptr);

private slots:
    void run();
    void saveCsv();
    void savePng();

private:
    QSpinBox *nSpin_, *windowsSpin_, *capSpin_;
    QProgressBar *bar_;
    QPushButton *runBtn_;
    QTabWidget *tabs_;
    PlotWidget *depthPlot_, *densityPlot_;
    QPlainTextEdit *csvText_;
};

#endif
