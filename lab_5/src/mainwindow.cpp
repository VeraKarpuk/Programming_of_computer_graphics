#include "mainwindow.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <cmath>

#include "benchmarkdialog.h"
#include "canvas.h"
#include "segmentclip.h"
#include "testcases.h"
#include "testdialog.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QString::fromUtf8("Лабораторная работа 5. Алгоритмы отсечения"));
    resize(1280, 800);

    canvas_ = new Canvas(this);
    setCentralWidget(canvas_);

    // панель управления справа
    QWidget *panel = new QWidget(this);
    QVBoxLayout *lay = new QVBoxLayout(panel);

    lay->addWidget(new QLabel(QString::fromUtf8("Режим:"), panel));
    modeBox_ = new QComboBox(panel);
    modeBox_->addItem(QString::fromUtf8("Отрезки / окно: Лианг-Барски"));
    modeBox_->addItem(QString::fromUtf8("Отрезки / окно: Сазерленд-Коэн"));
    modeBox_->addItem(QString::fromUtf8("Отрезки / выпуклый многоугольник: Сайрус-Бек"));
    modeBox_->addItem(QString::fromUtf8("Многоугольники / окно: Уайлер-Атертон"));
    modeBox_->addItem(QString::fromUtf8("Многоугольники / выпуклый отсекатель: Уайлер-Атертон"));
    modeBox_->addItem(QString::fromUtf8("Большие данные: квадродерево"));
    lay->addWidget(modeBox_);

    QGroupBox *dataBox = new QGroupBox(QString::fromUtf8("Данные"), panel);
    QVBoxLayout *dl = new QVBoxLayout(dataBox);
    QPushButton *openBtn = new QPushButton(QString::fromUtf8("Открыть файл…"), dataBox);
    QPushButton *saveBtn = new QPushButton(QString::fromUtf8("Сохранить сцену…"), dataBox);
    QPushButton *imgBtn = new QPushButton(QString::fromUtf8("Сохранить картинку…"), dataBox);
    dl->addWidget(openBtn);
    dl->addWidget(saveBtn);
    dl->addWidget(imgBtn);

    presetBox_ = new QComboBox(dataBox);
    presetBox_->addItem(QString::fromUtf8("Отрезки: учебный пример"));
    presetBox_->addItem(QString::fromUtf8("Отрезки: граничные случаи"));
    presetBox_->addItem(QString::fromUtf8("Многоугольник: вогнутая буква П"));
    presetBox_->addItem(QString::fromUtf8("Многоугольник: с дырой"));
    presetBox_->addItem(QString::fromUtf8("Многоугольник: самопересекающаяся звезда"));
    presetBox_->addItem(QString::fromUtf8("Многоугольник: рёбра через вершины окна"));
    presetBox_->addItem(QString::fromUtf8("Отсекатель: шестиугольник"));
    presetBox_->addItem(QString::fromUtf8("Отсекатель: пятиугольник"));
    presetBox_->addItem(QString::fromUtf8("Отсекатель: треугольник"));
    QPushButton *presetBtn = new QPushButton(QString::fromUtf8("Подставить пример"), dataBox);
    dl->addWidget(presetBox_);
    dl->addWidget(presetBtn);

    randomCount_ = new QSpinBox(dataBox);
    randomCount_->setRange(1, 20000);
    randomCount_->setValue(30);
    QPushButton *randBtn = new QPushButton(QString::fromUtf8("Случайные отрезки"), dataBox);
    dl->addWidget(randomCount_);
    dl->addWidget(randBtn);

    QCheckBox *aabbCheck = new QCheckBox(QString::fromUtf8("AABB-отбраковка перед отсечением"), dataBox);
    dl->addWidget(aabbCheck);
    QPushButton *fitBtn = new QPushButton(QString::fromUtf8("Вписать в экран"), dataBox);
    QPushButton *testBtn = new QPushButton(QString::fromUtf8("Тест-кейсы…"), dataBox);
    QPushButton *benchBtn = new QPushButton(QString::fromUtf8("Бенчмарк для отчёта…"), dataBox);
    dl->addWidget(fitBtn);
    dl->addWidget(testBtn);
    dl->addWidget(benchBtn);
    lay->addWidget(dataBox);

    bigBox_ = new QGroupBox(QString::fromUtf8("Большие данные"), panel);
    QVBoxLayout *bl = new QVBoxLayout(bigBox_);
    bigN_ = new QSpinBox(bigBox_);
    bigN_->setRange(1000, 2000000);
    bigN_->setSingleStep(50000);
    bigN_->setValue(500000);
    depthSpin_ = new QSpinBox(bigBox_);
    depthSpin_->setRange(1, 14);
    depthSpin_->setValue(8);
    capSpin_ = new QSpinBox(bigBox_);
    capSpin_->setRange(1, 512);
    capSpin_->setValue(32);
    dynSpin_ = new QSpinBox(bigBox_);
    dynSpin_->setRange(0, 20000);
    dynSpin_->setSingleStep(500);
    dynSpin_->setValue(0);
    treeCheck_ = new QCheckBox(QString::fromUtf8("использовать квадродерево"), bigBox_);
    treeCheck_->setChecked(true);
    animCheck_ = new QCheckBox(QString::fromUtf8("двигать окно (анимация)"), bigBox_);
    showAllCheck_ = new QCheckBox(QString::fromUtf8("рисовать всю базу (медленно)"), bigBox_);
    QPushButton *genBtn = new QPushButton(QString::fromUtf8("Создать базу и построить дерево"), bigBox_);
    bl->addWidget(new QLabel(QString::fromUtf8("Число отрезков:"), bigBox_));
    bl->addWidget(bigN_);
    bl->addWidget(new QLabel(QString::fromUtf8("Максимальная глубина:"), bigBox_));
    bl->addWidget(depthSpin_);
    bl->addWidget(new QLabel(QString::fromUtf8("Ёмкость листа:"), bigBox_));
    bl->addWidget(capSpin_);
    bl->addWidget(new QLabel(QString::fromUtf8("Обновлять отрезков за кадр:"), bigBox_));
    bl->addWidget(dynSpin_);
    bl->addWidget(genBtn);
    bl->addWidget(treeCheck_);
    bl->addWidget(animCheck_);
    bl->addWidget(showAllCheck_);
    lay->addWidget(bigBox_);
    bigBox_->setVisible(false);

    statsText_ = new QPlainTextEdit(panel);
    statsText_->setReadOnly(true);
    statsText_->setMinimumHeight(190);
    lay->addWidget(new QLabel(QString::fromUtf8("Статистика:"), panel));
    lay->addWidget(statsText_, 1);

    QLabel *help = new QLabel(QString::fromUtf8(
        "Мышь: левая - тянуть углы окна, вершины, концы отрезков; внутри окна - переместить; "
        "правая/средняя или пустое место - сдвиг вида; колесо - масштаб вида; "
        "Ctrl+колесо - масштаб окна; Shift+колесо - масштаб отрезков/многоугольников; "
        "Shift+тянуть - новый отрезок (в режиме многоугольников: Shift+клик - вершина, "
        "Ctrl+клик - новый контур); двойной щелчок по точке - удалить."), panel);
    help->setWordWrap(true);
    lay->addWidget(help);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidget(panel);
    scroll->setWidgetResizable(true);
    scroll->setMinimumWidth(330);
    QDockWidget *dock = new QDockWidget(QString::fromUtf8("Управление"), this);
    dock->setWidget(scroll);
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    QMenu *file = menuBar()->addMenu(QString::fromUtf8("Файл"));
    QAction *aOpen = file->addAction(QString::fromUtf8("Открыть…"));
    aOpen->setShortcut(QKeySequence::Open);
    QAction *aSave = file->addAction(QString::fromUtf8("Сохранить сцену…"));
    aSave->setShortcut(QKeySequence::Save);
    file->addSeparator();
    QAction *aQuit = file->addAction(QString::fromUtf8("Выход"));
    connect(aOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(aSave, &QAction::triggered, this, &MainWindow::saveFile);
    connect(aQuit, &QAction::triggered, this, &QWidget::close);

    // соединения
    connect(modeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModeChanged);
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveFile);
    connect(imgBtn, &QPushButton::clicked, this, &MainWindow::saveImage);
    connect(presetBtn, &QPushButton::clicked, this, &MainWindow::applyPreset);
    connect(randBtn, &QPushButton::clicked, this, &MainWindow::randomSegments);
    connect(fitBtn, &QPushButton::clicked, canvas_, &Canvas::fitView);
    connect(testBtn, &QPushButton::clicked, this, &MainWindow::showTests);
    connect(benchBtn, &QPushButton::clicked, this, &MainWindow::showBenchmark);
    connect(genBtn, &QPushButton::clicked, this, &MainWindow::generateBig);
    connect(aabbCheck, &QCheckBox::toggled, canvas_, &Canvas::setPrefilter);
    connect(treeCheck_, &QCheckBox::toggled, canvas_, &Canvas::setUseTree);
    connect(animCheck_, &QCheckBox::toggled, canvas_, &Canvas::setAnimate);
    connect(showAllCheck_, &QCheckBox::toggled, canvas_, &Canvas::setShowAll);
    connect(dynSpin_, QOverload<int>::of(&QSpinBox::valueChanged), canvas_, &Canvas::setDynamicPerFrame);
    connect(canvas_, &Canvas::statsChanged, statsText_, &QPlainTextEdit::setPlainText);
    connect(canvas_, &Canvas::cursorMoved, this, [this](const QString &t) { statusBar()->showMessage(t); });
    connect(canvas_, &Canvas::notice, this, [this](const QString &t) { statusBar()->showMessage(t, 4000); });

    onModeChanged(0);
}

