#include "MainWindow.h"
#include "HistogramWidget.h"
#include "ImageConvert.h"
#include "ImageView.h"
#include "SliderSpin.h"
#include "core/TestImages.h"

#include <QAction>
#include <QMenu>
#include <algorithm>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <chrono>

#define U8(s) QString::fromUtf8(s)

static const int kTestW = 800, kTestH = 600;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_busy(false), m_jobDone(false), m_pending(false) {
    setWindowTitle(U8("Лабораторная работа 3 (вариант 8): ВЧ-фильтры и пороговая обработка"));
    resize(1480, 900);
    buildMenus();
    buildUi();
    m_pollTimer.setInterval(40);
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_pollTimer, SIGNAL(timeout()), this, SLOT(onPoll()));
    connect(&m_debounce, SIGNAL(timeout()), this, SLOT(applyNow()));
    updateControlStates();
    statusBar()->showMessage(U8("Откройте изображение (Файл → Открыть) или выберите его из базы справа."));
}

MainWindow::~MainWindow() {
    if (m_worker.joinable()) m_worker.join();
}

ip::EdgeMode MainWindow::edgeMode() const {
    return m_edgeCombo->currentIndex() == 0 ? ip::EdgeMode::Clamp : ip::EdgeMode::Reflect;
}

// ------------------------------------------------------------------ меню ----
static QAction* addMenuAction(QMenu* menu, QObject* receiver, const QString& text, const char* slot,
                              const QKeySequence& shortcut = QKeySequence()) {
    QAction* act = new QAction(text, receiver);
    if (!shortcut.isEmpty()) act->setShortcut(shortcut);
    QObject::connect(act, SIGNAL(triggered()), receiver, slot);
    menu->addAction(act);
    return act;
}

void MainWindow::buildMenus() {
    QMenu* file = menuBar()->addMenu(U8("&Файл"));
    addMenuAction(file, this, U8("Открыть изображение…"), SLOT(openImage()), QKeySequence::Open);
    addMenuAction(file, this, U8("Сохранить результат…"), SLOT(saveResult()), QKeySequence::Save);
    addMenuAction(file, this, U8("Сохранить гистограммы (до/после)…"), SLOT(saveHistograms()));
    file->addSeparator();
    addMenuAction(file, this, U8("Выход"), SLOT(close()), QKeySequence::Quit);

    QMenu* tools = menuBar()->addMenu(U8("&Сервис"));
    addMenuAction(tools, this, U8("Результат → исходное изображение"), SLOT(useResultAsSource()));
    addMenuAction(tools, this, U8("Сохранить базу тестовых изображений в папку…"), SLOT(saveTestBase()));

    QMenu* help = menuBar()->addMenu(U8("&Справка"));
    addMenuAction(help, this, U8("О программе"), SLOT(showAbout()));
}

