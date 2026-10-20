// Presentation Layer: главное окно. Только отображение, параметры и запуск обработки.
#pragma once
#include <QMainWindow>
#include <QTimer>
#include <atomic>
#include <memory>
#include <thread>
#include "core/Algorithms.h"

class ImageView;
class HistogramWidget;
class SliderSpin;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QLabel;
class QPushButton;
class QProgressBar;
class QStackedWidget;
class QListWidget;
class QListWidgetItem;

struct JobResult {
    ip::Image image;
    ip::Histograms hist;
    int threshold;
    double ms;
    JobResult() : threshold(-1), ms(0) {}
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

private slots:
    void openImage();
    void saveResult();
    void saveHistograms();
    void useResultAsSource();
    void loadTestImage(QListWidgetItem* item);
    void saveTestBase();
    void showAbout();
    void scheduleApply();   // перезапуск с задержкой при изменении параметров (live-режим)
    void applyNow();
    void onPoll();          // опрос фонового потока: прогресс / завершение
    void updateControlStates();

private:
    void buildMenus();
    void buildUi();
    QWidget* buildControlPanel();
    QWidget* buildSharpenPage();
    QWidget* buildGlobalPage();
    QWidget* buildAdaptivePage();
    QWidget* buildClahePage();
    void setSource(const QImage& img, const QString& name);
    void finishJob();
    ip::EdgeMode edgeMode() const;

    // представление
    ImageView *m_viewBefore, *m_viewAfter;
    HistogramWidget *m_histBefore, *m_histAfter;
    QComboBox *m_opCombo, *m_edgeCombo;
    QStackedWidget* m_stack;
    QSpinBox* m_threadsSpin;
    QCheckBox* m_liveCheck;
    QPushButton *m_applyBtn, *m_useResultBtn;
    QProgressBar* m_progressBar;
    QLabel *m_statusLabel, *m_thresholdLabel;
    QListWidget* m_testList;

    // страница «Резкость»
    QComboBox *m_shMethod, *m_shBlurType;
    SliderSpin *m_shStrength, *m_shBoost, *m_shSigma, *m_shBoxR, *m_shThresh;
    QCheckBox* m_shDetail;
    // страница «Глобальный порог»
    QComboBox* m_glMethod;
    SliderSpin* m_glManual;
    QCheckBox* m_glInvert;
    // страница «Адаптивный порог»
    QComboBox* m_adMethod;
    SliderSpin *m_adWindow, *m_adC, *m_adK, *m_adR;
    QCheckBox *m_adInvert, *m_adNaive;
    // страница CLAHE
    SliderSpin *m_clTilesX, *m_clTilesY, *m_clClip;

    // данные
    std::shared_ptr<const ip::Image> m_source, m_jobSource;
    ip::Image m_result;
    ip::Histograms m_srcHist;

    // фоновая обработка
    std::thread m_worker;
    std::atomic<bool> m_busy, m_jobDone;
    JobResult m_jobResult;
    ip::Progress m_progress;
    QTimer m_pollTimer, m_debounce;
    bool m_pending;
};
