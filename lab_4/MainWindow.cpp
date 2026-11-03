#include "MainWindow.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>
#include <cmath>
#include <cstdlib>

#include "Benchmark.h"
#include "ChartWidget.h"
#include "GridPainter.h"
#include "RasterEngine.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_grid(61, 41), m_thread(0), m_bench(0)
{
    setWindowTitle("Лабораторная работа 4. Базовые растровые алгоритмы");

    QTabWidget *tabs = new QTabWidget;
    tabs->addTab(buildDrawTab(), "Растеризация");
    tabs->addTab(buildBenchTab(), "Бенчмарк");
    setCentralWidget(tabs);
    resize(1280, 780);

    algorithmChanged();
    draw();
}

QWidget *MainWindow::buildDrawTab() // строит вкладку «Растеризация»
{
    QWidget *tab = new QWidget;
    QHBoxLayout *root = new QHBoxLayout(tab);

    QWidget *left = new QWidget;
    left->setFixedWidth(290);
    QVBoxLayout *lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);

    QGroupBox *algoBox = new QGroupBox("Алгоритм");
    QVBoxLayout *ab = new QVBoxLayout(algoBox);
    m_algo = new QComboBox;
    m_algo->addItems(QStringList() << "Пошаговый алгоритм" << "ЦДА" << "Брезенхем (отрезок)"
                                   << "Ву (сглаженный отрезок)" << "Кастл–Питвей (отрезок, Евклид)"
                                   << "Брезенхем (окружность)" << "Питвей (эллипс и дуга)");
    ab->addWidget(m_algo);
    lv->addWidget(algoBox);

    m_paramBox = new QGroupBox;
    QGridLayout *pg = new QGridLayout(m_paramBox);
    const char *names[8] = {"x0", "y0", "x1", "y1", "R", "b", "угол от", "угол до"};
    const int defs[8] = {-10, -6, 15, 8, 12, 7, 0, 360};
    for (int i = 0; i < 8; ++i) {
        m_lab[i] = new QLabel(names[i]);
        m_val[i] = new QSpinBox;
        if (i < 4)
            m_val[i]->setRange(-1000, 1000);
        else if (i < 6)
            m_val[i]->setRange(0, 1000);
        else {
            m_val[i]->setRange(0, 360);
            m_val[i]->setSuffix(" °");
        }
        m_val[i]->setValue(defs[i]);
        pg->addWidget(m_lab[i], i, 0);
        pg->addWidget(m_val[i], i, 1);
    }
    lv->addWidget(m_paramBox);

    m_clearBox = new QCheckBox("Очищать растр перед построением");
    m_clearBox->setChecked(true);
    lv->addWidget(m_clearBox);

    QPushButton *drawBtn = new QPushButton("Построить");
    QPushButton *clearBtn = new QPushButton("Очистить растр");
    QHBoxLayout *bl = new QHBoxLayout;
    bl->addWidget(drawBtn);
    bl->addWidget(clearBtn);
    lv->addLayout(bl);

    QGroupBox *gridBox = new QGroupBox("Сетка и масштаб");
    QGridLayout *gg = new QGridLayout(gridBox);
    m_wSpin = new QSpinBox;
    m_wSpin->setRange(11, 401);
    m_wSpin->setValue(61);
    m_hSpin = new QSpinBox;
    m_hSpin->setRange(11, 401);
    m_hSpin->setValue(41);
    m_zoomSpin = new QSpinBox;
    m_zoomSpin->setRange(3, 80);
    m_zoomSpin->setValue(18);
    m_zoomSpin->setSuffix(" px");
    m_range = new QLabel;
    gg->addWidget(new QLabel("Ширина, клеток"), 0, 0);
    gg->addWidget(m_wSpin, 0, 1);
    gg->addWidget(new QLabel("Высота, клеток"), 1, 0);
    gg->addWidget(m_hSpin, 1, 1);
    gg->addWidget(new QLabel("Размер клетки"), 2, 0);
    gg->addWidget(m_zoomSpin, 2, 1);
    gg->addWidget(m_range, 3, 0, 1, 2);
    lv->addWidget(gridBox);

    QLabel *hint = new QLabel(
        "Привязка к сетке: целые координаты (x; y) - это центр клетки-пикселя. "
        "Клетка занимает [x-0,5; x+0,5] по x и [y-0,5; y+0,5] по y, поэтому линии сетки идут по "
        "полуцелым значениям, а красные оси проходят через центры клеток x = 0 и y = 0. "
        "Ось y направлена вверх.\n\n"
        "ЛКМ - начало отрезка (центр фигуры), ПКМ - конец (радиус, полуоси). "
        "Ctrl + колесо - масштаб. У сглаженных пикселей на крупном масштабе подписана яркость.");
    hint->setWordWrap(true);
    lv->addWidget(hint);
    lv->addStretch();

    m_painter = new GridPainter(&m_grid);
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidget(m_painter);

    m_trace = new QPlainTextEdit;
    m_trace->setReadOnly(true);
    m_trace->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    m_trace->setFont(mono);
    m_trace->setPlaceholderText("Здесь появятся пошаговые вычисления");

    QSplitter *split = new QSplitter;
    split->addWidget(scroll);
    split->addWidget(m_trace);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);

    root->addWidget(left);
    root->addWidget(split, 1);

    connect(m_algo, SIGNAL(currentIndexChanged(int)), this, SLOT(algorithmChanged()));
    connect(drawBtn, SIGNAL(clicked()), this, SLOT(draw()));
    connect(clearBtn, SIGNAL(clicked()), this, SLOT(clearGrid()));
    connect(m_wSpin, SIGNAL(valueChanged(int)), this, SLOT(gridSizeChanged()));
    connect(m_hSpin, SIGNAL(valueChanged(int)), this, SLOT(gridSizeChanged()));
    connect(m_zoomSpin, SIGNAL(valueChanged(int)), m_painter, SLOT(setCellSize(int)));
    connect(m_painter, SIGNAL(picked(int,int,int)), this, SLOT(picked(int,int,int)));
    connect(m_painter, SIGNAL(hovered(int,int,bool)), this, SLOT(hovered(int,int,bool)));
    connect(m_painter, SIGNAL(zoomRequested(int)), this, SLOT(zoomStep(int)));

    gridSizeChanged();
    return tab;
}