// ------------------------------------------------------------- интерфейс ----
void MainWindow::buildUi() {
    QWidget* central = new QWidget;
    QHBoxLayout* hl = new QHBoxLayout(central);
    hl->setContentsMargins(4, 4, 4, 4);

    QScrollArea* scroll = new QScrollArea;
    scroll->setWidget(buildControlPanel());
    scroll->setWidgetResizable(true);
    scroll->setFixedWidth(410);
    scroll->setFrameShape(QFrame::NoFrame);

    m_viewBefore = new ImageView;
    m_viewBefore->setCaption(U8("Исходное"));
    m_viewAfter = new ImageView;
    m_viewAfter->setCaption(U8("Результат"));
    QSplitter* views = new QSplitter(Qt::Horizontal);
    views->addWidget(m_viewBefore);
    views->addWidget(m_viewAfter);

    m_histBefore = new HistogramWidget;
    m_histAfter = new HistogramWidget;
    QWidget* histRow = new QWidget;
    QHBoxLayout* hh = new QHBoxLayout(histRow);
    hh->setContentsMargins(0, 0, 0, 0);
    hh->addWidget(m_histBefore);
    hh->addWidget(m_histAfter);

    QSplitter* vs = new QSplitter(Qt::Vertical);
    vs->addWidget(views);
    vs->addWidget(histRow);
    vs->setStretchFactor(0, 5);
    vs->setStretchFactor(1, 1);

    hl->addWidget(scroll);
    hl->addWidget(vs, 1);
    setCentralWidget(central);

    // статус-бар
    m_statusLabel = new QLabel;
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedWidth(220);
    statusBar()->addPermanentWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_progressBar);

    // база тестовых изображений
    QDockWidget* dock = new QDockWidget(U8("База тестовых изображений"), this);
    QWidget* dw = new QWidget;
    QVBoxLayout* dl = new QVBoxLayout(dw);
    m_testList = new QListWidget;
    const std::vector<ip::TestImageInfo>& cat = ip::testImageCatalog();
    for (size_t i = 0; i < cat.size(); ++i) {
        QListWidgetItem* it = new QListWidgetItem(U8(cat[i].title));
        it->setToolTip(U8(cat[i].description));
        m_testList->addItem(it);
    }
    QLabel* hint = new QLabel(U8("Щёлкните по элементу — изображение (800×600) будет сгенерировано. "
                                 "Подсказка с описанием — при наведении."));
    hint->setWordWrap(true);
    QPushButton* saveBase = new QPushButton(U8("Сохранить всю базу в папку…"));
    dl->addWidget(hint);
    dl->addWidget(m_testList, 1);
    dl->addWidget(saveBase);
    dock->setWidget(dw);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    // ---- соединения (все виджеты уже созданы) ----
    connect(m_testList, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(loadTestImage(QListWidgetItem*)));
    connect(saveBase, SIGNAL(clicked()), this, SLOT(saveTestBase()));
    connect(m_viewBefore, SIGNAL(viewChanged(double,QPointF,bool)), m_viewAfter, SLOT(applyView(double,QPointF,bool)));
    connect(m_viewAfter, SIGNAL(viewChanged(double,QPointF,bool)), m_viewBefore, SLOT(applyView(double,QPointF,bool)));

    QComboBox* combos[] = { m_opCombo, m_edgeCombo, m_shMethod, m_shBlurType, m_glMethod, m_adMethod };
    for (size_t i = 0; i < sizeof(combos) / sizeof(combos[0]); ++i) {
        connect(combos[i], SIGNAL(currentIndexChanged(int)), this, SLOT(updateControlStates()));
        connect(combos[i], SIGNAL(currentIndexChanged(int)), this, SLOT(scheduleApply()));
    }
    SliderSpin* sliders[] = { m_shStrength, m_shBoost, m_shSigma, m_shBoxR, m_shThresh, m_glManual,
                              m_adWindow, m_adC, m_adK, m_adR, m_clTilesX, m_clTilesY, m_clClip };
    for (size_t i = 0; i < sizeof(sliders) / sizeof(sliders[0]); ++i)
        connect(sliders[i], SIGNAL(valueChanged(double)), this, SLOT(scheduleApply()));
    QCheckBox* checks[] = { m_shDetail, m_glInvert, m_adInvert, m_adNaive };
    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); ++i)
        connect(checks[i], SIGNAL(toggled(bool)), this, SLOT(scheduleApply()));
    connect(m_threadsSpin, SIGNAL(valueChanged(int)), this, SLOT(scheduleApply()));
    connect(m_applyBtn, SIGNAL(clicked()), this, SLOT(applyNow()));
    connect(m_useResultBtn, SIGNAL(clicked()), this, SLOT(useResultAsSource()));
}

