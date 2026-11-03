// Nova Engine — core test suite (headless, no external framework).
// Build: see core/CMakeLists.txt or scripts/build_core.sh
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <unistd.h>

#include "nova/math.hpp"
#include "nova/json.hpp"
#include "nova/util/fs.hpp"
#include "nova/scene/Node.hpp"
#include "nova/scene/SceneManager.hpp"
#include "nova/physics2d/World2D.hpp"
#include "nova/physics3d/RigidBody3D.hpp"
#include "nova/script/novascript.hpp"
#include "nova/script/Script.hpp"
#include "nova/render/RenderDevice.hpp"
#include "nova/engine/Engine.hpp"
#include "nova/audio/AudioManager.hpp"

using namespace nova;

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------
static int g_pass = 0;
static int g_fail = 0;
static std::vector<std::string> g_failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (cond) {                                                            \
            ++g_pass;                                                          \
        } else {                                                               \
            ++g_fail;                                                          \
            g_failures.push_back(__FILE__ + std::string(":") +                \
                                 std::to_string(__LINE__) + ": " + #cond);   \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
        }                                                                      \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                   \
    do {                                                                       \
        double _a = (double)(a), _b = (double)(b);                             \
        if (std::fabs(_a - _b) < (eps)) {                                       \
            ++g_pass;                                                          \
        } else {                                                               \
            ++g_fail;                                                          \
            g_failures.push_back(__FILE__ + std::string(":") +                \
                                 std::to_string(__LINE__) + ": " + #a " ~= " #b); \
            std::printf("  FAIL %s:%d: %s ~= %s (got %g vs %g)\n", __FILE__,  \
                        __LINE__, #a, #b, _a, _b);                            \
        }                                                                      \
    } while (0)

static void section(const char* name) {
    std::printf("== %s ==\n", name);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
static void testMath() {
    section("math");
    Vec2 v(3, 4);
    CHECK_NEAR(v.length(), 5.0, 1e-6);
    CHECK(v.normalized() == Vec2(0.6f, 0.8f));
    Vec3 cross = Vec3(1, 0, 0).cross(Vec3(0, 1, 0));
    CHECK(cross == Vec3(0, 0, 1));
    Mat4 id = Mat4::identity();
    Vec3 p(1, 2, 3);
    CHECK(id.transformPoint(p) == p);
    Mat4 t = Mat4::translation(Vec3(10, 20, 30));
    Vec3 tp = t.transformPoint(p);
    CHECK_NEAR(tp.x, 11.0, 1e-6);
    CHECK_NEAR(tp.y, 22.0, 1e-6);
    CHECK_NEAR(tp.z, 33.0, 1e-6);
    Quat q = Quat::fromEuler(Vec3(0, 90, 0));
    Vec3 rotated = q.rotate(Vec3(1, 0, 0));
    CHECK_NEAR(rotated.x, 0.0, 1e-4);
    CHECK_NEAR(rotated.z, -1.0, 1e-4);
    Mat4 persp = Mat4::perspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    CHECK_NEAR(persp.m[5], 1.0 / std::tan(deg2rad(30.0f)), 1e-4);
}

static void testJson() {
    section("json");
    json::JsonValue obj(json::JsonValue::object());
    obj.set("name", "Nova");
    obj.set("version", 1.0);
    obj.set("enabled", true);
    json::JsonValue arr(json::JsonValue::array());
    arr.push(json::JsonValue(1));
    arr.push(json::JsonValue(2));
    arr.push(json::JsonValue(3));
    obj.set("scenes", arr);
    std::string text = obj.dump();
    json::JsonValue back = json::JsonValue::parse(text);
    CHECK(back.get("name").asString() == "Nova");
    CHECK_NEAR(back.get("version").asNumber(), 1.0, 1e-9);
    CHECK(back.get("enabled").asBool());
    CHECK(back.get("scenes").asArray().size() == 3);
    // Escaping.
    json::JsonValue s("hello\n\"world\"");
    std::string escaped = s.dump();
    json::JsonValue s2 = json::JsonValue::parse(escaped);
    CHECK(s2.asString() == "hello\n\"world\"");
    // Invalid input.
    bool threw = false;
    try {
        json::JsonValue::parse("{broken");
    } catch (...) {
        threw = true;
    }
    CHECK(threw);
    // Nested objects.
    json::JsonValue nested = json::JsonValue::parse(
        "{\"a\":{\"b\":{\"c\":42}},\"list\":[1,2,3]}");
    CHECK_NEAR(nested.get("a").get("b").get("c").asNumber(), 42.0, 1e-9);
    CHECK(nested.get("list").asArray()[1].asInt() == 2);
}

static void testFs(const std::string& tmpDir) {
    section("filesystem");
    std::string dir = fs::join(tmpDir, "fs_test_" + std::to_string(getpid()));
    CHECK(fs::makeDirectories(fs::join(dir, "a/b/c")));
    CHECK(fs::isDirectory(fs::join(dir, "a/b")));
    std::string file = fs::join(dir, "hello.txt");
    CHECK(fs::writeFile(file, "hello nova"));
    std::string content;
    CHECK(fs::readFile(file, content));
    CHECK(content == "hello nova");
    CHECK(fs::extension(file) == "txt");
    CHECK(fs::stem(file) == "hello");
    CHECK(fs::baseName(file) == "hello.txt");
    CHECK(fs::dirName(file) == fs::join(dir, ""));
    std::vector<std::string> entries = fs::listDirectory(dir);
    CHECK(entries.size() >= 1);
    CHECK(fs::removeDirectory(dir));
    CHECK(!fs::exists(dir));
}

static void testNodeTree() {
    section("node tree");
    auto root = Node::createNode("Node3D", "Root", 1, nullptr);
    CHECK(root != nullptr);
    CHECK(root->id == 1);
    auto player = Node::createNode("CharacterBody2D", "Player", 2, nullptr);
    root->addChild(player);
    CHECK(player->parent == root.get());
    CHECK(root->children.size() == 1);
    auto cam = Node::createNode("Camera2D", "Camera", 3, nullptr);
    player->addChild(cam);
    CHECK(cam->parent == player.get());
    CHECK(root->findById(3) == cam);
    auto found = root->findById(2);
    CHECK(found == player);
    CHECK(root->descendants().size() == 2);
    // World matrix.
    player->position = Vec3(100, 200, 0);
    Mat4 wm = player->worldMatrix();
    Vec3 p = wm.transformPoint(Vec3::zero());
    CHECK_NEAR(p.x, 100.0, 1e-4);
    CHECK_NEAR(p.y, 200.0, 1e-4);
    // Remove.
    CHECK(player->removeSelf());
    CHECK(root->children.size() == 0);
    CHECK(cam->parent == nullptr);
}

static void testSceneSerialization() {
    section("scene serialization");
    auto root = Node::createNode("Node3D", "Root", 1, nullptr);
    auto player = Node::createNode("CharacterBody2D", "Player", 2, nullptr);
    player->position = Vec3(5, 6, 7);
    player->props.setNum("speed", 120.0);
    root->addChild(player);
    auto health = Node::createNode("HealthBar", "HealthBar", 3, nullptr);
    health->props.set("value", "75");
    player->addChild(health);

    Scene scene;
    scene.root = root;
    scene.name = "Main";
    std::string text = scene.toJson();

    Scene scene2;
    std::string err;
    CHECK(scene2.loadJson(text, &err));
    CHECK(err.empty());
    CHECK(scene2.root != nullptr);
    CHECK(scene2.root->name == "Root");
    auto p2 = scene2.root->findChildByName("Player");
    CHECK(p2 != nullptr);
    CHECK_NEAR(p2->position.x, 5.0, 1e-4);
    CHECK_NEAR(p2->props.getNum("speed"), 120.0, 1e-4);
    auto hb = p2->findChildByName("HealthBar");
    CHECK(hb != nullptr);
    CHECK_NEAR(hb->props.getNum("value"), 75.0, 1e-9);
}

static void testUndoRedo() {
    section("undo/redo");
    SceneManager sm;
    sm.createScene("S");
    auto root = sm.current->root;
    auto n1 = sm.addNode("Sprite2D", "Spr1", root);
    auto n2 = sm.addNode("Sprite2D", "Spr2", root);
    CHECK(root->children.size() == 2);
    CHECK(sm.commands.size() >= 1);
    sm.commands.clear();

    // Delete + undo restores.
    CHECK(sm.deleteNode(n1.get()));
    CHECK(root->children.size() == 1);
    sm.commands.undo();
    CHECK(root->children.size() == 2);
    CHECK(root->findChildByName("Spr1") != nullptr);
    sm.commands.redo();
    CHECK(root->children.size() == 1);

    // Rename + undo.
    CHECK(sm.renameNode(n2.get(), "Spr2B"));
    CHECK(n2->name == "Spr2B");
    sm.commands.undo();
    CHECK(n2->name == "Spr2");

    // Move + undo.
    CHECK(sm.moveNodeToParent(n2.get(), n1, 0));
    CHECK(n1->children.size() == 1);
    CHECK(root->children.size() == 0); // n2 moved out of root
    sm.commands.undo();
    CHECK(n2->parent == root.get());
    CHECK(n1->children.size() == 0);
}

static void testPhysics2D() {
    section("physics 2d");
    World2D world;
    world.gravity = Vec2(0, 1000);

    Body2D ground;
    ground.name = "Ground";
    ground.type = BodyType::Static;
    ground.position = Vec2(0, 300);
    ground.shape = CollisionShape{CollisionShapeKind::Box, 0, Vec2(800, 60), false};
    world.addBody(&ground);

    Body2D ball;
    ball.name = "Ball";
    ball.type = BodyType::RigidBody;
    ball.position = Vec2(0, 0);
    ball.velocity = Vec2(0, 0);
    ball.mass = 1.0f;
    ball.shape = CollisionShape{CollisionShapeKind::Circle, 20, Vec2(), false};
    world.addBody(&ball);

    // Simulate 2 seconds.
    for (int i = 0; i < 120; ++i) world.step(1.0f / 60.0f);
    // Ball should be resting on the ground (y ~ 280 center for radius 20).
    CHECK(ball.position.y > 250.0f);
    CHECK(ball.position.y < 290.0f);
    CHECK_NEAR(ball.velocity.y, 0.0, 5.0);

    // Horizontal push resolves against static wall.
    Body2D wall;
    wall.type = BodyType::Static;
    wall.position = Vec2(300, 0);
    wall.shape = CollisionShape{CollisionShapeKind::Box, 0, Vec2(40, 1000), false};
    world.addBody(&wall);
    ball.position = Vec2(100, 100);
    ball.velocity = Vec2(500, 0);
    for (int i = 0; i < 60; ++i) world.step(1.0f / 60.0f);
    CHECK(ball.position.x < 260.0f); // stopped by wall around x ~ 260 (20 radius + wall at 280)

    // Raycast (from x=400 to avoid the ball at x=0).
    Body2D* hitBody = world.rayCast(Vec2(400, 0), Vec2(0, 1), 1000.0f, nullptr);
    CHECK(hitBody == &ground);

    // Queries.
    std::vector<Body2D*> inRange = world.queryCircle(Vec2(0, 300), 100);
    CHECK(inRange.size() >= 1);

    // Static bodies don't move.
    Vec2 groundBefore = ground.position;
    world.step(0.016f);
    CHECK(ground.position == groundBefore);
}

static void testPhysics3D() {
    section("physics 3d");
    World3D world;
    RigidBody3D box;
    box.position = Vec3(0, 10, 0);
    world.addBody(&box);
    RigidBody3D floor;
    floor.type = Body3DType::Static;
    floor.position = Vec3(0, 0, 0);
    floor.halfExtents = Vec3(50, 1, 50);
    world.addBody(&floor);
    for (int i = 0; i < 120; ++i) world.step(1.0f / 60.0f);
    CHECK(box.position.y > 0.0f);
    CHECK(box.position.y < 5.0f);
}

static void testScripting() {
    section("novascript");
    NovaScriptVM& vm = NovaScriptVM::instance();

    // Arithmetic + globals.
    std::string err;
    CHECK(vm.compile("var x = 21\nvar y = x * 2\n", &err));
    CHECK(vm.run(nullptr, &err));
    CHECK_NEAR(vm.getGlobal("y").num, 42.0, 1e-9);

    // if/else, loops, functions.
    CHECK(vm.compile(
        "func fact(n):\n"
        "    if n <= 1:\n"
        "        return 1\n"
        "    return n * fact(n - 1)\n"
        "var r = fact(5)\n", &err));
    CHECK(vm.run(nullptr, &err));
    CHECK_NEAR(vm.getGlobal("r").num, 120.0, 1e-9);

    // while + break/continue + string concat.
    CHECK(vm.compile(
        "var i = 0\nvar total = 0\n"
        "while true:\n"
        "    i = i + 1\n"
        "    if i > 5:\n"
        "        break\n"
        "    if i % 2 == 0:\n"
        "        continue\n"
        "    total = total + i\n"
        "print(to_string(total))\n", &err));
    CHECK(vm.run(nullptr, &err));
    CHECK_NEAR(vm.getGlobal("total").num, 9.0, 1e-9);

    // Property access on nodes via a fake host.
    struct TestHost : public NovaHost {
        Node root{1, "Root", "Node3D"};
        Node* findNode(const std::string&) override { return &root; }
        void log(const std::string&) override {}
        NsValue getProp(Node*, const std::string& key) override {
            if (key == "hp") return NsValue::number(100.0);
            return NsValue::nil();
        }
        void setProp(Node*, const std::string& key, const NsValue& v) override {
            if (key == "hp") stored = v.num;
        }
        double stored = -1;
    } host;

    CHECK(vm.compile(
        "self.hp = 50\n"
        "var hp = get_prop(self, \"hp\")\n", &err));
    // The engine sets `self` before running node scripts.
    vm.setGlobal("self", NsValue::nodeRef(&host.root));
    CHECK(vm.run(&host, &err));
    CHECK_NEAR(host.stored, 50.0, 1e-9);
    CHECK_NEAR(vm.getGlobal("hp").num, 100.0, 1e-9);

    // Syntax errors surface.
    std::string bad = "var x = \n  + \n";
    CHECK(!vm.compile(bad, &err));
    CHECK(!err.empty());
}

static void testEngineLoop() {
    section("engine loop");
    auto engine = createEngine();
    std::string err;
    CHECK(engine->initialize(&err));
    CHECK(err.empty());

    // Create a scene with a sprite + a script.
    auto scene = engine->currentScene();
    auto root = scene->root;
    auto sprite = engine->createNode("Sprite2D", "Hero", root);
    sprite->props.set("texture", "hero.png");
    sprite->props.setVec2("size", Vec2(64, 64));
    sprite->setPosition2d(Vec2(10, 20));
    std::string scriptErr;
    auto script = Script::createFromSource("Sprite2D", "Hero",
        "func on_ready():\n    self.x = self.x + 5\n", &scriptErr);
    CHECK(script != nullptr);
    sprite->script = script;

    // Physics body via props-driven body.
    Body2D body;
    body.type = BodyType::RigidBody;
    body.position = Vec2(0, 0);
    body.mass = 1.0f;
    body.shape = CollisionShape{CollisionShapeKind::Circle, 10, Vec2(), false};
    engine->physicsWorld().addBody(&body);

    engine->resize(1280, 720);
    engine->update(1.0f / 60.0f);
    CHECK_NEAR(sprite->position.x, 15.0, 1e-4); // script moved it
    engine->renderFrame(Vec4(0.1f, 0.1f, 0.12f, 1.0f));
    CHECK(engine->stats().totalDrawCalls >= 1);
    CHECK(engine->consoleLog().size() >= 1);
    CHECK(engine->stats().fps >= 0.0f); // no NaN
}

static void testProjectFlow(const std::string& tmpDir) {
    section("project flow");
    std::string projDir = fs::join(tmpDir, "proj_" + std::to_string(getpid()));
    CHECK(fs::makeDirectories(projDir));

    // Create project.json + scene, then open through the engine.
    json::JsonValue meta(json::JsonValue::object());
    meta.set("name", "TestGame");
    meta.set("type", "2d");
    meta.set("version", 1);
    json::JsonValue scenes(json::JsonValue::array());
    scenes.push(json::JsonValue("Main"));
    meta.set("scenes", scenes);
    CHECK(fs::writeFile(fs::join(projDir, "project.json"), meta.dump(2)));

    // Save a scene file.
    Scene scene;
    scene.name = "Main";
    scene.root = Node::createNode("Node3D", "Root", 1, nullptr);
    scene.root->addChild(Node::createNode("Sprite2D", "Hero", 2, nullptr));
    CHECK(fs::makeDirectories(fs::join(projDir, "scenes")));
    CHECK(scene.saveToFile(fs::join(projDir, "scenes/Main.scene.json"), nullptr));

    auto engine = createEngine();
    std::string err;
    CHECK(engine->openProject(projDir, &err));
    CHECK(err.empty());
    CHECK(engine->currentScene() != nullptr);
    CHECK(engine->currentScene()->name == "Main");
    CHECK(engine->currentScene()->root->findChildByName("Hero") != nullptr);

    // Edit + save roundtrip.
    auto hero = engine->currentScene()->root->findChildByName("Hero");
    engine->renameNode(hero.get(), "Player");
    CHECK(engine->saveProject(&err));
    CHECK(err.empty());

    // Reopen and verify.
    auto engine2 = createEngine();
    CHECK(engine2->openProject(projDir, &err));
    CHECK(engine2->currentScene()->root->findChildByName("Player") != nullptr);

    fs::removeDirectory(projDir);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    std::printf("Nova Engine core test suite\n\n");
    testMath();
    testJson();
    testFs("/tmp");
    testNodeTree();
    testSceneSerialization();
    testUndoRedo();
    testPhysics2D();
    testPhysics3D();
    testScripting();
    testEngineLoop();
    testProjectFlow("/tmp");

    std::printf("\n----------------------------------------\n");
    std::printf("PASSED: %d   FAILED: %d\n", g_pass, g_fail);
    if (!g_failures.empty()) {
        std::printf("\nFailures:\n");
        for (const auto& f : g_failures) std::printf("  - %s\n", f.c_str());
    }
    return g_fail == 0 ? 0 : 1;
}
