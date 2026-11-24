#include "quadtree.h"

#include "segmentclip.h"

namespace geo {

namespace {

bool segmentTouchesCell(const Segment &s, const Rect &cell) {
    if (aabbOutside(s, cell)) return false;
    Segment tmp;
    return clipLiangBarsky(s, cell, tmp);
}

// общая часть: габарит, трivиальное принятие, Лианг-Барски
inline bool clipOne(const Segment &s, const Rect &w, Segment &out, QueryStats &st) {
    if (aabbOutside(s, w)) { ++st.aabbRejected; return false; }
    ++st.candidates;
    if ((outcode(s.a, w) | outcode(s.b, w)) == 0) {
        out = s;
        return true;
    }
    return clipLiangBarsky(s, w, out);
}

}  // namespace

int Quadtree::newNode(const Rect &cell) {
    Node n;
    n.cell = cell;
    nodes_.push_back(n);
    return (int)nodes_.size() - 1;
}

void Quadtree::build(const std::vector<Segment> *data, const Rect &bounds, int maxDepth,
                     int leafCapacity) {
    data_ = data;
    maxDepth_ = maxDepth;
    capacity_ = leafCapacity;
    reachedDepth_ = 0;
    nodes_.clear();
    nodes_.reserve(1024);
    newNode(bounds);
    stamp_.assign(data->size(), 0);
    curStamp_ = 0;
    for (size_t i = 0; i < data->size(); ++i) insert(0, (int)i, 0);
}

void Quadtree::insert(int node, int id, int depth) {
    const Segment &s = (*data_)[id];
    if (!segmentTouchesCell(s, nodes_[node].cell)) return;
    if (depth > reachedDepth_) reachedDepth_ = depth;

    if (nodes_[node].leaf()) {
        if ((int)nodes_[node].ids.size() < capacity_ || depth >= maxDepth_) {
            nodes_[node].ids.push_back(id);
            return;
        }
        // лист переполнен: делим ячейку и раздаём отрезки детям
        Rect c = nodes_[node].cell;
        double mx = (c.xmin + c.xmax) / 2, my = (c.ymin + c.ymax) / 2;
        int ch[4];
        ch[0] = newNode(Rect(c.xmin, c.ymin, mx, my));
        ch[1] = newNode(Rect(mx, c.ymin, c.xmax, my));
        ch[2] = newNode(Rect(c.xmin, my, mx, c.ymax));
        ch[3] = newNode(Rect(mx, my, c.xmax, c.ymax));
        std::vector<int> old;
        old.swap(nodes_[node].ids);
        for (int k = 0; k < 4; ++k) nodes_[node].child[k] = ch[k];
        for (int oid : old)
            for (int k = 0; k < 4; ++k) insert(ch[k], oid, depth + 1);
    }
    for (int k = 0; k < 4; ++k) insert(nodes_[node].child[k], id, depth + 1);
}

bool Quadtree::remove(int node, int id, const Segment &seg) {
    if (!segmentTouchesCell(seg, nodes_[node].cell)) return false;
    if (nodes_[node].leaf()) {
        std::vector<int> &v = nodes_[node].ids;
        for (size_t i = 0; i < v.size(); ++i)
            if (v[i] == id) {
                v[i] = v.back();
                v.pop_back();
                return true;
            }
        return false;
    }
    bool any = false;
    for (int k = 0; k < 4; ++k) any |= remove(nodes_[node].child[k], id, seg);
    return any;
}

void Quadtree::update(int id, const Segment &oldSeg, const Segment &newSeg) {
    remove(0, id, oldSeg);
    // data_ - константный указатель на тот же массив, новое значение уже записано снаружи
    (void)newSeg;
    insert(0, id, 0);
}

void Quadtree::clipWindow(const Rect &w, std::vector<Segment> &visible,
                          std::vector<int> *candidateIds, QueryStats &st) const {
    visible.clear();
    if (candidateIds) candidateIds->clear();
    st = QueryStats();
    if (++curStamp_ == 0) {  // переполнение счётчика
        std::fill(stamp_.begin(), stamp_.end(), 0);
        curStamp_ = 1;
    }

    int stack[256];
    int top = 0;
    stack[top++] = 0;
    while (top > 0) {
        const Node &n = nodes_[stack[--top]];
        ++st.nodesVisited;
        // ячейка целиком вне окна - отбрасываем весь кластер
        if (gt(n.cell.xmin, w.xmax) || lt(n.cell.xmax, w.xmin) ||
            gt(n.cell.ymin, w.ymax) || lt(n.cell.ymax, w.ymin))
            continue;
        if (!n.leaf()) {
            for (int k = 0; k < 4; ++k) stack[top++] = n.child[k];
            continue;
        }
        for (int id : n.ids) {
            if (stamp_[id] == curStamp_) continue;
            stamp_[id] = curStamp_;
            Segment out;
            if (clipOne((*data_)[id], w, out, st)) {
                visible.push_back(out);
                ++st.visible;
            }
            if (candidateIds && !aabbOutside((*data_)[id], w)) candidateIds->push_back(id);
        }
    }
}

void sortSpatially(std::vector<Segment> &data, const Rect &bounds) {
    auto spread = [](uint32_t v) {
        v = (v | (v << 8)) & 0x00FF00FF;
        v = (v | (v << 4)) & 0x0F0F0F0F;
        v = (v | (v << 2)) & 0x33333333;
        v = (v | (v << 1)) & 0x55555555;
        return v;
    };
    auto cell = [](double v, double lo, double hi) {
        double t = (v - lo) / (hi - lo);
        t = std::max(0.0, std::min(1.0, t));
        return (uint32_t)(t * 65535.0);
    };
    std::vector<std::pair<uint32_t, uint32_t>> keys(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        double mx = (data[i].a.x + data[i].b.x) / 2, my = (data[i].a.y + data[i].b.y) / 2;
        keys[i] = {spread(cell(mx, bounds.xmin, bounds.xmax)) | (spread(cell(my, bounds.ymin, bounds.ymax)) << 1),
                   (uint32_t)i};
    }
    std::sort(keys.begin(), keys.end());
    std::vector<Segment> sorted;
    sorted.reserve(data.size());
    for (const auto &k : keys) sorted.push_back(data[k.second]);
    data.swap(sorted);
}

void clipBruteForce(const std::vector<Segment> &data, const Rect &w,
                    std::vector<Segment> &visible, QueryStats &st) {
    visible.clear();
    st = QueryStats();
    st.nodesVisited = 1;
    Segment out;
    for (const Segment &s : data)
        if (clipOne(s, w, out, st)) {
            visible.push_back(out);
            ++st.visible;
        }
}

}  // namespace geo
