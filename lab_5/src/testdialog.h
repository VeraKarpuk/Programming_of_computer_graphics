#ifndef TESTDIALOG_H
#define TESTDIALOG_H

#include <QDialog>

class QTableWidget;
class QTabWidget;
class QLabel;

// таблица тест-кейсов с маркерами вырожденных случаев
class TestDialog : public QDialog {
    Q_OBJECT
public:
    explicit TestDialog(QWidget *parent = nullptr);

signals:
    void showSegmentCase(int index);
    void showPolygonCase(int index);

private:
    void fillSegments();
    void fillPolygons();
    void copyCurrent();

    QTabWidget *tabs_;
    QTableWidget *segTable_, *polyTable_;
    QLabel *summary_;
    int failed_ = 0, total_ = 0;
};

#endif
