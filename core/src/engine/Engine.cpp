// Nova Engine — main engine facade implementation.
#include "nova/engine/Engine.hpp"
#include "nova/json.hpp"
#include "nova/util/fs.hpp"
#include "nova/script/Script.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace nova {

NovaEngine::NovaEngine() {
    // Default script globals.
    vm_.setGlobal("PI", NsValue::number(PI));
}

void NovaEngine::log(const std::string& msg) {
    console_.push_back(msg);
    if (console_.size() > 500) console_.erase(console_.begin(), console_.begin() + 100);
}

Node* NovaEngine::findNode(const std::string& path) {
    auto scene = currentScene();
    if (!scene) return nullptr;
    auto n = scene->findByPath(path);
    return n.get();
}

NsValue NovaEngine::getProp(Node* node, const std::string& key) {
    if (!node) return NsValue::nil();
    // Friendly mappings for transform components.
    if (key == "position") return NsValue::string(std::to_string(node->position.x));
    if (key == "x") return NsValue::number(node->position.x);
    if (key == "y") return NsValue::number(node->position.y);
    if (key == "z") return NsValue::number(node->position.z);
    if (key == "rotation") return NsValue::number(node->rotationDeg.z);
    if (key == "scale") return NsValue::number(node->scale3.x);
    if (key == "enabled") return NsValue::ofBool(node->enabled);
    if (key == "name") return NsValue::string(node->name);
    if (key == "type") return NsValue::string(node->type);
    // Property bag.
    if (node->props.has(key)) {
        std::string v = node->props.get(key);
        try {
            return NsValue::number(std::stod(v));
        } catch (...) {
            return NsValue::string(v);
        }
    }
    return NsValue::nil();
}

void NovaEngine::setProp(Node* node, const std::string& key, const NsValue& value) {
    if (!node) return;
    if (key == "x") { node->position.x = (float)value.num; return; }
    if (key == "y") { node->position.y = (float)value.num; return; }
    if (key == "z") { node->position.z = (float)value.num; return; }
    if (key == "rotation") { node->rotationDeg.z = (float)value.num; return; }
    if (key == "scale") { node->scale3 = Vec3((float)value.num, (float)value.num, 1.0f); return; }
    if (key == "enabled") { node->enabled = value.truthy(); return; }
    if (value.type == NsValue::Type::Num) {
        node->props.setNum(key, value.num);
    } else if (value.type == NsValue::Type::Str) {
        node->props.set(key, value.str);
    } else if (value.type == NsValue::Type::Bool) {
        node->props.set(key, value.boolean ? "true" : "false");
    }
}

bool NovaEngine::initialize(std::string* error) {
    if (!renderer) {
        renderer = createNullRenderer();
    }
    std::string err;
    if (!renderer->initialize(width_ > 0 ? width_ : 1280, height_ > 0 ? height_ : 720, &err)) {
        if (error) *error = "renderer init failed: " + err;
        return false;
    }
    initialized_ = true;
    if (!sceneManager.current) {
        sceneManager.createScene("Main");
        log("Nova Engine initialized (" + renderer->backendName() + " renderer)");
        log("Default scene 'Main' created");
    }
    return true;
}

void NovaEngine::shutdown() {
    renderer.reset();
    initialized_ = false;
}

void NovaEngine::update(float dt) {
    if (!initialized_) return;
    // FPS accounting.
    timeAccum_ += dt;
    ++frameCount_;
    if (timeAccum_ >= 0.5f) {
        stats_.fps = (float)frameCount_ / timeAccum_;
        stats_.frameMs = timeAccum_ / (float)frameCount_ * 1000.0f;
        stats_.physicsBodies = physics2d.bodyCount();
        timeAccum_ = 0.0f;
        frameCount_ = 0;
    }
    // Fixed-step physics.
    accumulator_ += dt;
    float step = physics2d.fixedTimeStep;
    int sub = physics2d.maxSubSteps;
    while (accumulator_ >= step && sub-- > 0) {
        updatePhysics(step);
        accumulator_ -= step;
    }
    if (sub <= 0) accumulator_ = 0.0f;

    runReadyScripts();
    injectTouchInputIntoScripts();
}