QWidget* MainWindow::buildControlPanel() {
    QWidget* panel = new QWidget;
    QVBoxLayout* v = new QVBoxLayout(panel);

    QGroupBox* gOp = new QGroupBox(U8("Операция"));
    QVBoxLayout* lo = new QVBoxLayout(gOp);
    m_opCombo = new QComboBox;
    m_opCombo->addItem(U8("Повышение резкости (ВЧ-фильтры)"));
    m_opCombo->addItem(U8("Глобальная пороговая обработка"));
    m_opCombo->addItem(U8("Адаптивная пороговая обработка (окно)"));
    m_opCombo->addItem(U8("CLAHE (дополнительно)"));
    m_stack = new QStackedWidget;
    m_stack->addWidget(buildSharpenPage());
    m_stack->addWidget(buildGlobalPage());
    m_stack->addWidget(buildAdaptivePage());
    m_stack->addWidget(buildClahePage());
    lo->addWidget(m_opCombo);
    lo->addWidget(m_stack);
    v->addWidget(gOp);

    QGroupBox* gCommon = new QGroupBox(U8("Общие параметры"));
    QFormLayout* f = new QFormLayout(gCommon);
    m_edgeCombo = new QComboBox;
    m_edgeCombo->addItem(U8("Clamp (дублирование граничных пикселей)"));
    m_edgeCombo->addItem(U8("Reflect (зеркальное отражение)"));
    m_edgeCombo->setCurrentIndex(1);
    m_threadsSpin = new QSpinBox;
    m_threadsSpin->setRange(0, 64);
    m_threadsSpin->setValue(0);
    m_threadsSpin->setSpecialValueText(U8("авто"));
    m_liveCheck = new QCheckBox(U8("Пересчитывать при изменении параметров"));
    m_liveCheck->setChecked(true);
    m_applyBtn = new QPushButton(U8("Применить"));
    m_useResultBtn = new QPushButton(U8("Результат → исходное"));
    m_useResultBtn->setEnabled(false);
    f->addRow(U8("Обработка краёв:"), m_edgeCombo);
    f->addRow(U8("Потоков (1 = однопоточно):"), m_threadsSpin);
    f->addRow(m_liveCheck);
    f->addRow(m_applyBtn);
    f->addRow(m_useResultBtn);
    v->addWidget(gCommon);
    v->addStretch(1);
    return panel;
}

QWidget* MainWindow::buildSharpenPage() {
    QWidget* pg = new QWidget;
    QFormLayout* f = new QFormLayout(pg);
    m_shMethod = new QComboBox;
    m_shMethod->addItem(U8("Лапласиан, 4 соседа"));
    m_shMethod->addItem(U8("Лапласиан, 8 соседей"));
    m_shMethod->addItem(U8("Нерезкое маскирование (unsharp)"));
    m_shMethod->addItem(U8("High-boost: A·f − НЧ(f)"));
    m_shMethod->setCurrentIndex(2);
    m_shStrength = new SliderSpin(0.0, 5.0, 0.1, 1.0, 1);
    m_shBoost = new SliderSpin(1.0, 5.0, 0.1, 2.0, 1);
    m_shBlurType = new QComboBox;
    m_shBlurType->addItem(U8("Гаусс (разделяемая свёртка)"));
    m_shBlurType->addItem(U8("Бокс (разделяемая свёртка)"));
    m_shSigma = new SliderSpin(0.3, 15.0, 0.1, 1.5, 1);
    m_shBoxR = new SliderSpin(1, 25, 1, 2, 0);
    m_shThresh = new SliderSpin(0, 60, 1, 0, 0);
    m_shDetail = new QCheckBox(U8("Показать только ВЧ-составляющую (+128)"));
    f->addRow(U8("Метод:"), m_shMethod);
    f->addRow(U8("Сила k:"), m_shStrength);
    f->addRow(U8("Коэффициент A:"), m_shBoost);
    f->addRow(U8("НЧ-фильтр:"), m_shBlurType);
    f->addRow(U8("σ Гаусса:"), m_shSigma);
    f->addRow(U8("Радиус бокса:"), m_shBoxR);
    f->addRow(U8("Порог |ВЧ| (шум):"), m_shThresh);
    f->addRow(m_shDetail);
    return pg;
}

QWidget* MainWindow::buildGlobalPage() {
    QWidget* pg = new QWidget;
    QFormLayout* f = new QFormLayout(pg);
    m_glMethod = new QComboBox;
    m_glMethod->addItem(U8("Оцу (макс. межклассовая дисперсия)"));
    m_glMethod->addItem(U8("Итерационный (ISODATA)"));
    m_glMethod->addItem(U8("Треугольник (Zack)"));
    m_glMethod->addItem(U8("Ручной порог"));
    m_glManual = new SliderSpin(0, 255, 1, 128, 0);
    m_glInvert = new QCheckBox(U8("Инвертировать (тёмные объекты → белые)"));
    m_thresholdLabel = new QLabel(U8("Порог: —"));
    QFont bold = m_thresholdLabel->font();
    bold.setBold(true);
    m_thresholdLabel->setFont(bold);
    f->addRow(U8("Метод:"), m_glMethod);
    f->addRow(U8("Порог (ручной):"), m_glManual);
    f->addRow(m_glInvert);
    f->addRow(m_thresholdLabel);
    return pg;
}

