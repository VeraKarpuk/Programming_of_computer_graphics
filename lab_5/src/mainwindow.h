#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class Canvas;
class QCheckBox;
class QComboBox;
class QPlainTextEdit;
class QSpinBox;
class QGroupBox;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onModeChanged(int index);
    void openFile();
    void saveFile();
    void saveImage();
    void applyPreset();
    void randomSegments();
    void generateBig();
    void showTests();
    void showBenchmark();

private:
    void ensureDataForMode();
    void loadPolygons(int kind);

    Canvas *canvas_;
    QComboBox *modeBox_, *presetBox_;
    QSpinBox *randomCount_, *bigN_, *depthSpin_, *capSpin_, *dynSpin_;
    QCheckBox *treeCheck_, *animCheck_, *showAllCheck_;
    QGroupBox *bigBox_;
    QPlainTextEdit *statsText_;
    bool bigReady_ = false;
};

#endif