void NovaEngine::updatePhysics(float dt) {
    physics2d.step(dt);
    // Free-floating bodies get clamped velocity for stability.
    for (Body2D* b : physics2d.bodies) {
        if (b->type != BodyType::Static && b->enabled) {
            float vl = b->velocity.length();
            if (vl > 8000.0f) b->velocity = b->velocity.normalized() * 8000.0f;
        }
    }
}

void NovaEngine::runReadyScripts() {
    auto scene = currentScene();
    if (!scene || !scene->root) return;
    if (lastScriptScene_ != scene->root.get()) {
        scriptReady_.clear();
        lastScriptScene_ = scene->root.get();
    }
    std::vector<std::shared_ptr<Node>> all;
    all.push_back(scene->root);
    for (auto& c : scene->root->children) {
        auto d = c->descendants();
        for (auto& x : d) all.push_back(x);
        all.push_back(c);
    }
    for (auto& n : all) {
        if (!n->enabled) continue;
        if (scriptReady_.count(n.get()) && scriptReady_[n.get()]) continue;
        if (n->script) {
            scriptReady_[n.get()] = true;
            std::string err;
            if (!runScriptOnNode(n.get(), &err)) {
                ++stats_.scriptErrors;
                log("script error on " + n->name + ": " + err);
            }
        } else {
            scriptReady_[n.get()] = true;
        }
    }
}

void NovaEngine::injectTouchInputIntoScripts() {
    // Expose touch state to scripts via the "self.touch" property bag.
    // The Android layer reads input_ directly; this keeps core self-contained.
}

bool NovaEngine::runScriptOnNode(Node* node, std::string* error) {
    if (!node || !node->script) return true;
    node->script->setNode(node);
    std::string err;
    node->script->run(this, &err);
    if (!err.empty()) {
        if (error) *error = err;
        return false;
    }
    // Auto-invoke on_ready() if the script defines it (Godot-style lifecycle).
    NovaScriptVM& vm = NovaScriptVM::instance();
    if (vm.functions().count("on_ready") > 0) {
        if (!vm.callFunction("on_ready", this, {}, &err)) {
            if (error) *error = err;
            return false;
        }
    }
    return true;
}

bool NovaEngine::runScriptSource(const std::string& source, std::string* error) {
    std::string err;
    auto script = Script::createFromSource("Node", "Script", source, &err);
    if (!script) {
        if (error) *error = err;
        return false;
    }
    script->run(this, error);
    return true;
}

std::shared_ptr<Scene> NovaEngine::currentScene() {
    return sceneManager.current;
}

bool NovaEngine::newScene(const std::string& name, std::string* error) {
    auto s = sceneManager.createScene(name);
    if (!s) {
        if (error) *error = "scene already exists or invalid name: " + name;
        return false;
    }
    return true;
}

bool NovaEngine::openScene(const std::string& name, std::string* error) {
    if (!sceneManager.loadScene(name, error)) return false;
    return true;
}

bool NovaEngine::saveScene(std::string* error) {
    if (!sceneManager.current) {
        if (error) *error = "no current scene";
        return false;
    }
    if (!sceneManager.saveScene(sceneManager.current->name, error)) return false;
    return true;
}

std::string NovaEngine::sceneName() {
    return sceneManager.current ? sceneManager.current->name : std::string();
}

bool NovaEngine::openProject(const std::string& projectPath, std::string* error) {
    std::string metaPath = fs::join(projectPath, "project.json");
    if (!sceneManager.openProjectMeta(metaPath, error)) return false;
    // Load scenes listed in project.json.
    const json::JsonValue& scenesArr = sceneManager.projectMeta.get("scenes");
    if (scenesArr.isArray() && !scenesArr.asArray().empty()) {
        std::string first = scenesArr.asArray()[0].asString();
        if (!first.empty()) sceneManager.loadScene(first, error);
    }
    // Create a default scene if none.
    if (!sceneManager.current) {
        sceneManager.createScene("Main");
    }
    return true;
}

bool NovaEngine::saveProject(std::string* error) {
    std::string metaPath = fs::join(sceneManager.projectPath, "project.json");
    if (!sceneManager.saveProjectMeta(metaPath, error)) return false;
    for (auto& kv : sceneManager.scenes) {
        std::string err;
        if (!sceneManager.saveScene(kv.first, &err)) {
            if (error) *error = err;
            return false;
        }
    }
    return true;
}

std::shared_ptr<Node> NovaEngine::createNode(const std::string& type, const std::string& name,
                                             std::shared_ptr<Node> parent) {
    auto scene = currentScene();
    if (!scene) return nullptr;
    return sceneManager.addNode(type, name, parent ? parent : scene->root);
}