void MainWindow::ensureDataForMode() {
    geo::SceneData s = canvas_->scene();
    Canvas::Mode m = canvas_->mode();
    bool changed = false;
    if ((m == Canvas::PolyWindow || m == Canvas::PolyConvex) && s.polygons.empty()) {
        geo::Ring u = {geo::Point(1, 1), geo::Point(9, 1), geo::Point(9, 9), geo::Point(7, 9),
                       geo::Point(7, 3), geo::Point(3, 3), geo::Point(3, 9), geo::Point(1, 9)};
        s.polygons.push_back(u);
        if (m == Canvas::PolyWindow) s.window = geo::Rect(0, 5, 10, 10);
        changed = true;
    }
    if (changed) canvas_->setScene(s);
}

void MainWindow::onModeChanged(int index) {
    Canvas::Mode m = static_cast<Canvas::Mode>(index);
    bigBox_->setVisible(m == Canvas::BigData);
    canvas_->setMode(m);
    ensureDataForMode();
    if (m == Canvas::BigData && !bigReady_) generateBig();
}

void MainWindow::openFile() {
    QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("Открыть файл"), QString(),
                                                QString::fromUtf8("Текстовые файлы (*.txt *.in *.dat);;Все файлы (*)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8("Не удалось открыть файл"));
        return;
    }
    std::string text = QString::fromUtf8(f.readAll()).toStdString();
    geo::SceneData s;
    std::string err;
    if (!geo::parseScene(text, s, err)) {
        QMessageBox::warning(this, windowTitle(),
                             QString::fromUtf8("Ошибка в файле: ") + QString::fromUtf8(err.c_str()));
        return;
    }
    // то, чего в файле нет, остаётся от текущей сцены
    const geo::SceneData &cur = canvas_->scene();
    if (s.clipperPoly.empty()) s.clipperPoly = cur.clipperPoly;
    canvas_->setScene(s);
    ensureDataForMode();
    statusBar()->showMessage(QString::fromUtf8("Загружено отрезков: %1, многоугольников: %2")
                                 .arg(s.segments.size()).arg(s.polygons.size()), 5000);
}

