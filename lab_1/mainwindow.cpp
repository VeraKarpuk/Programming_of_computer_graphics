#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QDoubleValidator>
#include <QIntValidator>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_rgbSection(nullptr)
    , m_xyzSection(nullptr)
    , m_hsvSection(nullptr)
    , m_colorPreview(nullptr)
    , m_colorPickerBtn(nullptr)
    , m_illuminantCombo(nullptr)
    , m_strategyCombo(nullptr)
    , m_warningLabel(nullptr)
    , m_updating(false)
{
    for (int i = 0; i < 3; i++) {
        m_rgbSliders[i] = nullptr;
        m_rgbInputs[i] = nullptr;
        m_rgbLabels[i] = nullptr;
        m_xyzInputs[i] = nullptr;
        m_xyzLabels[i] = nullptr;
        m_hsvSliders[i] = nullptr;
        m_hsvInputs[i] = nullptr;
        m_hsvLabels[i] = nullptr;
    }

    m_controller = new ColorController(this);

    connect(m_controller, &ColorController::rgbChanged, this, &MainWindow::updateUiFromController);
    connect(m_controller, &ColorController::xyzChanged, this, &MainWindow::updateUiFromController);
    connect(m_controller, &ColorController::hsvChanged, this, &MainWindow::updateUiFromController);
    connect(m_controller, &ColorController::warningMessageChanged, this, &MainWindow::updateUiFromController);

    setupUi();
    updateUiFromController();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    QLabel* titleLabel = new QLabel("Color Converter - RGB ↔ XYZ ↔ HSV", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; margin: 10px;");
    mainLayout->addWidget(titleLabel);

    QHBoxLayout* previewLayout = new QHBoxLayout();
    m_colorPreview = new QLabel(this);
    m_colorPreview->setFixedSize(300, 60);
    m_colorPreview->setStyleSheet("border: 2px solid #333; border-radius: 5px;");
    previewLayout->addWidget(m_colorPreview);

    m_colorPickerBtn = new QPushButton("Выбрать цвет из палитры", this);
    m_colorPickerBtn->setFixedHeight(60);
    connect(m_colorPickerBtn, &QPushButton::clicked, this, &MainWindow::updateFromColorPicker);
    previewLayout->addWidget(m_colorPickerBtn);
    mainLayout->addLayout(previewLayout);

    QHBoxLayout* controlsLayout = new QHBoxLayout();

    QGroupBox* illuminantGroup = new QGroupBox("Стандарт освещения", this);
    QVBoxLayout* illuminantLayout = new QVBoxLayout(illuminantGroup);
    m_illuminantCombo = new QComboBox(this);
    m_illuminantCombo->addItem("D65 (sRGB)");
    m_illuminantCombo->addItem("D50 (Полиграфия)");
    m_illuminantCombo->addItem("E (Равноэнергетический)");
    connect(m_illuminantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onIlluminantChanged);
    illuminantLayout->addWidget(m_illuminantCombo);
    controlsLayout->addWidget(illuminantGroup);

    QGroupBox* strategyGroup = new QGroupBox("Стратегия выхода за границы", this);
    QVBoxLayout* strategyLayout = new QVBoxLayout(strategyGroup);
    m_strategyCombo = new QComboBox(this);
    m_strategyCombo->addItem("Clipping (Обрезание)");
    m_strategyCombo->addItem("Scaling (Масштабирование)");
    connect(m_strategyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onStrategyChanged);
    strategyLayout->addWidget(m_strategyCombo);
    controlsLayout->addWidget(strategyGroup);

    mainLayout->addLayout(controlsLayout);

    m_warningLabel = new QLabel(this);
    m_warningLabel->setStyleSheet("color: orange; font-weight: bold;");
    mainLayout->addWidget(m_warningLabel);

    QHBoxLayout* sectionsLayout = new QHBoxLayout();

    setupRgbSection();
    setupXyzSection();
    setupHsvSection();

    sectionsLayout->addWidget(m_rgbSection);
    sectionsLayout->addWidget(m_xyzSection);
    sectionsLayout->addWidget(m_hsvSection);

    mainLayout->addLayout(sectionsLayout);

    setCentralWidget(centralWidget);
    setWindowTitle("Color Converter");
    resize(1000, 500);
}

void MainWindow::setupRgbSection() {
    m_rgbSection = new QGroupBox("RGB", this);
    QVBoxLayout* layout = new QVBoxLayout(m_rgbSection);

    QStringList names = {"R", "G", "B"};
    for (int i = 0; i < 3; i++) {
        QHBoxLayout* row = new QHBoxLayout();

        m_rgbLabels[i] = new QLabel(names[i], this);
        m_rgbLabels[i]->setFixedWidth(20);
        row->addWidget(m_rgbLabels[i]);

        m_rgbSliders[i] = new QSlider(Qt::Horizontal, this);
        m_rgbSliders[i]->setRange(0, 255);
        connect(m_rgbSliders[i], &QSlider::valueChanged, this, &MainWindow::updateFromRgbSliders);
        row->addWidget(m_rgbSliders[i]);

        m_rgbInputs[i] = new QLineEdit(this);
        m_rgbInputs[i]->setFixedWidth(60);
        m_rgbInputs[i]->setValidator(new QIntValidator(0, 255, this));
        connect(m_rgbInputs[i], &QLineEdit::editingFinished, this, &MainWindow::updateFromRgbInputs);
        row->addWidget(m_rgbInputs[i]);

        layout->addLayout(row);
    }
}

void MainWindow::setupXyzSection() {
    m_xyzSection = new QGroupBox("XYZ", this);
    QVBoxLayout* layout = new QVBoxLayout(m_xyzSection);

    QStringList names = {"X", "Y", "Z"};
    for (int i = 0; i < 3; i++) {
        QHBoxLayout* row = new QHBoxLayout();

        m_xyzLabels[i] = new QLabel(names[i], this);
        m_xyzLabels[i]->setFixedWidth(20);
        row->addWidget(m_xyzLabels[i]);

        m_xyzInputs[i] = new QLineEdit(this);
        m_xyzInputs[i]->setValidator(new QDoubleValidator(0, 100, 3, this));
        connect(m_xyzInputs[i], &QLineEdit::editingFinished, this, &MainWindow::updateFromXyzInputs);
        row->addWidget(m_xyzInputs[i]);

        layout->addLayout(row);
    }
}

void MainWindow::setupHsvSection() {
    m_hsvSection = new QGroupBox("HSV", this);
    QVBoxLayout* layout = new QVBoxLayout(m_hsvSection);

    QStringList names = {"H", "S", "V"};
    for (int i = 0; i < 3; i++) {
        QHBoxLayout* row = new QHBoxLayout();

        m_hsvLabels[i] = new QLabel(names[i], this);
        m_hsvLabels[i]->setFixedWidth(20);
        row->addWidget(m_hsvLabels[i]);

        m_hsvSliders[i] = new QSlider(Qt::Horizontal, this);
        m_hsvSliders[i]->setRange(0, i == 0 ? 360 : 100);
        connect(m_hsvSliders[i], &QSlider::valueChanged, this, &MainWindow::updateFromHsvSliders);
        row->addWidget(m_hsvSliders[i]);

        m_hsvInputs[i] = new QLineEdit(this);
        m_hsvInputs[i]->setFixedWidth(60);
        m_hsvInputs[i]->setValidator(new QDoubleValidator(0, i == 0 ? 360 : 100, 1, this));
        connect(m_hsvInputs[i], &QLineEdit::editingFinished, this, &MainWindow::updateFromHsvInputs);
        row->addWidget(m_hsvInputs[i]);

        layout->addLayout(row);
    }
}

void MainWindow::updateFromRgbSliders() {
    if (m_updating) return;

    double r = m_rgbSliders[0]->value();
    double g = m_rgbSliders[1]->value();
    double b = m_rgbSliders[2]->value();

    m_controller->setRgb(QVector3D(r, g, b));
}

void MainWindow::updateFromRgbInputs() {
    if (m_updating) return;

    double r = m_rgbInputs[0]->text().toDouble();
    double g = m_rgbInputs[1]->text().toDouble();
    double b = m_rgbInputs[2]->text().toDouble();

    m_controller->setRgb(QVector3D(r, g, b));
}

void MainWindow::updateFromXyzInputs() {
    if (m_updating) return;

    double x = m_xyzInputs[0]->text().toDouble();
    double y = m_xyzInputs[1]->text().toDouble();
    double z = m_xyzInputs[2]->text().toDouble();

    m_controller->setXyz(QVector3D(x, y, z));
}

void MainWindow::updateFromHsvSliders() {
    if (m_updating) return;

    double h = m_hsvSliders[0]->value();
    double s = m_hsvSliders[1]->value();
    double v = m_hsvSliders[2]->value();

    m_controller->setHsv(QVector3D(h, s, v));
}

void MainWindow::updateFromHsvInputs() {
    if (m_updating) return;

    double h = m_hsvInputs[0]->text().toDouble();
    double s = m_hsvInputs[1]->text().toDouble();
    double v = m_hsvInputs[2]->text().toDouble();

    m_controller->setHsv(QVector3D(h, s, v));
}

void MainWindow::updateFromColorPicker() {
    QColor color = QColorDialog::getColor(
        QColor(m_controller->getRgb().x(), m_controller->getRgb().y(), m_controller->getRgb().z()),
        this,
        "Выберите цвет"
        );

    if (color.isValid()) {
        m_controller->setRgb(QVector3D(color.red(), color.green(), color.blue()));
    }
}

void MainWindow::onIlluminantChanged(int index) {
    Illuminant illum = static_cast<Illuminant>(index);
    m_controller->setIlluminant(illum);
}

void MainWindow::onStrategyChanged(int index) {
    ColorController::OutOfGamutStrategy strategy =
        static_cast<ColorController::OutOfGamutStrategy>(index);
    m_controller->setStrategy(strategy);
}

void MainWindow::updateUiFromController() {
    m_updating = true;

    QVector3D rgb = m_controller->getRgb();
    QVector3D xyz = m_controller->getXyz();
    QVector3D hsv = m_controller->getHsv();

    for (int i = 0; i < 3; i++) {
        int val = qRound(rgb[i]);
        m_rgbSliders[i]->setValue(val);
        m_rgbInputs[i]->setText(QString::number(val));
    }

    for (int i = 0; i < 3; i++) {
        m_xyzInputs[i]->setText(QString::number(xyz[i], 'f', 2));
    }

    for (int i = 0; i < 3; i++) {
        int val = qRound(hsv[i]);
        m_hsvSliders[i]->setValue(val);
        m_hsvInputs[i]->setText(QString::number(val));
    }

    updateColorPreview();
    updateRgbGradients();

    m_warningLabel->setText(m_controller->getWarningMessage());

    m_updating = false;
}

void MainWindow::updateRgbGradients() {
    QVector3D rgb = m_controller->getRgb();

    for (int i = 0; i < 3; i++) {
        QString style = QString(
                            "QSlider::groove:horizontal { "
                            "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                            "stop:0 rgb(%1,%2,%3), "
                            "stop:1 rgb(%4,%5,%6)); "
                            "height: 20px; border-radius: 10px; }"
                            "QSlider::handle:horizontal { "
                            "width: 20px; height: 20px; "
                            "background: white; "
                            "border: 2px solid #333; "
                            "border-radius: 10px; "
                            "margin: -5px 0; }"
                            )
                            .arg(i == 0 ? 0 : (int)rgb.x())
                            .arg(i == 1 ? 0 : (int)rgb.y())
                            .arg(i == 2 ? 0 : (int)rgb.z())
                            .arg(i == 0 ? 255 : (int)rgb.x())
                            .arg(i == 1 ? 255 : (int)rgb.y())
                            .arg(i == 2 ? 255 : (int)rgb.z());

        m_rgbSliders[i]->setStyleSheet(style);
    }
}

void MainWindow::updateColorPreview() {
    QVector3D rgb = m_controller->getRgb();
    QColor color(qRound(rgb.x()), qRound(rgb.y()), qRound(rgb.z()));

    m_colorPreview->setStyleSheet(
        QString("border: 2px solid #333; border-radius: 5px; background-color: %1; color: %2;")
            .arg(color.name())
            .arg(color.lightness() > 128 ? "#000" : "#fff")
        );

    m_colorPreview->setText(color.name());
    m_colorPreview->setAlignment(Qt::AlignCenter);
}