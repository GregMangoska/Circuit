#pragma once
#include "Simulation.hpp"
#include "World.hpp"
#include <SFML/Graphics.hpp>

// Batched drawing of grid cells into a single sf::Triangles vertex array.
// Everything is drawn in "cell space": a cell at (x, y) covers [x, x+1] x [y, y+1].
namespace render {

sf::Color baseColor(CellType t);
sf::Color shade(sf::Color c, bool on);

void addRect(sf::VertexArray& va, float x0, float y0, float x1, float y1, sf::Color c);

// Draws one cell glyph whose top-left is `origin` with edge length `size`.
// neighborMask bit d set => something is connected in direction d (wires draw arms).
void addCellGlyph(sf::VertexArray& va, const Cell& cell, sf::Vector2f origin, float size,
                  bool wireHigh, uint8_t neighborMask);

// All non-empty cells inside the inclusive rect.
void buildCells(sf::VertexArray& va, const World& w, const Simulation& sim,
                int x0, int y0, int x1, int y1);

// Grid lines for the inclusive rect (sf::Lines).
void buildGrid(sf::VertexArray& lines, int x0, int y0, int x1, int y1);

} // namespace render