void MainWindow::saveFile() {
    QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("Сохранить сцену"), "scene.txt",
                                                QString::fromUtf8("Текстовые файлы (*.txt)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8("Не удалось записать файл"));
        return;
    }
    f.write(QByteArray(geo::serializeScene(canvas_->scene()).c_str()));
}

void MainWindow::saveImage() {
    QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("Сохранить картинку"), "clip.png", "PNG (*.png)");
    if (path.isEmpty()) return;
    canvas_->grab().save(path);
}

void MainWindow::loadPolygons(int kind) {
    geo::SceneData s = canvas_->scene();
    s.polygons.clear();
    s.window = geo::Rect(0, 0, 10, 10);
    auto sq = [](double x1, double y1, double x2, double y2, bool ccw) {
        if (ccw) return geo::Ring{geo::Point(x1, y1), geo::Point(x2, y1), geo::Point(x2, y2), geo::Point(x1, y2)};
        return geo::Ring{geo::Point(x1, y1), geo::Point(x1, y2), geo::Point(x2, y2), geo::Point(x2, y1)};
    };
    if (kind == 0) {
        s.polygons.push_back({geo::Point(1, 1), geo::Point(9, 1), geo::Point(9, 9), geo::Point(7, 9),
                              geo::Point(7, 3), geo::Point(3, 3), geo::Point(3, 9), geo::Point(1, 9)});
        s.window = geo::Rect(0, 5, 10, 10);
    } else if (kind == 1) {
        s.polygons.push_back(sq(-2, -2, 12, 12, true));
        s.polygons.push_back(sq(3, 3, 7, 7, false));
        s.window = geo::Rect(1, 1, 9, 5);
    } else if (kind == 2) {
        s.polygons.push_back({geo::Point(5, 0), geo::Point(8, 10), geo::Point(0, 4), geo::Point(10, 4), geo::Point(2, 10)});
        s.window = geo::Rect(1, 1, 9, 7);
    } else {
        s.polygons.push_back({geo::Point(5, -5), geo::Point(15, 5), geo::Point(5, 15), geo::Point(-5, 5)});
    }
    canvas_->setScene(s);
}

