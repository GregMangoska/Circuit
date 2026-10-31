#include "MobileGame.hpp"
#include "CellRenderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace {

constexpr float kBarWidth = 170.f;    // side bar width at 1080p
constexpr float kMoveThreshold = 22.f; // px at 1080p before a touch counts as a drag
constexpr float kHoldToPaint = 0.12f;  // s; still finger starts painting after this

float length(sf::Vector2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }

void tri(sf::VertexArray& va, sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Color col) {
    va.append(sf::Vertex(a, col));
    va.append(sf::Vertex(b, col));
    va.append(sf::Vertex(c, col));
}

// Arrow head centred at c pointing in dir (0=E,1=S,2=W,3=N).
void arrow(sf::VertexArray& va, sf::Vector2f c, float r, int dir, sf::Color col) {
    sf::Vector2f f{float(DX[dir]), float(DY[dir])}, s{-f.y, f.x};
    tri(va, c + f * r, c - f * r * 0.6f + s * r, c - f * r * 0.6f - s * r, col);
}

} // namespace

MobileGame::MobileGame(std::string savePath)
    : window_(sf::VideoMode::getDesktopMode(), "Circuit Grid", sf::Style::Fullscreen),
      savePath_(std::move(savePath)) {
    window_.setVerticalSyncEnabled(true);
    world_.load(savePath_); // resume last session if there is one
    worldView_.setCenter(0.f, 0.f);
    layout();
    zoom_ = 64.f * ui_;
    worldView_.setSize(sf::Vector2f(window_.getSize()) / zoom_);
}

void MobileGame::layout() {
    sf::Vector2f size(window_.getSize());
    ui_ = size.y / 1080.f;
    sf::Vector2f centre = worldView_.getCenter();
    worldView_.setSize(size / zoom_);
    worldView_.setCenter(centre);
    uiView_.reset({0.f, 0.f, size.x, size.y});

    buttons_.clear();
    const float w = kBarWidth * ui_, pad = 8.f * ui_;

    const std::pair<Mode, CellType> tools[] = {
        {Mode::Place, CellType::Wire}, {Mode::Place, CellType::Switch},
        {Mode::Place, CellType::Lamp}, {Mode::Place, CellType::And},
        {Mode::Place, CellType::Or},   {Mode::Place, CellType::Not},
        {Mode::Place, CellType::Xor},  {Mode::Erase, CellType::Empty},
        {Mode::Pan, CellType::Empty}};
    float h = size.y / std::size(tools);
    for (size_t i = 0; i < std::size(tools); ++i)
        buttons_.push_back({{pad, i * h + pad, w - 2 * pad, h - 2 * pad},
                            Action::Tool, tools[i].first, tools[i].second});

    const Action right[] = {Action::Rotate, Action::PlayPause, Action::Step, Action::Faster,
                            Action::Slower, Action::Save,      Action::Load, Action::Home};
    h = size.y / std::size(right);
    for (size_t i = 0; i < std::size(right); ++i)
        buttons_.push_back({{size.x - w + pad, i * h + pad, w - 2 * pad, h - 2 * pad}, right[i]});
}

void MobileGame::run() {
    sf::Clock clock;
    while (window_.isOpen()) {
        sf::Event e;
        while (window_.pollEvent(e)) handleEvent(e);
        float dt = std::min(clock.restart().asSeconds(), 0.1f);
        if (!active_) { sf::sleep(sf::milliseconds(50)); continue; }
        update(dt);
        render();
    }
}

// ---------------------------------------------------------------- camera

sf::Vector2f MobileGame::toWorld(sf::Vector2f px) const {
    sf::Vector2f ws(window_.getSize());
    return worldView_.getCenter() + (px - ws * 0.5f) / zoom_;
}

CellPos MobileGame::toCell(sf::Vector2f px) const {
    sf::Vector2f w = toWorld(px);
    return {int(std::floor(w.x)), int(std::floor(w.y))};
}