QWidget *MainWindow::buildBenchTab() // строит вкладку «Бенчмарк»
{
    QWidget *tab = new QWidget;
    QVBoxLayout *root = new QVBoxLayout(tab);

    m_linesSpin = new QSpinBox;
    m_linesSpin->setRange(1000, 1000000);
    m_linesSpin->setSingleStep(10000);
    m_linesSpin->setValue(100000);
    m_shapesSpin = new QSpinBox;
    m_shapesSpin->setRange(1000, 1000000);
    m_shapesSpin->setSingleStep(10000);
    m_shapesSpin->setValue(50000);
    m_repSpin = new QSpinBox;
    m_repSpin->setRange(1, 10);
    m_repSpin->setValue(2);
    m_startBtn = new QPushButton("Запустить");
    m_stopBtn = new QPushButton("Остановить");
    m_stopBtn->setEnabled(false);
    m_saveBtn = new QPushButton("Сохранить графики и CSV...");
    m_saveBtn->setEnabled(false);
    m_bar = new QProgressBar;
    m_bar->setRange(0, 100);

    QHBoxLayout *top = new QHBoxLayout;
    top->addWidget(new QLabel("Отрезков:"));
    top->addWidget(m_linesSpin);
    top->addWidget(new QLabel("Окружностей:"));
    top->addWidget(m_shapesSpin);
    top->addWidget(new QLabel("Повторов (берётся лучший):"));
    top->addWidget(m_repSpin);
    top->addWidget(m_startBtn);
    top->addWidget(m_stopBtn);
    top->addWidget(m_saveBtn);
    top->addStretch();
    root->addLayout(top);
    root->addWidget(m_bar);

    QGridLayout *grid = new QGridLayout;
    for (int i = 0; i < 4; ++i) {
        m_charts[i] = new ChartWidget;
        grid->addWidget(m_charts[i], i / 2, i % 2);
    }
    root->addLayout(grid, 1);

    m_summary = new QPlainTextEdit;
    m_summary->setReadOnly(true);
    m_summary->setMaximumHeight(170);
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    m_summary->setFont(mono);
    root->addWidget(m_summary);

    connect(m_startBtn, SIGNAL(clicked()), this, SLOT(startBenchmark()));
    connect(m_stopBtn, SIGNAL(clicked()), this, SLOT(stopBenchmark()));
    connect(m_saveBtn, SIGNAL(clicked()), this, SLOT(saveResults()));

    m_charts[0]->setup("Отрезки: время от числа отрезков", "число отрезков", Benchmark::lineNames());
    m_charts[1]->setup("Отрезки: время от длины отрезка", "длина отрезка, пикс.", Benchmark::lineNames());
    m_charts[2]->setup("Фигуры: время от числа фигур", "число фигур", Benchmark::shapeNames());
    m_charts[3]->setup("Фигуры: время от радиуса", "радиус (у эллипса большая полуось), пикс.", Benchmark::shapeNames());
    return tab;
}