void MainWindow::applyPreset() {
    int k = presetBox_->currentIndex();
    geo::SceneData s = canvas_->scene();
    if (k == 0) {
        // учебный пример: те же отрезки, что и при запуске
        geo::SceneData d;
        d.window = geo::Rect(-4, -3, 5, 4);
        d.segments = {geo::Segment(-6, -1, 7, 2),  geo::Segment(-2, -2, 3, 3), geo::Segment(-7, 5, -1, 6),
                      geo::Segment(-5, -4, 6, 5),  geo::Segment(0, -6, 0, 6),  geo::Segment(6, -2, 8, 3),
                      geo::Segment(-4, -3, 5, -3), geo::Segment(-3, 6, 2, 1),  geo::Segment(1, 1, 1, 1)};
        d.clipperPoly = s.clipperPoly;
        d.polygons = s.polygons;
        canvas_->setScene(d);
    } else if (k == 1) {
        s.window = geo::testWindow();
        s.segments.clear();
        for (const geo::SegTest &t : geo::segmentTests()) s.segments.push_back(t.seg);
        canvas_->setScene(s);
    } else if (k >= 2 && k <= 5) {
        loadPolygons(k - 2);
    } else {
        int n = k == 6 ? 6 : (k == 7 ? 5 : 3);
        s.clipperPoly.clear();
        for (int i = 0; i < n; ++i)
            s.clipperPoly.push_back(geo::Point(4.5 * std::cos(2 * M_PI * i / n + M_PI / 2),
                                               0.5 + 4.5 * std::sin(2 * M_PI * i / n + M_PI / 2)));
        canvas_->setScene(s);
    }
}

void MainWindow::randomSegments() {
    geo::SceneData s = canvas_->scene();
    geo::Rect area(s.window.xmin - 6, s.window.ymin - 6, s.window.xmax + 6, s.window.ymax + 6);
    if (canvas_->mode() == Canvas::CyrusBeck && !s.clipperPoly.empty()) {
        double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
        for (const geo::Point &p : s.clipperPoly) {
            x0 = std::min(x0, p.x); x1 = std::max(x1, p.x);
            y0 = std::min(y0, p.y); y1 = std::max(y1, p.y);
        }
        area = geo::Rect(x0 - 6, y0 - 6, x1 + 6, y1 + 6);
    }
    static unsigned seed = 1;
    s.segments = geo::randomSegments(randomCount_->value(), area, 12.0, seed++);
    canvas_->setScene(s);
}

void MainWindow::generateBig() {
    canvas_->generateBigData(bigN_->value(), depthSpin_->value(), capSpin_->value());
    bigReady_ = true;
    canvas_->setUseTree(treeCheck_->isChecked());
    canvas_->setShowAll(showAllCheck_->isChecked() || bigN_->value() <= 50000);
    if (bigN_->value() <= 50000) showAllCheck_->setChecked(true);
    canvas_->setDynamicPerFrame(dynSpin_->value());
}

void MainWindow::showTests() {
    TestDialog *dlg = new TestDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &TestDialog::showSegmentCase, this, [this](int i) {
        std::vector<geo::SegTest> tests = geo::segmentTests();
        geo::SceneData s = canvas_->scene();
        s.window = geo::testWindow();
        s.segments.assign(1, tests[i].seg);
        modeBox_->setCurrentIndex(Canvas::LiangBarsky);
        canvas_->setScene(s);
    });
    connect(dlg, &TestDialog::showPolygonCase, this, [this](int i) {
        std::vector<geo::PolyTest> tests = geo::polygonTests();
        if (!geo::isConvex(tests[i].clipper)) {
            statusBar()->showMessage(QString::fromUtf8("Вогнутый отсекатель проверяется только в таблице"), 5000);
            return;
        }
        geo::SceneData s = canvas_->scene();
        s.polygons = tests[i].subject;
        s.clipperPoly = tests[i].clipper;
        modeBox_->setCurrentIndex(Canvas::PolyConvex);
        canvas_->setScene(s);
    });
    dlg->show();
}

void MainWindow::showBenchmark() {
    BenchmarkDialog dlg(this);
    dlg.exec();
}