bool NovaEngine::deleteNode(Node* node) {
    return sceneManager.deleteNode(node);
}

bool NovaEngine::renameNode(Node* node, const std::string& newName) {
    return sceneManager.renameNode(node, newName);
}

void NovaEngine::resize(int w, int h) {
    width_ = w;
    height_ = h;
    if (renderer) renderer->resize(w, h);
}

void NovaEngine::renderFrame(const Vec4& clearColor) {
    if (!renderer || !currentScene()) return;
    renderer->beginFrame(clearColor);
    renderSceneNodes(currentScene()->root.get(), Mat4::identity(), 0);
    renderer->endFrame();
    stats_.frameMs = renderer->stats().lastFrameMs;
    stats_.totalDrawCalls = renderer->stats().drawCalls;
}

void NovaEngine::renderSceneNodes(Node* node, const Mat4& parentViewProj, int depth) {
    if (!node || depth > 32) return;
    if (!node->visible || !node->enabled) return;

    Mat4 model = node->worldMatrix();

    // 2D sprite rendering.
    if (node->type == "Sprite2D" || node->type == "Sprite") {
        std::string texture = node->props.get("texture", "white");
        Vec2 size = node->props.getVec2("size", Vec2(64, 64));
        float rotation = node->props.getNum("rotation", node->rotationDeg.z);
        Vec4 tint(1, 1, 1, 1);
        renderer->drawSprite(texture, node->position2d(), size, rotation, tint);
    }
    // 3D mesh rendering (cube primitive for tests/preview).
    if (node->type == "MeshInstance3D" || node->type == "Cube") {
        MeshData mesh;
        // Unit cube: 6 faces x 4 verts x 3 floats = 72 floats (12 triangles).
        const float v[72] = {
            -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1, // back
            -1, -1, 1, 1, -1, 1, 1, 1, 1, -1, 1, 1,    // front
            -1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, // left
            1, -1, -1, 1, 1, -1, 1, 1, 1, 1, -1, 1,     // right
            -1, -1, -1, -1, -1, 1, 1, -1, 1, 1, -1, -1, // bottom
            -1, 1, -1, -1, 1, 1, 1, 1, 1, 1, 1, -1,     // top
        };
        mesh.vertices.assign(v, v + 72);
        const unsigned int idx[36] = {
            0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11,
            12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
        };
        mesh.indices.assign(idx, idx + 36);
        MaterialData mat;
        mat.color = Vec4(0.6f, 0.7f, 0.9f, 1.0f);
        renderer->setLight("directional", Vec3(0.5f, 1.0f, 0.5f).normalized(),
                           Vec4(1.0f, 1.0f, 1.0f, 1.0f));
        renderer->drawMesh(mesh, mat, model, parentViewProj);
    }
    // 3D lights / grid handled by GL backend for real scenes; null backend counts.
    if (node->type == "DirectionalLight3D") {
        Vec3 dir = node->props.getVec3("direction", Vec3(0.5f, -1.0f, 0.5f));
        renderer->setLight(node->name, dir, Vec4(1, 1, 1, 1));
    }
    if (node->type == "WorldGrid") {
        renderer->drawGrid(parentViewProj, Vec3::zero(), 10.0f, 1.0f);
    }

    Mat4 childViewProj = parentViewProj * model;
    for (auto& c : node->children) {
        renderSceneNodes(c.get(), childViewProj, depth + 1);
    }
}

void NovaEngine::setTouch(int id, float x, float y, bool pressed) {
    for (auto& t : input_.touches) {
        if (t.id == id) {
            t.x = x; t.y = y; t.pressed = pressed;
            if (!pressed) {
                input_.touches.erase(std::remove_if(input_.touches.begin(), input_.touches.end(),
                                                    [id](const TouchInput& t2) { return t2.id == id && !t2.pressed; }),
                                     input_.touches.end());
            }
            return;
        }
    }
    if (pressed) input_.touches.push_back({id, x, y, pressed});
}

void NovaEngine::clearTouches() { input_.touches.clear(); }

void NovaEngine::setKey(const std::string& key, bool down) {
    input_.keys[key] = down;
}

std::unique_ptr<NovaEngine> createEngine() {
    return std::make_unique<NovaEngine>();
}

} // namespace nova