void MobileGame::zoomAt(sf::Vector2f px, float factor) {
    sf::Vector2f before = toWorld(px);
    zoom_ = std::clamp(zoom_ * factor, 6.f * ui_, 240.f * ui_);
    worldView_.setSize(sf::Vector2f(window_.getSize()) / zoom_);
    worldView_.move(before - toWorld(px));
}

// ---------------------------------------------------------------- events

void MobileGame::handleEvent(const sf::Event& e) {
    switch (e.type) {
    case sf::Event::Closed:
        world_.save(savePath_);
        window_.close();
        break;
    case sf::Event::Resized:
        layout();
        break;
    // On Android these mark the GL surface being destroyed / recreated.
    case sf::Event::MouseLeft:    active_ = false; break;
    case sf::Event::MouseEntered: active_ = true; break;
    case sf::Event::LostFocus:
        world_.save(savePath_); // the OS may kill us while in the background
        fingers_.clear();
        gesture_ = Gesture::None;
        break;
    case sf::Event::TouchBegan:
        touchBegan(e.touch.finger, {float(e.touch.x), float(e.touch.y)});
        break;
    case sf::Event::TouchMoved:
        touchMoved(e.touch.finger, {float(e.touch.x), float(e.touch.y)});
        break;
    case sf::Event::TouchEnded:
        touchEnded(e.touch.finger, {float(e.touch.x), float(e.touch.y)});
        break;
#ifndef __ANDROID__
    // Desktop testing: left mouse acts as one finger, wheel as pinch.
    case sf::Event::MouseButtonPressed:
        if (e.mouseButton.button == sf::Mouse::Left)
            touchBegan(0, {float(e.mouseButton.x), float(e.mouseButton.y)});
        break;
    case sf::Event::MouseMoved:
        if (fingers_.count(0)) touchMoved(0, {float(e.mouseMove.x), float(e.mouseMove.y)});
        break;
    case sf::Event::MouseButtonReleased:
        if (e.mouseButton.button == sf::Mouse::Left)
            touchEnded(0, {float(e.mouseButton.x), float(e.mouseButton.y)});
        break;
    case sf::Event::MouseWheelScrolled:
        zoomAt({float(e.mouseWheelScroll.x), float(e.mouseWheelScroll.y)},
               std::pow(1.15f, e.mouseWheelScroll.delta));
        break;
#endif
    default:
        break;
    }
}

void MobileGame::touchBegan(unsigned finger, sf::Vector2f px) {
    fingers_[finger] = px;

    if (fingers_.size() == 1) {
        for (const Button& b : buttons_)
            if (b.rect.contains(px)) { press(b); gesture_ = Gesture::Ui; return; }
        gesture_ = Gesture::Stroke;
        strokeFinger_ = finger;
        strokePending_ = mode_ != Mode::Pan;
        strokeStartPx_ = lastPx_ = px;
        strokeAge_ = 0.f;
        lastCell_ = toCell(px);
        return;
    }

    if (fingers_.size() == 2 && (gesture_ == Gesture::Stroke || gesture_ == Gesture::None)) {
        // Second finger: whatever the first one was doing becomes a pinch.
        // A stroke still pending is dropped, so pinching never leaves stray cells.
        strokePending_ = false;
        auto it = fingers_.begin();
        sf::Vector2f a = it->second, b = std::next(it)->second;
        pinchMid_ = (a + b) * 0.5f;
        pinchDist_ = std::max(length(a - b), 1.f);
        gesture_ = Gesture::Pinch;
    }
}