QWidget* MainWindow::buildAdaptivePage() {
    QWidget* pg = new QWidget;
    QFormLayout* f = new QFormLayout(pg);
    m_adMethod = new QComboBox;
    m_adMethod->addItem(U8("Локальное среднее: T = mean − C"));
    m_adMethod->addItem(U8("Савола (дополнительно)"));
    m_adWindow = new SliderSpin(3, 201, 2, 25, 0);
    m_adC = new SliderSpin(-50, 50, 1, 10, 0);
    m_adK = new SliderSpin(0.0, 1.0, 0.01, 0.2, 2);
    m_adR = new SliderSpin(16, 255, 1, 128, 0);
    m_adInvert = new QCheckBox(U8("Инвертировать (тёмные объекты → белые)"));
    m_adNaive = new QCheckBox(U8("Без интегрального изображения (медленно, для сравнения)"));
    f->addRow(U8("Метод:"), m_adMethod);
    f->addRow(U8("Окно (нечётное):"), m_adWindow);
    f->addRow(U8("Константа C:"), m_adC);
    f->addRow(U8("k (Савола):"), m_adK);
    f->addRow(U8("R (Савола):"), m_adR);
    f->addRow(m_adInvert);
    f->addRow(m_adNaive);
    return pg;
}

QWidget* MainWindow::buildClahePage() {
    QWidget* pg = new QWidget;
    QFormLayout* f = new QFormLayout(pg);
    m_clTilesX = new SliderSpin(2, 32, 1, 8, 0);
    m_clTilesY = new SliderSpin(2, 32, 1, 8, 0);
    m_clClip = new SliderSpin(1.0, 10.0, 0.5, 3.0, 1);
    f->addRow(U8("Плиток по X:"), m_clTilesX);
    f->addRow(U8("Плиток по Y:"), m_clTilesY);
    f->addRow(U8("Предел контраста:"), m_clClip);
    return pg;
}

void MainWindow::updateControlStates() {
    const int op = m_opCombo->currentIndex();
    m_stack->setCurrentIndex(op);
    const int sm = m_shMethod->currentIndex();  // 0,1 — лапласианы; 2 — unsharp; 3 — high-boost
    const bool blurBased = (sm == 2 || sm == 3);
    m_shStrength->setEnabled(sm <= 2);
    m_shBoost->setEnabled(sm == 3);
    m_shBlurType->setEnabled(blurBased);
    m_shSigma->setEnabled(blurBased && m_shBlurType->currentIndex() == 0);
    m_shBoxR->setEnabled(blurBased && m_shBlurType->currentIndex() == 1);
    m_shThresh->setEnabled(sm == 2);
    m_glManual->setEnabled(m_glMethod->currentIndex() == 3);
    m_adC->setEnabled(m_adMethod->currentIndex() == 0);
    m_adK->setEnabled(m_adMethod->currentIndex() == 1);
    m_adR->setEnabled(m_adMethod->currentIndex() == 1);
    m_edgeCombo->setEnabled(op == 0 || op == 2);
}

// ----------------------------------------------------------- файлы/источник --
void MainWindow::openImage() {
    const QString fn = QFileDialog::getOpenFileName(this, U8("Открыть изображение"), QString(),
        U8("Изображения (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.gif);;Все файлы (*)"));
    if (fn.isEmpty()) return;
    QImage img;
    if (!img.load(fn)) {
        QMessageBox::warning(this, U8("Ошибка"), U8("Не удалось загрузить файл."));
        return;
    }
    setSource(img, QFileInfo(fn).fileName());
}