void MainWindow::algorithmChanged() //при смене алгоритма показывает/скрывает нужные поля ввода
{
    int a = m_algo->currentIndex();
    bool line = a <= 4;
    m_val[2]->setVisible(line);
    m_lab[2]->setVisible(line);
    m_val[3]->setVisible(line);
    m_lab[3]->setVisible(line);
    m_val[4]->setVisible(!line);
    m_lab[4]->setVisible(!line);
    for (int i = 5; i < 8; ++i) {
        m_val[i]->setVisible(a == 6);
        m_lab[i]->setVisible(a == 6);
    }
    m_lab[4]->setText(a == 5 ? "R" : "a");

    if (line)
        m_paramBox->setTitle("Отрезок (x0; y0) - (x1; y1)");
    else if (a == 5)
        m_paramBox->setTitle("Окружность: центр (x0; y0), радиус R");
    else
        m_paramBox->setTitle("Эллипс: центр (x0; y0), полуоси a и b, дуга от и до (0 - 360 = весь эллипс)");
}

//при изменении размеров сетки вызывает VirtualGrid::resize, обновляет GridPainter,
//показывает диапазон координат и очищает протокол
void MainWindow::gridSizeChanged()
{
    m_grid.resize(m_wSpin->value(), m_hSpin->value());
    m_painter->refresh();
    m_range->setText(QString("x от %1 до %2, y от %3 до %4")
                         .arg(m_grid.minX()).arg(m_grid.maxX()).arg(m_grid.minY()).arg(m_grid.maxY()));
    m_trace->clear();
}


//меняет масштаб
void MainWindow::zoomStep(int direction)
{
    m_zoomSpin->setValue(m_zoomSpin->value() + 2 * direction);
}


//очищает VirtualGrid и панель протокола
void MainWindow::clearGrid()
{
    m_grid.clear();
    m_painter->update();
    m_trace->clear();
}

//вызывает нужный метод RasterEngine в зависимости от выбранного алгоритма, затем выводит пошаговый
//протокол в правую панель и просит GridPainter перерисоваться
void MainWindow::draw()
{
    if (m_clearBox->isChecked())
        m_grid.clear();

    int x0 = m_val[0]->value(), y0 = m_val[1]->value();
    int x1 = m_val[2]->value(), y1 = m_val[3]->value();
    int r = m_val[4]->value(), b = m_val[5]->value();

    Trace tr;
    switch (m_algo->currentIndex()) {
    case 0: RasterEngine::stepLine(x0, y0, x1, y1, m_grid, &tr); break;
    case 1: RasterEngine::ddaLine(x0, y0, x1, y1, m_grid, &tr); break;
    case 2: RasterEngine::bresenhamLine(x0, y0, x1, y1, m_grid, &tr); break;
    case 3: RasterEngine::wuLine(x0, y0, x1, y1, m_grid, &tr); break;
    case 4: RasterEngine::castlePitteway(x0, y0, x1, y1, m_grid, &tr); break;
    case 5: RasterEngine::bresenhamCircle(x0, y0, r, m_grid, &tr); break;
    case 6: RasterEngine::pittewayArc(x0, y0, r, b, m_val[6]->value(), m_val[7]->value(), m_grid, &tr); break;
    }

    QString text;
    for (size_t i = 0; i < tr.size(); ++i)
        text += QString::fromUtf8(tr[i].c_str()) + "\n";
    m_trace->setPlainText(text);
    m_painter->update();
}