void MobileGame::touchMoved(unsigned finger, sf::Vector2f px) {
    if (!fingers_.count(finger)) return;
    fingers_[finger] = px;

    if (gesture_ == Gesture::Stroke && finger == strokeFinger_) {
        if (mode_ == Mode::Pan) {
            worldView_.move((lastPx_ - px) / zoom_);
        } else {
            if (strokePending_ && length(px - strokeStartPx_) > kMoveThreshold * ui_)
                commitStroke();
            if (!strokePending_) {
                CellPos c = toCell(px);
                if (!(c == lastCell_)) { paintLine(lastCell_, c); lastCell_ = c; }
            }
        }
        lastPx_ = px;
    } else if (gesture_ == Gesture::Pinch && fingers_.size() >= 2) {
        auto it = fingers_.begin();
        sf::Vector2f a = it->second, b = std::next(it)->second;
        sf::Vector2f mid = (a + b) * 0.5f;
        float dist = std::max(length(a - b), 1.f);
        worldView_.move((pinchMid_ - mid) / zoom_);
        zoomAt(mid, dist / pinchDist_);
        pinchMid_ = mid;
        pinchDist_ = dist;
    }
}

void MobileGame::touchEnded(unsigned finger, sf::Vector2f px) {
    if (gesture_ == Gesture::Stroke && finger == strokeFinger_ && strokePending_) {
        strokePending_ = false;
        tapCell(toCell(strokeStartPx_));
    }
    (void)px;
    fingers_.erase(finger);
    if (fingers_.empty()) gesture_ = Gesture::None;
    else if (gesture_ == Gesture::Pinch && fingers_.size() < 2)
        gesture_ = Gesture::Locked; // ignore the leftover finger until it lifts
}

void MobileGame::press(const Button& b) {
    switch (b.action) {
    case Action::Tool:
        mode_ = b.mode;
        if (b.mode == Mode::Place) tool_ = b.tool;
        break;
    case Action::Rotate:    toolDir_ = (toolDir_ + 1) & 3; break;
    case Action::PlayPause: paused_ = !paused_; break;
    case Action::Step:      if (paused_) sim_.tick(world_); else paused_ = true; break;
    case Action::Faster:    tps_ = std::min(tps_ * 2.f, 5120.f); break;
    case Action::Slower:    tps_ = std::max(tps_ / 2.f, 0.625f); break;
    case Action::Save:      flash(Action::Save, world_.save(savePath_)); break;
    case Action::Load:      flash(Action::Load, world_.load(savePath_)); break;
    case Action::Home:      worldView_.setCenter(0.f, 0.f); break;
    }
}

// ---------------------------------------------------------------- editing

void MobileGame::tapCell(CellPos p) {
    Cell* c = world_.getMut(p.x, p.y);
    if (mode_ == Mode::Place && c) {
        if (c->type == CellType::Switch) { // tapping a switch always toggles it
            c->state = !c->state;
            return;
        }
        if (isGate(c->type) && c->type == tool_) { // tap same gate again => rotate it
            Cell r = *c;
            r.dir = (r.dir + 1) & 3;
            world_.set(p.x, p.y, r);
            return;
        }
    }
    applyCell(p);
}

void MobileGame::applyCell(CellPos p) {
    if (mode_ == Mode::Erase) { world_.erase(p.x, p.y); return; }
    if (mode_ != Mode::Place) return;
    uint8_t dir = isGate(tool_) ? toolDir_ : 0;
    const Cell& cur = world_.get(p);
    if (cur.type != tool_ || cur.dir != dir) world_.set(p.x, p.y, Cell{tool_, dir, false});
}

void MobileGame::paintLine(CellPos a, CellPos b) {
    // Bresenham, 4-connected so fast swipes still produce touching wires.
    int dx = std::abs(b.x - a.x), dy = -std::abs(b.y - a.y);
    int sx = a.x < b.x ? 1 : -1, sy = a.y < b.y ? 1 : -1, err = dx + dy;
    CellPos p = a;
    while (true) {
        applyCell(p);
        if (p == b) break;
        if (2 * err - dy > dx - 2 * err) { err += dy; p.x += sx; }
        else                             { err += dx; p.y += sy; }
    }
}

void MobileGame::commitStroke() {
    strokePending_ = false;
    lastCell_ = toCell(strokeStartPx_);
    applyCell(lastCell_);
}

// ---------------------------------------------------------------- frame

void MobileGame::flash(Action a, bool ok) {
    flashAction_ = a;
    flashOk_ = ok;
    flashTime_ = 0.6f;
}

