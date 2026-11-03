// Nova Engine — main engine facade.
// Owns the scene manager, 2D physics world, script VM, renderer, input state,
// console log, and the fixed-timestep update loop. Implements NovaHost so
// scripts can query/modify the world.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "nova/scene/SceneManager.hpp"
#include "nova/physics2d/World2D.hpp"
#include "nova/script/novascript.hpp"
#include "nova/render/RenderDevice.hpp"
#include "nova/math.hpp"

namespace nova {

struct TouchInput {
    int id = -1;
    float x = 0.0f;
    float y = 0.0f;
    bool pressed = false;
};

struct InputState {
    std::vector<TouchInput> touches;
    std::map<std::string, bool> keys;
    float mouseX = 0.0f, mouseY = 0.0f;
    bool mouseDown = false;
};

struct EngineStats {
    float fps = 0.0f;
    float frameMs = 0.0f;
    int physicsBodies = 0;
    int scriptErrors = 0;
    int totalDrawCalls = 0;
};

class NovaEngine : public NovaHost {
public:
    NovaEngine();

    // --- NovaHost -----------------------------------------------------------
    void log(const std::string& msg) override;
    Node* findNode(const std::string& path) override;
    NsValue getProp(Node* node, const std::string& key) override;
    void setProp(Node* node, const std::string& key, const NsValue& value) override;

    // --- Lifecycle ------------------------------------------------------------
    bool initialize(std::string* error);
    void shutdown();

    // --- Main loop --------------------------------------------------------------
    // Advance simulation by dt (seconds). Physics runs fixed-step.
    void update(float dt);

    // --- Scene / content --------------------------------------------------------
    std::shared_ptr<Scene> currentScene();

    // Scene management (editor surface).
    bool newScene(const std::string& name, std::string* error);
    bool openScene(const std::string& name, std::string* error);
    bool saveScene(std::string* error);
    std::string sceneName();
    std::string projectPath() { return sceneManager.projectPath; }
    bool openProject(const std::string& projectPath, std::string* error);
    bool saveProject(std::string* error);
    std::shared_ptr<Node> createNode(const std::string& type, const std::string& name,
                                     std::shared_ptr<Node> parent);
    bool deleteNode(Node* node);
    bool renameNode(Node* node, const std::string& newName);

    // --- Rendering ---------------------------------------------------------------
    void setRenderer(std::unique_ptr<RenderDevice> r) { renderer = std::move(r); }
    void resize(int w, int h);
    void renderFrame(const Vec4& clearColor);

    // --- Input ---------------------------------------------------------------------
    InputState& input() { return input_; }
    void setTouch(int id, float x, float y, bool pressed);
    void clearTouches();
    void setKey(const std::string& key, bool down);

    // --- Scripting ---------------------------------------------------------------
    // Run a compiled script attached to a node (on_ready).
    bool runScriptOnNode(Node* node, std::string* error);
    bool runScriptSource(const std::string& source, std::string* error);
    NovaScriptVM& scriptVM() { return vm_; }

    // --- Physics accessors (editor bindings) ---------------------------------------
    World2D& physicsWorld() { return physics2d; }

    // --- Console / stats ----------------------------------------------------------
    const std::vector<std::string>& consoleLog() const { return console_; }
    const EngineStats& stats() const { return stats_; }
    void clearConsole() { console_.clear(); }

    // --- Assets (Phase 3+) ---------------------------------------------------------
    std::map<std::string, std::string> textures; // path -> name

private:
    void updatePhysics(float dt);
    void runReadyScripts();
    void renderSceneNodes(Node* node, const Mat4& parentViewProj, int depth);
    void injectTouchInputIntoScripts();

    SceneManager sceneManager;
    World2D physics2d;
    NovaScriptVM vm_;
    std::unique_ptr<RenderDevice> renderer;
    InputState input_;
    EngineStats stats_;
    std::vector<std::string> console_;
    std::map<Node*, bool> scriptReady_;
    Node* lastScriptScene_ = nullptr;
    int width_ = 0, height_ = 0;
    float accumulator_ = 0.0f;
    float timeAccum_ = 0.0f;
    int frameCount_ = 0;
    bool initialized_ = false;

    // 3D camera for the current scene (editor viewport).
    Vec3 cameraPos = Vec3(0, 0, 10);
    Vec3 cameraTarget = Vec3(0, 0, 0);
};

// Create the default engine (null renderer in core; GL backend injected on Android).
std::unique_ptr<NovaEngine> createEngine();

} // namespace nova
