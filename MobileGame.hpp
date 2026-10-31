#pragma once
#include "Simulation.hpp"
#include "World.hpp"
#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <vector>

// Touch front end for phones (designed for 2400x1080 landscape, scales to others).
//
// Left bar: tools (Wire, Switch, Lamp, AND, OR, NOT, XOR, Erase, Pan).
// Right bar: Rotate, Play/Pause, Step, Faster, Slower, Save, Load, Recenter.
// Canvas: tap = place / toggle switch / rotate same gate; drag = paint;
//         two fingers = pan + pinch zoom.
class MobileGame {
public:
    explicit MobileGame(std::string savePath);
    void run();

private:
    enum class Mode { Place, Erase, Pan };
    enum class Action { Tool, Rotate, PlayPause, Step, Faster, Slower, Save, Load, Home };
    enum class Gesture { None, Ui, Stroke, Pinch, Locked };

    struct Button {
        sf::FloatRect rect;
        Action action;
        Mode mode = Mode::Place;
        CellType tool = CellType::Empty;
    };

    // events
    void handleEvent(const sf::Event& e);
    void touchBegan(unsigned finger, sf::Vector2f px);
    void touchMoved(unsigned finger, sf::Vector2f px);
    void touchEnded(unsigned finger, sf::Vector2f px);
    void press(const Button& b);

    // editing
    void tapCell(CellPos p);
    void applyCell(CellPos p);
    void paintLine(CellPos a, CellPos b);
    void commitStroke();

    // camera
    sf::Vector2f toWorld(sf::Vector2f px) const;
    CellPos toCell(sf::Vector2f px) const;
    void zoomAt(sf::Vector2f px, float factor);
    void layout();

    // frame
    void update(float dt);
    void render();
    void drawUi();
    void flash(Action a, bool ok);

    sf::RenderWindow window_;
    sf::View worldView_, uiView_;
    World world_;
    Simulation sim_;
    std::string savePath_;

    float ui_ = 1.f;     // UI scale relative to a 1080px-tall screen
    float zoom_ = 64.f;  // pixels per cell
    Mode mode_ = Mode::Place;
    CellType tool_ = CellType::Wire;
    uint8_t toolDir_ = 0;

    bool active_ = true; // false while Android has no surface (app in background)
    bool paused_ = false;
    float tps_ = 10.f, tickAccum_ = 0.f;

    std::vector<Button> buttons_;
    std::map<unsigned, sf::Vector2f> fingers_;
    Gesture gesture_ = Gesture::None;

    // single-finger stroke
    unsigned strokeFinger_ = 0;
    bool strokePending_ = false; // not yet painting: could still become a tap or a pinch
    sf::Vector2f strokeStartPx_, lastPx_;
    float strokeAge_ = 0.f;
    CellPos lastCell_;

    // pinch
    sf::Vector2f pinchMid_;
    float pinchDist_ = 1.f;

    // button feedback
    Action flashAction_ = Action::Tool;
    bool flashOk_ = true;
    float flashTime_ = 0.f;
};