void MobileGame::update(float dt) {
    if (gesture_ == Gesture::Stroke && strokePending_) {
        strokeAge_ += dt;
        if (strokeAge_ > kHoldToPaint) commitStroke();
    }
    if (!paused_) {
        tickAccum_ += dt * tps_;
        int budget = 2000;
        while (tickAccum_ >= 1.f && budget-- > 0) { sim_.tick(world_); tickAccum_ -= 1.f; }
        if (budget <= 0) tickAccum_ = 0.f;
    }
    sim_.refresh(world_);
    flashTime_ = std::max(0.f, flashTime_ - dt);
}

void MobileGame::render() {
    window_.clear(sf::Color(18, 20, 26));

    window_.setView(worldView_);
    sf::Vector2f c = worldView_.getCenter(), s = worldView_.getSize();
    int x0 = int(std::floor(c.x - s.x / 2)) - 1, x1 = int(std::ceil(c.x + s.x / 2)) + 1;
    int y0 = int(std::floor(c.y - s.y / 2)) - 1, y1 = int(std::ceil(c.y + s.y / 2)) + 1;

    if (zoom_ >= 14.f * ui_) {
        sf::VertexArray grid(sf::Lines);
        render::buildGrid(grid, x0, y0, x1, y1);
        window_.draw(grid);
    }
    sf::VertexArray cells(sf::Triangles);
    render::buildCells(cells, world_, sim_, x0, y0, x1, y1);
    window_.draw(cells);

    window_.setView(uiView_);
    drawUi();
    window_.display();
}

