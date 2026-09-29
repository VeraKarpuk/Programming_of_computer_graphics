#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QColor>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include <QVector3D>
#include "colorcontroller.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateFromRgbSliders();
    void updateFromXyzInputs();
    void updateFromHsvSliders();
    void updateFromRgbInputs();
    void updateFromHsvInputs();
    void updateFromColorPicker();
    void onIlluminantChanged(int index);
    void onStrategyChanged(int index);
    void updateUiFromController();

private:
    void setupUi();
    void setupRgbSection();
    void setupXyzSection();
    void setupHsvSection();
    void updateRgbGradients();
    void updateColorPreview();

    ColorController* m_controller;

    QGroupBox* m_rgbSection;
    QGroupBox* m_xyzSection;
    QGroupBox* m_hsvSection;

    QSlider* m_rgbSliders[3];
    QLineEdit* m_rgbInputs[3];
    QLabel* m_rgbLabels[3];

    QLineEdit* m_xyzInputs[3];
    QLabel* m_xyzLabels[3];

    QSlider* m_hsvSliders[3];
    QLineEdit* m_hsvInputs[3];
    QLabel* m_hsvLabels[3];

    QLabel* m_colorPreview;
    QPushButton* m_colorPickerBtn;
    QComboBox* m_illuminantCombo;
    QComboBox* m_strategyCombo;
    QLabel* m_warningLabel;

    bool m_updating;
};

#endif