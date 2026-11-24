#include "benchmarkdialog.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <chrono>
#include <random>

#include "plotwidget.h"
#include "quadtree.h"
#include "sceneio.h"

namespace {

using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

struct Measure {
    double buildMs = 0, treeMs = 0, bruteMs = 0, visible = 0;
    int nodes = 0;
};

// усреднённое время запроса окном 100x80 в случайных местах области 1000x1000
Measure measure(const std::vector<geo::Segment> &data, const geo::Rect &area, int depth, int cap,
                int windows) {
    Measure m;
    geo::Quadtree tree;
    Clock::time_point t0 = Clock::now();
    tree.build(&data, area, depth, cap);
    m.buildMs = msSince(t0);
    m.nodes = tree.nodeCount();

    std::mt19937 rng(77);
    std::uniform_real_distribution<double> u(0, 900);
    std::vector<geo::Rect> ws;
    for (int i = 0; i < windows; ++i) {
        double x = u(rng), y = u(rng) * 0.92;
        ws.push_back(geo::Rect(x, y, x + 100, y + 80));
    }

    std::vector<geo::Segment> out;
    geo::QueryStats st;
    double vis = 0;
    t0 = Clock::now();
    for (const geo::Rect &w : ws) {
        tree.clipWindow(w, out, nullptr, st);
        vis += st.visible;
    }
    m.treeMs = msSince(t0) / windows;
    m.visible = vis / windows;

    int brute = std::min(windows, 20);
    t0 = Clock::now();
    for (int i = 0; i < brute; ++i) geo::clipBruteForce(data, ws[i], out, st);
    m.bruteMs = msSince(t0) / brute;
    return m;
}

}  // namespace

BenchmarkDialog::BenchmarkDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QString::fromUtf8("Бенчмарк для отчёта"));
    resize(900, 640);

    nSpin_ = new QSpinBox(this);
    nSpin_->setRange(10000, 2000000);
    nSpin_->setSingleStep(50000);
    nSpin_->setValue(500000);
    windowsSpin_ = new QSpinBox(this);
    windowsSpin_->setRange(10, 2000);
    windowsSpin_->setValue(200);
    capSpin_ = new QSpinBox(this);
    capSpin_->setRange(1, 512);
    capSpin_->setValue(32);

    QFormLayout *form = new QFormLayout();
    form->addRow(QString::fromUtf8("Отрезков в базе (N):"), nSpin_);
    form->addRow(QString::fromUtf8("Запросов окном на точку графика:"), windowsSpin_);
    form->addRow(QString::fromUtf8("Ёмкость листа:"), capSpin_);

    runBtn_ = new QPushButton(QString::fromUtf8("Запустить"), this);
    QPushButton *csvBtn = new QPushButton(QString::fromUtf8("Сохранить CSV…"), this);
    QPushButton *pngBtn = new QPushButton(QString::fromUtf8("Сохранить график (PNG)…"), this);
    QHBoxLayout *btns = new QHBoxLayout();
    btns->addWidget(runBtn_);
    btns->addWidget(csvBtn);
    btns->addWidget(pngBtn);
    btns->addStretch(1);

    bar_ = new QProgressBar(this);
    bar_->setValue(0);

    tabs_ = new QTabWidget(this);
    depthPlot_ = new PlotWidget(this);
    densityPlot_ = new PlotWidget(this);
    csvText_ = new QPlainTextEdit(this);
    csvText_->setReadOnly(true);
    tabs_->addTab(depthPlot_, QString::fromUtf8("Время от глубины дерева"));
    tabs_->addTab(densityPlot_, QString::fromUtf8("Время от плотности данных"));
    tabs_->addTab(csvText_, QString::fromUtf8("Таблица (CSV)"));

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addLayout(btns);
    lay->addWidget(bar_);
    lay->addWidget(tabs_, 1);

    connect(runBtn_, &QPushButton::clicked, this, &BenchmarkDialog::run);
    connect(csvBtn, &QPushButton::clicked, this, &BenchmarkDialog::saveCsv);
    connect(pngBtn, &QPushButton::clicked, this, &BenchmarkDialog::savePng);
}