void MainWindow::picked(int x, int y, int button) //обработчик клика по клетке
{
    int a = m_algo->currentIndex();
    if (button == Qt::LeftButton) {
        m_val[0]->setValue(x);
        m_val[1]->setValue(y);
    } else if (button == Qt::RightButton) {
        if (a <= 4) {
            m_val[2]->setValue(x);
            m_val[3]->setValue(y);
        } else if (a == 5) {
            m_val[4]->setValue((int)std::floor(std::hypot((double)(x - m_val[0]->value()), (double)(y - m_val[1]->value())) + 0.5));
        } else {
            m_val[4]->setValue(std::abs(x - m_val[0]->value()));
            m_val[5]->setValue(std::abs(y - m_val[1]->value()));
        }
    } else {
        return;
    }
    if (m_clearBox->isChecked())
        draw();
}

void MainWindow::hovered(int x, int y, bool inside) //выводит в строку состояния границы клетки и её яркость
{
    if (!inside) {
        statusBar()->clearMessage();
        return;
    }
    statusBar()->showMessage(QString("Пиксель (%1; %2): центр в точке (%1; %2), занимает x от %3 до %4, y от %5 до %6, яркость %7")
                                 .arg(x).arg(y).arg(x - 0.5).arg(x + 0.5).arg(y - 0.5).arg(y + 0.5)
                                 .arg(m_grid.at(x, y), 0, 'f', 3));
}

void MainWindow::startBenchmark() //стресс-тест в отдельном потоке
{
    for (int i = 0; i < 4; ++i)
        m_charts[i]->clearData();
    m_summary->clear();
    m_bar->setValue(0);
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_saveBtn->setEnabled(false);

    m_thread = new QThread(this);
    m_bench = new Benchmark(m_linesSpin->value(), m_shapesSpin->value(), m_repSpin->value());
    m_bench->moveToThread(m_thread);

    connect(m_thread, SIGNAL(started()), m_bench, SLOT(run()));
    connect(m_bench, SIGNAL(point(int,int,double,double)), this, SLOT(benchPoint(int,int,double,double)));
    connect(m_bench, SIGNAL(progress(int)), m_bar, SLOT(setValue(int)));
    connect(m_bench, SIGNAL(summary(QString)), this, SLOT(benchSummary(QString)));
    connect(m_bench, SIGNAL(finished()), m_thread, SLOT(quit()));
    connect(m_thread, SIGNAL(finished()), m_bench, SLOT(deleteLater()));
    connect(m_thread, SIGNAL(finished()), this, SLOT(benchFinished()));
    m_thread->start();
}

void MainWindow::stopBenchmark()
{
    if (m_bench)
        m_bench->cancel();
}

void MainWindow::benchPoint(int chart, int series, double x, double ms)
{
    m_charts[chart]->addPoint(series, x, ms);
}

void MainWindow::benchSummary(const QString &text)
{
    m_summary->setPlainText(text);
}

void MainWindow::benchFinished()
{
    m_bench = 0;
    m_thread->deleteLater();
    m_thread = 0;
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_saveBtn->setEnabled(true);
}

void MainWindow::saveResults() // сохраняет PNG-снимки графиков, CSV и summary.txt
{
    QString dir = QFileDialog::getExistingDirectory(this, "Куда сохранить результаты");
    if (dir.isEmpty())
        return;

    QString csv = "график;алгоритм;x;время, мс\n";
    for (int i = 0; i < 4; ++i) {
        csv += m_charts[i]->csv();
        m_charts[i]->grab().save(QDir(dir).filePath(QString("chart%1.png").arg(i + 1)));
    }
    QFile f(QDir(dir).filePath("benchmark.csv"));
    if (f.open(QIODevice::WriteOnly)) {
        f.write("\xEF\xBB\xBF");   // BOM, чтобы Excel правильно прочитал кириллицу
        f.write(csv.toUtf8());
    }
    QFile s(QDir(dir).filePath("summary.txt"));
    if (s.open(QIODevice::WriteOnly))
        s.write(m_summary->toPlainText().toUtf8());
    statusBar()->showMessage("Сохранено в " + dir, 5000);
}

void MainWindow::closeEvent(QCloseEvent *ev) //при закрытии окна корректно останавливает фоновый поток
{
    if (m_thread) {
        if (m_bench)
            m_bench->cancel();
        m_thread->quit();
        m_thread->wait();
    }
    ev->accept();
}