void MainWindow::setSource(const QImage& q, const QString& name) {
    ip::Image im = qimageToImage(q);
    m_source = std::make_shared<const ip::Image>(std::move(im));
    m_viewBefore->setImage(imageToQImage(*m_source));
    m_viewBefore->setCaption(U8("Исходное: ") + name);
    m_srcHist = ip::computeHistograms(*m_source);
    m_histBefore->setData(m_srcHist, U8("До"));
    m_histBefore->setThreshold(-1);
    m_result = ip::Image();
    m_viewAfter->setImage(QImage());
    m_histAfter->clearData();
    m_useResultBtn->setEnabled(false);
    m_statusLabel->setText(U8("%1 × %2, %3").arg(m_source->width).arg(m_source->height)
                           .arg(m_source->channels == 1 ? U8("градации серого") : U8("цветное RGB")));
    if (m_liveCheck->isChecked()) applyNow();
}

void MainWindow::saveResult() {
    if (m_result.empty()) {
        QMessageBox::information(this, U8("Сохранение"), U8("Сначала выполните обработку."));
        return;
    }
    const QString fn = QFileDialog::getSaveFileName(this, U8("Сохранить результат"), "result.png",
                                                    "PNG (*.png);;BMP (*.bmp);;JPEG (*.jpg)");
    if (fn.isEmpty()) return;
    if (!imageToQImage(m_result).save(fn))
        QMessageBox::warning(this, U8("Ошибка"), U8("Не удалось сохранить файл."));
}

void MainWindow::saveHistograms() {
    const QPixmap a = m_histBefore->grab(), b = m_histAfter->grab();
    QPixmap out(a.width() + b.width() + 10, std::max(a.height(), b.height()));
    out.fill(Qt::white);
    QPainter p(&out);
    p.drawPixmap(0, 0, a);
    p.drawPixmap(a.width() + 10, 0, b);
    p.end();
    const QString fn = QFileDialog::getSaveFileName(this, U8("Сохранить гистограммы"), "histograms.png", "PNG (*.png)");
    if (!fn.isEmpty()) out.save(fn);
}

void MainWindow::useResultAsSource() {
    if (m_result.empty()) return;
    setSource(imageToQImage(m_result), U8("результат обработки"));
}

void MainWindow::loadTestImage(QListWidgetItem* item) {
    const int idx = m_testList->row(item);
    const ip::TestImageInfo& info = ip::testImageCatalog()[static_cast<size_t>(idx)];
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const ip::Image im = ip::generateTestImage(idx, kTestW, kTestH, 0);
    QApplication::restoreOverrideCursor();
    setSource(imageToQImage(im), U8(info.title));
    statusBar()->showMessage(U8(info.description), 15000);
}

void MainWindow::saveTestBase() {
    const QString dir = QFileDialog::getExistingDirectory(this, U8("Папка для базы тестовых изображений"));
    if (dir.isEmpty()) return;
    const std::vector<ip::TestImageInfo>& cat = ip::testImageCatalog();
    int saved = 0;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (size_t i = 0; i < cat.size(); ++i) {
        ip::Image truth;
        const ip::Image im = ip::generateTestImage(static_cast<int>(i), kTestW, kTestH, &truth);
        if (imageToQImage(im).save(dir + "/" + cat[i].id + ".png")) ++saved;
        if (cat[i].hasTruth && !truth.empty() && imageToQImage(truth).save(dir + "/" + cat[i].id + "_truth.png")) ++saved;
    }
    QApplication::restoreOverrideCursor();
    QMessageBox::information(this, U8("База изображений"), U8("Сохранено файлов: %1").arg(saved));
}

void MainWindow::showAbout() {
    QMessageBox::about(this, U8("О программе"),
        U8("Лабораторная работа 3, вариант 8 (группа 10А).\n\n"
           "• Высокочастотные фильтры: лапласиан (4/8 соседей), нерезкое маскирование, high-boost.\n"
           "• Глобальная пороговая обработка: Оцу, итерационный, треугольник, ручной.\n"
           "• Адаптивная пороговая обработка: локальное среднее (интегральное изображение), Савола.\n"
           "• Дополнительно: CLAHE.\n\n"
           "Прямой доступ к памяти, края Clamp/Reflect, разделяемые свёртки, многопоточность.\n"
           "Колесо мыши — масштаб, перетаскивание — сдвиг, двойной щелчок — вписать."));
}