void BenchmarkDialog::run() {
    runBtn_->setEnabled(false);
    const geo::Rect area(0, 0, 1000, 1000);
    int N = nSpin_->value(), windows = windowsSpin_->value(), cap = capSpin_->value();

    const int maxDepth = 12;
    int steps = maxDepth + 4, done = 0;
    bar_->setRange(0, steps);
    bar_->setValue(0);

    QString csv = "experiment,depth,N,density_per_10k_units2,nodes,build_ms,query_tree_ms,query_brute_ms,avg_visible\n";
    auto line = [&](const char *name, int depth, int n, const Measure &m) {
        csv += QString("%1,%2,%3,%4,%5,%6,%7,%8,%9\n").arg(QString::fromLatin1(name)).arg(depth).arg(n)
                   .arg(n / 100.0, 0, 'f', 1).arg(m.nodes).arg(m.buildMs, 0, 'f', 1)
                   .arg(m.treeMs, 0, 'f', 4).arg(m.bruteMs, 0, 'f', 4).arg(m.visible, 0, 'f', 1);
    };

    // 1. зависимость от глубины при фиксированном N
    std::vector<geo::Segment> data = geo::randomSegments(N, area, 20.0, 1);
    geo::sortSpatially(data, area);
    PlotSeries tree1, brute1;
    tree1.name = QString::fromUtf8("квадродерево");
    tree1.color = QColor(30, 110, 200);
    brute1.name = QString::fromUtf8("полный перебор");
    brute1.color = QColor(210, 70, 50);
    int bestDepth = 1;
    double bestMs = 1e300;
    for (int d = 1; d <= maxDepth; ++d) {
        Measure m = measure(data, area, d, cap, windows);
        tree1.x.push_back(d);
        tree1.y.push_back(m.treeMs);
        brute1.x.push_back(d);
        brute1.y.push_back(m.bruteMs);
        line("depth", d, N, m);
        if (m.treeMs < bestMs) { bestMs = m.treeMs; bestDepth = d; }
        bar_->setValue(++done);
        QApplication::processEvents();
    }
    depthPlot_->setData(QString::fromUtf8("Время отсечения окном 100x80, N = %1").arg(N),
                        QString::fromUtf8("максимальная глубина дерева"),
                        QString::fromUtf8("среднее время запроса, мс"), {tree1, brute1});

    // 2. зависимость от плотности при лучшей глубине
    PlotSeries tree2, brute2;
    tree2.name = tree1.name;
    tree2.color = tree1.color;
    brute2.name = brute1.name;
    brute2.color = brute1.color;
    const double fractions[4] = {0.1, 0.25, 0.5, 1.0};
    for (double f : fractions) {
        int n = std::max(1000, (int)(N * f));
        std::vector<geo::Segment> d2 = geo::randomSegments(n, area, 20.0, 2);
        geo::sortSpatially(d2, area);
        Measure m = measure(d2, area, bestDepth, cap, windows);
        double density = n / 100.0;
        tree2.x.push_back(density);
        tree2.y.push_back(m.treeMs);
        brute2.x.push_back(density);
        brute2.y.push_back(m.bruteMs);
        line("density", bestDepth, n, m);
        bar_->setValue(++done);
        QApplication::processEvents();
    }
    densityPlot_->setData(QString::fromUtf8("Время отсечения от плотности данных, глубина %1").arg(bestDepth),
                          QString::fromUtf8("плотность, отрезков на 10^4 ед.^2"),
                          QString::fromUtf8("среднее время запроса, мс"), {tree2, brute2});

    csvText_->setPlainText(csv);
    runBtn_->setEnabled(true);
}

void BenchmarkDialog::saveCsv() {
    QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("Сохранить CSV"), "benchmark.csv", "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8("Не удалось записать файл"));
        return;
    }
    f.write(csvText_->toPlainText().toUtf8());
}

void BenchmarkDialog::savePng() {
    QWidget *w = tabs_->currentWidget();
    if (w != depthPlot_ && w != densityPlot_) {
        QMessageBox::information(this, windowTitle(), QString::fromUtf8("Выберите вкладку с графиком"));
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("Сохранить график"), "plot.png", "PNG (*.png)");
    if (path.isEmpty()) return;
    w->grab().save(path);
}