void MobileGame::drawUi() {
    sf::Vector2f size(window_.getSize());
    const float w = kBarWidth * ui_;
    sf::VertexArray va(sf::Triangles);

    render::addRect(va, 0, 0, w, size.y, sf::Color(26, 28, 36, 235));
    render::addRect(va, size.x - w, 0, size.x, size.y, sf::Color(26, 28, 36, 235));

    const sf::Color fg(225, 228, 235), dim(120, 125, 140);

    for (const Button& b : buttons_) {
        const sf::FloatRect& r = b.rect;
        bool selected = b.action == Action::Tool && b.mode == mode_ &&
                        (b.mode != Mode::Place || b.tool == tool_);
        bool toggled = (b.action == Action::PlayPause && paused_);
        sf::Color bg = selected ? sf::Color(70, 90, 140) : sf::Color(40, 44, 56);
        if (toggled) bg = sf::Color(110, 70, 50);
        if (flashTime_ > 0 && b.action == flashAction_)
            bg = flashOk_ ? sf::Color(50, 130, 70) : sf::Color(150, 50, 50);
        render::addRect(va, r.left, r.top, r.left + r.width, r.top + r.height, bg);

        sf::Vector2f ctr(r.left + r.width / 2, r.top + r.height / 2);
        float g = std::min(r.width, r.height) * 0.62f; // icon size
        float q = g * 0.5f;

        switch (b.action) {
        case Action::Tool:
            if (b.mode == Mode::Place) {
                Cell sample{b.tool, isGate(b.tool) ? toolDir_ : uint8_t(0), true};
                uint8_t mask = b.tool == CellType::Wire ? 0b0101 : 0; // E + W arms
                render::addCellGlyph(va, sample, ctr - sf::Vector2f(q, q), g, true, mask);
            } else if (b.mode == Mode::Erase) { // red X
                sf::Color red(235, 80, 80);
                float t = g * 0.09f;
                auto bar = [&](sf::Vector2f a, sf::Vector2f z) {
                    sf::Vector2f d = z - a, n = sf::Vector2f(-d.y, d.x) / length(d) * t;
                    tri(va, a + n, z + n, z - n, red);
                    tri(va, a + n, z - n, a - n, red);
                };
                bar(ctr + sf::Vector2f(-q, -q), ctr + sf::Vector2f(q, q));
                bar(ctr + sf::Vector2f(-q, q), ctr + sf::Vector2f(q, -q));
            } else { // pan: four arrows
                render::addRect(va, ctr.x - q * 0.6f, ctr.y - g * 0.04f, ctr.x + q * 0.6f,
                                ctr.y + g * 0.04f, fg);
                render::addRect(va, ctr.x - g * 0.04f, ctr.y - q * 0.6f, ctr.x + g * 0.04f,
                                ctr.y + q * 0.6f, fg);
                for (int d = 0; d < 4; ++d)
                    arrow(va, ctr + sf::Vector2f(DX[d], DY[d]) * (q * 0.75f), q * 0.3f, d, fg);
            }
            break;
        case Action::Rotate: // arrow showing the current placement direction
            render::addRect(va, ctr.x - q * 0.35f, ctr.y - q * 0.35f, ctr.x + q * 0.35f,
                            ctr.y + q * 0.35f, dim);
            arrow(va, ctr + sf::Vector2f(DX[toolDir_], DY[toolDir_]) * (q * 0.65f), q * 0.4f,
                  toolDir_, fg);
            break;
        case Action::PlayPause:
            if (paused_) arrow(va, ctr, q * 0.7f, 0, fg); // show "play"
            else {
                render::addRect(va, ctr.x - q * 0.6f, ctr.y - q * 0.7f, ctr.x - q * 0.15f,
                                ctr.y + q * 0.7f, fg);
                render::addRect(va, ctr.x + q * 0.15f, ctr.y - q * 0.7f, ctr.x + q * 0.6f,
                                ctr.y + q * 0.7f, fg);
            }
            break;
        case Action::Step:
            arrow(va, ctr - sf::Vector2f(q * 0.2f, 0), q * 0.55f, 0, fg);
            render::addRect(va, ctr.x + q * 0.45f, ctr.y - q * 0.6f, ctr.x + q * 0.7f,
                            ctr.y + q * 0.6f, fg);
            break;
        case Action::Faster:
        case Action::Slower: {
            int dir = b.action == Action::Faster ? 3 : 1;
            arrow(va, ctr - sf::Vector2f(0, q * 0.3f), q * 0.5f, dir, fg);
            // speed meter: one pip per doubling above the slowest speed
            int level = int(std::round(std::log2(tps_ / 0.625f)));
            float pw = r.width * 0.8f / 14.f;
            for (int i = 0; i < 14; ++i) {
                float x = r.left + r.width * 0.1f + i * pw;
                render::addRect(va, x + 1, ctr.y + q * 0.55f, x + pw - 1, ctr.y + q * 0.8f,
                                i <= level ? sf::Color(255, 200, 60) : sf::Color(60, 64, 76));
            }
            break;
        }
        case Action::Save:
        case Action::Load: {
            render::addRect(va, ctr.x - q * 0.75f, ctr.y + q * 0.45f, ctr.x + q * 0.75f,
                            ctr.y + q * 0.65f, fg); // tray
            render::addRect(va, ctr.x - q * 0.1f, ctr.y - q * 0.7f, ctr.x + q * 0.1f,
                            ctr.y + q * 0.05f, fg);
            bool down = b.action == Action::Save;
            arrow(va, ctr + sf::Vector2f(0, down ? q * 0.05f : -q * 0.7f), q * 0.38f,
                  down ? 1 : 3, fg);
            break;
        }
        case Action::Home: // crosshair
            render::addRect(va, ctr.x - q * 0.8f, ctr.y - g * 0.03f, ctr.x + q * 0.8f,
                            ctr.y + g * 0.03f, fg);
            render::addRect(va, ctr.x - g * 0.03f, ctr.y - q * 0.8f, ctr.x + g * 0.03f,
                            ctr.y + q * 0.8f, fg);
            render::addRect(va, ctr.x - q * 0.3f, ctr.y - q * 0.3f, ctr.x + q * 0.3f,
                            ctr.y + q * 0.3f, sf::Color(255, 200, 60));
            break;
        }
    }
    window_.draw(va);
}
