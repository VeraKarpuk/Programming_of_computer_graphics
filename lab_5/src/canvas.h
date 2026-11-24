#ifndef CANVAS_H
#define CANVAS_H

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>
#include <random>

#include "geometry.h"
#include "polygonclip.h"
#include "quadtree.h"
#include "sceneio.h"
#include "segmentclip.h"

class Canvas : public QWidget {
    Q_OBJECT
public:
    enum Mode { LiangBarsky, SutherlandCohen, CyrusBeck, PolyWindow, PolyConvex, BigData };

    explicit Canvas(QWidget *parent = nullptr);

    Mode mode() const { return mode_; }
    void setMode(Mode m);

    const geo::SceneData &scene() const { return scene_; }
    void setScene(const geo::SceneData &s);
    void fitView();

    // режим "большие данные"
    void generateBigData(int n, int depth, int capacity);
    void rebuildTree(int depth, int capacity);
    int bigCount() const { return (int)big_.size(); }
    void setUseTree(bool on) { useTree_ = on; update(); }
    void setAnimate(bool on);
    void setDynamicPerFrame(int k);
    void setShowAll(bool on) { showAll_ = on; update(); }
    void setPrefilter(bool on) { prefilter_ = on; recompute(); }

signals:
    void statsChanged(const QString &text);
    void cursorMoved(const QString &text);
    void notice(const QString &text);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void showEvent(QShowEvent *e) override;

private:
    struct Hit {
        enum Kind { None, WinCorner, WinBody, ClipVertex, ClipBody, SegPoint, RingPoint } kind = None;
        int i = 0, j = 0;
    };
    enum Drag { NoDrag, Pan, MoveObject };

    bool usesWindow() const { return mode_ != CyrusBeck && mode_ != PolyConvex; }
    bool segmentMode() const { return mode_ == LiangBarsky || mode_ == SutherlandCohen || mode_ == CyrusBeck; }
    geo::Rect &win() { return mode_ == BigData ? bigWindow_ : scene_.window; }
    const geo::Rect &win() const { return mode_ == BigData ? bigWindow_ : scene_.window; }

    QPointF toScreen(const geo::Point &p) const;
    geo::Point toWorld(const QPointF &s) const;

    void recompute();
    QString segmentStatsText() const;
    Hit hitTest(const QPointF &pos) const;
    void applyDrag(const geo::Point &wp);
    void scaleClipper(double f);
    void scaleSubject(double f);

    void drawGrid(QPainter &p);
    void drawLegend(QPainter &p);
    void drawWindowOrClipper(QPainter &p);
    void drawHandles(QPainter &p);
    void drawSegmentsScene(QPainter &p);
    void drawPolygonsScene(QPainter &p);
    void drawBig(QPainter &p);
    void drawRings(QPainter &p, const std::vector<geo::Ring> &rings, const QPen &pen, const QBrush &brush);

    void tick();
    void refreshTimer();
    void animatePlace();

    Mode mode_ = LiangBarsky;
    geo::SceneData scene_;

    // результаты для режимов с малым числом объектов
    std::vector<geo::Segment> visible_;
    geo::OpStats stLB_, stSC_, stCB_;
    int mismatches_ = 0, rejected_ = 0;
    bool prefilter_ = false;
    bool convexOk_ = true;
    geo::ConvexClipper convex_;
    geo::WAResult wa_;

    // вид
    double scale_ = 40;
    QPointF origin_;
    bool needFit_ = true;

    // перетаскивание
    Drag drag_ = NoDrag;
    Hit hit_;
    QPointF lastPos_;
    geo::Point pressWorld_;
    geo::Rect savedWindow_;
    geo::Ring savedClipper_;
    geo::Point fixedCorner_;

    // большие данные
    std::vector<geo::Segment> big_;
    geo::Quadtree tree_;
    geo::Rect bigArea_ = geo::Rect(0, 0, 1000, 1000);
    geo::Rect bigWindow_ = geo::Rect(440, 460, 560, 540);
    std::vector<geo::Segment> bigVisible_;
    geo::QueryStats qs_;
    bool useTree_ = true, animate_ = false, showAll_ = false;
    int dynPerFrame_ = 0, treeDepth_ = 8, treeCap_ = 32;
    double queryMs_ = 0, updateMs_ = 0, buildMs_ = 0, drawMs_ = 0, fps_ = 0, animT_ = 0;
    geo::Point animBase_ = geo::Point(500, 500);
    QTimer timer_;
    QElapsedTimer clock_, paintClock_, statClock_;
    std::mt19937 rng_;
};

#endif
