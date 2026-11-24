#ifndef QUADTREE_H
#define QUADTREE_H

#include <cstdint>

#include "geometry.h"

namespace geo {

struct QueryStats {
    int nodesVisited = 0;
    int candidates = 0;   // прошли габаритную отбраковку
    int visible = 0;      // дали непустую видимую часть
    int aabbRejected = 0;
};

// Четверичное дерево отрезков. Отрезок хранится во всех листьях,
// чьи ячейки он пересекает; повторы при запросе убираются метками.
class Quadtree {
public:
    Quadtree() {}

    void build(const std::vector<Segment> *data, const Rect &bounds, int maxDepth, int leafCapacity);

    // обновление одного отрезка: старые координаты нужны, чтобы найти листья
    void update(int id, const Segment &oldSeg, const Segment &newSeg);

    // отсечение окном всех отрезков: результат - видимые части
    void clipWindow(const Rect &w, std::vector<Segment> &visible,
                    std::vector<int> *candidateIds, QueryStats &st) const;

    int nodeCount() const { return (int)nodes_.size(); }
    int maxDepthReached() const { return reachedDepth_; }

private:
    struct Node {
        Rect cell;
        int child[4] = {-1, -1, -1, -1};
        std::vector<int> ids;
        bool leaf() const { return child[0] < 0; }
    };

    void insert(int node, int id, int depth);
    bool remove(int node, int id, const Segment &seg);
    int newNode(const Rect &cell);

    const std::vector<Segment> *data_ = nullptr;
    std::vector<Node> nodes_;
    int maxDepth_ = 8, capacity_ = 32, reachedDepth_ = 0;
    mutable std::vector<uint32_t> stamp_;
    mutable uint32_t curStamp_ = 0;
};

// Раскладывает отрезки в памяти по Z-кривой (по середине отрезка): отрезки,
// близкие на плоскости, оказываются рядом в памяти, запросы меньше промахиваются мимо кэша.
// Вызывать до построения дерева.
void sortSpatially(std::vector<Segment> &data, const Rect &bounds);

// то же самое полным перебором (для сравнения)
void clipBruteForce(const std::vector<Segment> &data, const Rect &w,
                    std::vector<Segment> &visible, QueryStats &st);

}  // namespace geo

#endif
