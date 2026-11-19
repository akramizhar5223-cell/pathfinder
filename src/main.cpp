#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

#include <vector>
#include <cmath>

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Pathfinder — predictive path-simulation autoplay bot (cube mode, v1)
//
// Instead of reacting to hazards when they are already on top of you, the bot
// simulates two trajectories of the player every frame:
//   A) do nothing
//   B) jump now
// If trajectory A ends in a hazard collision, the bot taps jump (trajectory B).
// The simulated trajectory is drawn on screen when "Show Predicted Path" is on.
//
// NOTE: physics constants are community estimates, exposed as settings because
// we cannot read GD's exact per-version values. Behaviour is therefore
// "experimental" and aimed at cube, normal-gravity, hazard avoidance.
// ---------------------------------------------------------------------------

static bool g_botEnabled = false;
static bool g_jumpHeld   = false;

static float modFloat(const char* key) {
    return static_cast<float>(Mod::get()->getSettingValue<double>(key));
}
static bool modBool(const char* key) {
    return Mod::get()->getSettingValue<bool>(key);
}

static CCNode* objectLayer(PlayLayer* pl) {
    return pl->getChildByTag(9);
}

// Hazard rects (GameObjectType::Hazard / AnimatedHazard) within the lookahead window.
static std::vector<CCRect> collectHazards(PlayLayer* pl, float playerX, float lookahead) {
    std::vector<CCRect> out;
    auto* objLayer = objectLayer(pl);
    if (!objLayer) return out;

    auto& children = *objLayer->getChildren();
    for (unsigned int i = 0; i < children.count(); ++i) {
        auto* obj = dynamic_cast<GameObject*>(children.objectAtIndex(i));
        if (!obj || !obj->isVisible()) continue;
        if (obj->m_objectType != GameObjectType::Hazard &&
            obj->m_objectType != GameObjectType::AnimatedHazard) {
            continue;
        }
        float hw = obj->getContentWidth()  * obj->getScaleX() * 0.5f;
        float hh = obj->getContentHeight() * obj->getScaleY() * 0.5f;
        CCPoint pos = obj->getPosition(); // objLayer-local space (same space we simulate in)
        CCRect rect(pos.x - hw, pos.y - hh, hw * 2.f, hh * 2.f);
        if (rect.origin.x > playerX + lookahead + 60.f) continue;
        if (rect.origin.x + rect.size.width < playerX - 60.f) continue;
        out.push_back(rect);
    }
    return out;
}

static bool playerHitsRect(const CCRect& r, float px, float py, float halfW, float halfH) {
    float left   = px - halfW;
    float right  = px + halfW;
    float bottom = py - halfH;
    float top    = py + halfH;
    float rLeft  = r.origin.x;
    float rRight = r.origin.x + r.size.width;
    float rBot   = r.origin.y;
    float rTop   = r.origin.y + r.size.height;
    return !(right < rLeft || left > rRight || top < rBot || bottom > rTop);
}

struct SimPath {
    bool collided = false;
    std::vector<CCPoint> points;
};

// Simulate forward from (startX, startY) with initial vertical velocity vy0 (px/s),
// horizontal speed vx and downward gravity g (px/s^2, positive value pulling down).
// Stops on hazard collision or after `seconds` elapse.
static SimPath simulate(
    const std::vector<CCRect>& hazards,
    float startX, float startY,
    float vx, float vy0, float g,
    float seconds, float halfW, float halfH
) {
    SimPath path;
    float x = startX, y = startY, vy = vy0;
    const float dt = 1.0f / 240.0f;
    int steps = static_cast<int>(seconds / dt);
    for (int i = 0; i < steps; ++i) {
        vy -= g * dt;
        y  += vy * dt;
        x  += vx * dt;

        if (i % 4 == 0) path.points.push_back(CCPoint(x, y));

        for (auto const& h : hazards) {
            if (h.origin.x > x + halfW + 5.f) continue;
            if (h.origin.x + h.size.width < x - halfW - 5.f) continue;
            if (playerHitsRect(h, x, y, halfW, halfH)) {
                path.collided = true;
                path.points.push_back(CCPoint(x, y));
                return path;
            }
        }
    }
    return path;
}

// Path rendering node (retained once, re-added to whatever object layer is active).
static CCDrawNode* g_pathNode = nullptr;

static void drawPredictedPath(CCNode* objLayer, SimPath const& path, bool dangerous) {
    if (!modBool("show-path")) {
        if (g_pathNode && g_pathNode->getParent()) g_pathNode->clear();
        return;
    }
    if (!objLayer) return;
    if (!g_pathNode) {
        g_pathNode = CCDrawNode::create();
        g_pathNode->retain();
    }
    if (g_pathNode->getParent() != objLayer) {
        if (g_pathNode->getParent()) g_pathNode->removeFromParentAndCleanup(false);
        objLayer->addChild(g_pathNode, 999);
    }
    g_pathNode->clear();
    ccColor4F color = dangerous
        ? ccColor4F{1.0f, 0.25f, 0.25f, 0.9f}
        : ccColor4F{0.25f, 1.0f, 0.45f, 0.9f};
    for (size_t i = 1; i < path.points.size(); ++i) {
        g_pathNode->drawSegment(path.points[i - 1], path.points[i], 2.5f, color);
    }
}