// ---------------------------------------------------------------- обработка --
void MainWindow::scheduleApply() {
    if (m_liveCheck->isChecked()) m_debounce.start();
}

void MainWindow::applyNow() {
    if (!m_source) return;
    if (m_busy) { m_pending = true; return; }
    if (m_worker.joinable()) m_worker.join();

    const int op = m_opCombo->currentIndex();
    const ip::EdgeMode edge = edgeMode();

    ip::SharpenParams sp;
    sp.method = static_cast<ip::SharpenParams::Method>(m_shMethod->currentIndex());
    sp.strength = m_shStrength->value();
    sp.boostA = m_shBoost->value();
    sp.boxBlur = (m_shBlurType->currentIndex() == 1);
    sp.sigma = m_shSigma->value();
    sp.boxRadius = static_cast<int>(m_shBoxR->value());
    sp.threshold = static_cast<int>(m_shThresh->value());
    sp.detailOnly = m_shDetail->isChecked();
    sp.edge = edge;

    ip::GlobalThresholdParams gp;
    gp.method = static_cast<ip::GlobalThresholdParams::Method>(m_glMethod->currentIndex());
    gp.manual = static_cast<int>(m_glManual->value());
    gp.invert = m_glInvert->isChecked();

    ip::AdaptiveParams ap;
    ap.method = static_cast<ip::AdaptiveParams::Method>(m_adMethod->currentIndex());
    ap.window = static_cast<int>(m_adWindow->value());
    ap.C = m_adC->value();
    ap.k = m_adK->value();
    ap.R = m_adR->value();
    ap.invert = m_adInvert->isChecked();
    ap.useIntegral = !m_adNaive->isChecked();
    ap.edge = edge;

    ip::ClaheParams cp;
    cp.tilesX = static_cast<int>(m_clTilesX->value());
    cp.tilesY = static_cast<int>(m_clTilesY->value());
    cp.clipLimit = m_clClip->value();

    ip::setThreadCount(m_threadsSpin->value());
    m_jobSource = m_source;
    m_busy = true;
    m_pending = false;
    m_jobDone = false;
    m_applyBtn->setEnabled(false);
    m_progress.reset(1);
    m_progressBar->setValue(0);

    const std::shared_ptr<const ip::Image> src = m_source;
    m_worker = std::thread([this, src, op, sp, gp, ap, cp]() {
        const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
        JobResult r;
        switch (op) {
        case 0: r.image = ip::sharpen(*src, sp, &m_progress); break;
        case 1: r.image = ip::globalThreshold(*src, gp, &r.threshold, &m_progress); break;
        case 2: r.image = ip::adaptiveThreshold(*src, ap, &m_progress); break;
        default: r.image = ip::clahe(*src, cp, &m_progress); break;
        }
        r.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        r.hist = ip::computeHistograms(r.image);
        m_jobResult = std::move(r);
        m_jobDone.store(true);
    });
    m_pollTimer.start();
}

void MainWindow::onPoll() {
    m_progressBar->setValue(m_progress.percent());
    if (m_jobDone.load()) finishJob();
}

void MainWindow::finishJob() {
    if (m_worker.joinable()) m_worker.join();
    m_pollTimer.stop();
    m_busy = false;
    m_applyBtn->setEnabled(true);
    m_progressBar->setValue(100);

    if (m_jobSource != m_source) {
        m_pending = true;  // за время расчёта сменили изображение — результат устарел
    } else {
        m_result = std::move(m_jobResult.image);
        m_viewAfter->setImage(imageToQImage(m_result));
        m_viewAfter->setCaption(U8("Результат: ") + m_opCombo->currentText());
        m_histAfter->setData(m_jobResult.hist, U8("После"));
        m_histBefore->setThreshold(m_jobResult.threshold);
        m_useResultBtn->setEnabled(true);
        m_thresholdLabel->setText(m_jobResult.threshold >= 0 ? U8("Порог: T = %1").arg(m_jobResult.threshold)
                                                              : U8("Порог: —"));
        m_statusLabel->setText(U8("Готово: %1 мс, потоков: %2")
                               .arg(m_jobResult.ms, 0, 'f', 1).arg(ip::effectiveThreads()));
    }
    if (m_pending) { m_pending = false; applyNow(); }
}
