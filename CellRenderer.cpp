#include "CellRenderer.hpp"

namespace render {
namespace {

// Rotate a point around the cell centre by dir quarter turns (screen y points down).
sf::Vector2f rot(sf::Vector2f p, int dir) {
    for (int i = 0; i < (dir & 3); ++i) p = {-p.y, p.x};
    return p;
}

struct Pen { // maps local coords in [-0.5, 0.5] to screen/world coords
    sf::Vector2f centre;
    float size;
    int dir;
    sf::Vector2f at(float u, float v) const { return centre + rot({u, v}, dir) * size; }
};

void rotRect(sf::VertexArray& va, const Pen& p, float u0, float v0, float u1, float v1,
             sf::Color c) {
    sf::Vector2f a = p.at(u0, v0), b = p.at(u1, v0), d = p.at(u0, v1), e = p.at(u1, v1);
    for (auto q : {a, b, e, a, e, d}) va.append(sf::Vertex(q, c));
}

void rotTri(sf::VertexArray& va, const Pen& p, sf::Vector2f p0, sf::Vector2f p1,
            sf::Vector2f p2, sf::Color c) {
    for (auto q : {p0, p1, p2}) va.append(sf::Vertex(p.at(q.x, q.y), c));
}

} // namespace

sf::Color baseColor(CellType t) {
    switch (t) {
        case CellType::Wire:   return {255, 200, 60};
        case CellType::Switch: return {235, 70, 70};
        case CellType::Lamp:   return {255, 250, 170};
        case CellType::And:    return {80, 140, 255};
        case CellType::Or:     return {70, 200, 110};
        case CellType::Not:    return {240, 140, 50};
        case CellType::Xor:    return {180, 100, 240};
        default:               return sf::Color::White;
    }
}

sf::Color shade(sf::Color c, bool on) {
    if (on) return c;
    return sf::Color(c.r / 3 + 20, c.g / 3 + 20, c.b / 3 + 20, c.a);
}

void addRect(sf::VertexArray& va, float x0, float y0, float x1, float y1, sf::Color c) {
    sf::Vector2f a{x0, y0}, b{x1, y0}, d{x0, y1}, e{x1, y1};
    for (auto q : {a, b, e, a, e, d}) va.append(sf::Vertex(q, c));
}

void addCellGlyph(sf::VertexArray& va, const Cell& cell, sf::Vector2f origin, float size,
                  bool wireHigh, uint8_t neighborMask) {
    const sf::Vector2f centre = origin + sf::Vector2f(size, size) * 0.5f;
    const sf::Color dark(15, 15, 20);
    Pen p{centre, size, 0};

    switch (cell.type) {
    case CellType::Wire: {
        sf::Color col = shade(baseColor(CellType::Wire), wireHigh);
        rotRect(va, p, -0.14f, -0.14f, 0.14f, 0.14f, col);
        for (int d = 0; d < 4; ++d)
            if (neighborMask & (1u << d))
                rotRect(va, Pen{centre, size, d}, 0.14f, -0.14f, 0.5f, 0.14f, col);
        break;
    }
    case CellType::Switch: {
        rotRect(va, p, -0.38f, -0.38f, 0.38f, 0.38f, sf::Color(60, 60, 70));
        float off = cell.state ? 0.f : 0.3f;
        rotRect(va, p, -0.28f, -0.28f + off, 0.28f, -0.02f + off,
                shade(baseColor(CellType::Switch), cell.state));
        break;
    }
    case CellType::Lamp:
        if (cell.state) rotRect(va, p, -0.45f, -0.45f, 0.45f, 0.45f, sf::Color(255, 240, 120, 70));
        rotRect(va, p, -0.3f, -0.3f, 0.3f, 0.3f, shade(baseColor(CellType::Lamp), cell.state));
        break;
    default: {
        if (!isGate(cell.type)) break;
        Pen g{centre, size, cell.dir};
        rotRect(va, g, -0.38f, -0.38f, 0.34f, 0.38f, shade(baseColor(cell.type), cell.state));
        rotTri(va, g, {0.34f, -0.2f}, {0.34f, 0.2f}, {0.5f, 0.f}, sf::Color(240, 240, 240));
        if (cell.type == CellType::Not) {
            rotRect(va, g, -0.5f, -0.08f, -0.38f, 0.08f, dark);
            rotRect(va, g, 0.1f, -0.1f, 0.3f, 0.1f, dark);
        } else {
            rotRect(va, g, -0.08f, -0.5f, 0.08f, -0.38f, dark);
            rotRect(va, g, -0.08f, 0.38f, 0.08f, 0.5f, dark);
            if (cell.type != CellType::Or)  rotRect(va, g, -0.25f, -0.06f, 0.2f, 0.06f, dark);
            if (cell.type != CellType::And) rotRect(va, g, -0.06f, -0.25f, 0.06f, 0.25f, dark);
        }
        break;
    }
    }
}

void buildCells(sf::VertexArray& va, const World& w, const Simulation& sim,
                int x0, int y0, int x1, int y1) {
    w.forEachInRect(x0, y0, x1, y1, [&](int x, int y, const Cell& c) {
        uint8_t mask = 0;
        if (c.type == CellType::Wire)
            for (int d = 0; d < 4; ++d)
                if (w.get(x + DX[d], y + DY[d]).type != CellType::Empty) mask |= 1u << d;
        addCellGlyph(va, c, {float(x), float(y)}, 1.f, sim.wireHigh(x, y), mask);
    });
}

void buildGrid(sf::VertexArray& lines, int x0, int y0, int x1, int y1) {
    sf::Color minor(32, 36, 46), major(45, 50, 64);
    for (int x = x0; x <= x1; ++x) {
        sf::Color col = (x % 8 == 0) ? major : minor;
        lines.append(sf::Vertex({float(x), float(y0)}, col));
        lines.append(sf::Vertex({float(x), float(y1)}, col));
    }
    for (int y = y0; y <= y1; ++y) {
        sf::Color col = (y % 8 == 0) ? major : minor;
        lines.append(sf::Vertex({float(x0), float(y)}, col));
        lines.append(sf::Vertex({float(x1), float(y)}, col));
    }
}

} // namespace render