static void refreshToggleVisual(CCNode* layer) {
    auto* menu = static_cast<CCMenu*>(layer->getChildByID("pathfinder-menu"));
    if (!menu) return;
    auto* btn  = static_cast<CCMenuItemSpriteExtra*>(menu->getChildByID("pathfinder-toggle-btn"));
    if (!btn) return;
    auto* bg  = static_cast<CCScale9Sprite*>(btn->getNormalImage());
    if (!bg) return;
    auto* lbl = static_cast<CCLabelBMFont*>(bg->getChildren()->objectAtIndex(0));
    if (!lbl) return;
    lbl->setString(g_botEnabled ? "Pathfinder: ON" : "Pathfinder: OFF");
    lbl->setColor(g_botEnabled ? ccColor3B{50, 255, 100} : ccColor3B{255, 80, 80});
}

class $modify(PathfinderPlayLayer, PlayLayer) {
    void update(float dt) {
        PlayLayer::update(dt);
        if (!g_botEnabled) {
            if (g_pathNode && g_pathNode->getParent()) g_pathNode->clear();
            return;
        }
        if (!m_player1 || m_player1->m_isDead) return;

        auto* objLayer = objectLayer(this);

        // Player position in object-layer space (same space hazards live in).
        CCPoint world = m_player1->getParent()
            ? m_player1->getParent()->convertToWorldSpace(m_player1->getPosition())
            : m_player1->getPosition();
        CCPoint start = objLayer ? objLayer->convertToNodeSpace(world) : world;

        float halfW = m_player1->getContentWidth()  * m_player1->getScaleX() * 0.5f;
        float halfH = m_player1->getContentHeight() * m_player1->getScaleY() * 0.5f;

        float lookahead = modFloat("lookahead");
        float simTime   = modFloat("sim-time");
        float gravity   = modFloat("gravity");
        float jumpVel   = modFloat("jump-velocity");
        float moveSpeed = modFloat("move-speed");

        auto hazards = collectHazards(this, start.x, lookahead);

        // Branch A: keep running (assume grounded, vy = 0).
        SimPath noJump = simulate(hazards, start.x, start.y, moveSpeed, 0.0f, gravity, simTime, halfW, halfH);

        // Branch B: jump right now.
        SimPath jump = simulate(hazards, start.x, start.y, moveSpeed, jumpVel, gravity, simTime, halfW, halfH);

        bool danger = noJump.collided;
        SimPath const& shown = danger ? jump : noJump;

        drawPredictedPath(objLayer, shown, danger && jump.collided);

        if (danger && !g_jumpHeld) {
            handleButton(true, 1, false);
            g_jumpHeld = true;
        } else if (!danger && g_jumpHeld) {
            handleButton(false, 1, false);
            g_jumpHeld = false;
        }
    }

    void resetLevel() {
        g_jumpHeld = false;
        if (g_pathNode) g_pathNode->clear();
        PlayLayer::resetLevel();
    }
};

class $modify(PathfinderPauseLayer, PauseLayer) {
    static PauseLayer* create(bool p0) {
        auto* layer = PauseLayer::create(p0);
        if (!layer) return layer;

        auto* winSize = CCDirector::sharedDirector()->getWinSize();

        auto* bg = CCScale9Sprite::create("GJ_button_01.png");
        bg->setContentSize({ 160.f, 36.f });

        auto* lbl = CCLabelBMFont::create(
            g_botEnabled ? "Pathfinder: ON" : "Pathfinder: OFF",
            "bigFont.fnt"
        );
        lbl->setScale(0.5f);
        lbl->setColor(g_botEnabled ? ccColor3B{50, 255, 100} : ccColor3B{255, 80, 80});
        lbl->setPosition({ 80.f, 18.f });
        bg->addChild(lbl, 1);

        auto* btn = CCMenuItemSpriteExtra::create(bg, static_cast<PathfinderPauseLayer*>(layer), menu_selector(PathfinderPauseLayer::onPathfinderToggle));
        btn->setID("pathfinder-toggle-btn");

        auto* menu = CCMenu::create();
        menu->setID("pathfinder-menu");
        menu->setTouchPriority(-500);
        menu->addChild(btn);
        menu->setPosition(winSize.width / 2.f, 30.f);
        layer->addChild(menu, 500);

        return layer;
    }

    void onPathfinderToggle(CCObject*) {
        g_botEnabled = !g_botEnabled;
        g_jumpHeld = false;
        if (!g_botEnabled) {
            auto* pl = PlayLayer::get();
            if (pl && pl->m_player1 && !pl->m_player1->m_isDead) {
                pl->handleButton(false, 1, false);
            }
        }
        refreshToggleVisual(this);
        log::info("[Pathfinder] Bot {}", g_botEnabled ? "ON" : "OFF");
    }
};

class $modify(PathfinderKeyboard, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double extra) {
        bool result = CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, extra);
        if (key == enumKeyCodes::KEY_B && isKeyDown && !isKeyRepeat) {
            auto* pl = PlayLayer::get();
            if (pl && !pl->m_isPaused) {
                g_botEnabled = !g_botEnabled;
                g_jumpHeld = false;
                if (!g_botEnabled && pl->m_player1 && !pl->m_player1->m_isDead) {
                    pl->handleButton(false, 1, false);
                }
                log::info("[Pathfinder] Bot {}", g_botEnabled ? "ON" : "OFF");
            }
        }
        return result;
    }
};

$on_mod(Loaded) {
    log::info("[Pathfinder] v1.0.0 loaded. Toggle in the pause menu, or press B on PC.");
}
