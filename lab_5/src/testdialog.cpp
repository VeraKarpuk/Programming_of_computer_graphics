#include "testdialog.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "polygonclip.h"
#include "segmentclip.h"
#include "testcases.h"

namespace {

QString num(double v) { return QString::number(v, 'g', 6); }

QString segText(const geo::Segment &s) {
    return QString("(%1; %2) - (%3; %4)").arg(num(s.a.x)).arg(num(s.a.y)).arg(num(s.b.x)).arg(num(s.b.y));
}

QTableWidgetItem *cell(const QString &text) {
    QTableWidgetItem *it = new QTableWidgetItem(text);
    it->setFlags(it->flags() & ~Qt::ItemIsEditable);
    return it;
}

void colorStatus(QTableWidgetItem *it, bool ok) {
    it->setForeground(ok ? QColor(20, 130, 60) : QColor(200, 30, 30));
}

}  // namespace

TestDialog::TestDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QString::fromUtf8("Тест-кейсы"));
    resize(1100, 560);

    QVBoxLayout *lay = new QVBoxLayout(this);
    summary_ = new QLabel(this);
    lay->addWidget(summary_);

    tabs_ = new QTabWidget(this);
    segTable_ = new QTableWidget(this);
    polyTable_ = new QTableWidget(this);
    tabs_->addTab(segTable_, QString::fromUtf8("Отрезки, окно (0,0)-(10,10)"));
    tabs_->addTab(polyTable_, QString::fromUtf8("Многоугольники"));
    lay->addWidget(tabs_);

    QHBoxLayout *row = new QHBoxLayout();
    QLabel *hint = new QLabel(QString::fromUtf8("Двойной щелчок по строке показывает случай на холсте"), this);
    QPushButton *copy = new QPushButton(QString::fromUtf8("Копировать таблицу"), this);
    QPushButton *close = new QPushButton(QString::fromUtf8("Закрыть"), this);
    row->addWidget(hint, 1);
    row->addWidget(copy);
    row->addWidget(close);
    lay->addLayout(row);
    connect(copy, &QPushButton::clicked, this, &TestDialog::copyCurrent);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    fillSegments();
    fillPolygons();
    summary_->setText(QString::fromUtf8("Пройдено %1 из %2").arg(total_ - failed_).arg(total_));

    connect(segTable_, &QTableWidget::cellDoubleClicked, this, [this](int r, int) { emit showSegmentCase(r); });
    connect(polyTable_, &QTableWidget::cellDoubleClicked, this, [this](int r, int) { emit showPolygonCase(r); });
}

void TestDialog::fillSegments() {
    geo::Rect w = geo::testWindow();
    geo::ConvexClipper cb = geo::ConvexClipper::fromRect(w);
    std::vector<geo::SegTest> tests = geo::segmentTests();

    QStringList heads;
    heads << "№" << QString::fromUtf8("Случай") << QString::fromUtf8("Маркер") << QString::fromUtf8("Отрезок")
          << QString::fromUtf8("Ожидается") << QString::fromUtf8("Лианг-Барски")
          << QString::fromUtf8("Сазерленд-Коэн") << QString::fromUtf8("Сайрус-Бек") << QString::fromUtf8("Итог");
    segTable_->setColumnCount(heads.size());
    segTable_->setHorizontalHeaderLabels(heads);
    segTable_->setRowCount((int)tests.size());

    for (size_t i = 0; i < tests.size(); ++i) {
        const geo::SegTest &t = tests[i];
        geo::Segment r[3];
        bool vis[3];
        vis[0] = geo::clipLiangBarsky(t.seg, w, r[0]);
        vis[1] = geo::clipSutherlandCohen(t.seg, w, r[1]);
        vis[2] = cb.clip(t.seg, r[2]);

        bool ok = true;
        QString shown[3];
        for (int k = 0; k < 3; ++k) {
            shown[k] = vis[k] ? segText(r[k]) : QString::fromUtf8("невидим");
            if (vis[k] != t.visible || (t.visible && !geo::sameSegment(r[k], t.expected))) ok = false;
        }
        int row = (int)i;
        segTable_->setItem(row, 0, cell(QString::number(i + 1)));
        segTable_->setItem(row, 1, cell(QString::fromUtf8(t.name.c_str())));
        segTable_->setItem(row, 2, cell(QString::fromUtf8(t.marker.c_str())));
        segTable_->setItem(row, 3, cell(segText(t.seg)));
        segTable_->setItem(row, 4, cell(t.visible ? segText(t.expected) : QString::fromUtf8("невидим")));
        for (int k = 0; k < 3; ++k) segTable_->setItem(row, 5 + k, cell(shown[k]));
        QTableWidgetItem *st = cell(ok ? "OK" : QString::fromUtf8("ОШИБКА"));
        colorStatus(st, ok);
        segTable_->setItem(row, 8, st);
        ++total_;
        if (!ok) ++failed_;
    }
    segTable_->resizeColumnsToContents();
    segTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
}

void TestDialog::fillPolygons() {
    std::vector<geo::PolyTest> tests = geo::polygonTests();
    QStringList heads;
    heads << "№" << QString::fromUtf8("Случай") << QString::fromUtf8("Маркер")
          << QString::fromUtf8("Контуров ожидается") << QString::fromUtf8("Контуров получено")
          << QString::fromUtf8("Расхождений в пробных точках") << QString::fromUtf8("Итог");
    polyTable_->setColumnCount(heads.size());
    polyTable_->setHorizontalHeaderLabels(heads);
    polyTable_->setRowCount((int)tests.size());

    for (size_t i = 0; i < tests.size(); ++i) {
        const geo::PolyTest &t = tests[i];
        geo::WAResult res = geo::weilerAtherton(t.subject, t.clipper);
        int bad = geo::polygonMismatches(t.subject, t.clipper, res.contours);
        bool ok = bad == 0 && (int)res.contours.size() == t.contours;
        int row = (int)i;
        polyTable_->setItem(row, 0, cell(QString::number(i + 1)));
        polyTable_->setItem(row, 1, cell(QString::fromUtf8(t.name.c_str())));
        polyTable_->setItem(row, 2, cell(QString::fromUtf8(t.marker.c_str())));
        polyTable_->setItem(row, 3, cell(QString::number(t.contours)));
        polyTable_->setItem(row, 4, cell(QString::number(res.contours.size())));
        polyTable_->setItem(row, 5, cell(QString::number(bad)));
        QTableWidgetItem *st = cell(ok ? "OK" : QString::fromUtf8("ОШИБКА"));
        colorStatus(st, ok);
        polyTable_->setItem(row, 6, st);
        ++total_;
        if (!ok) ++failed_;
    }
    polyTable_->resizeColumnsToContents();
    polyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
}

void TestDialog::copyCurrent() {
    QTableWidget *t = tabs_->currentIndex() == 0 ? segTable_ : polyTable_;
    QString out;
    for (int c = 0; c < t->columnCount(); ++c)
        out += t->horizontalHeaderItem(c)->text() + (c + 1 < t->columnCount() ? "\t" : "\n");
    for (int r = 0; r < t->rowCount(); ++r)
        for (int c = 0; c < t->columnCount(); ++c)
            out += t->item(r, c)->text() + (c + 1 < t->columnCount() ? "\t" : "\n");
    QApplication::clipboard()->setText(out);
}
