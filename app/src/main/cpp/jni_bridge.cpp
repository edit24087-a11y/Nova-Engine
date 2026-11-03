// Nova Engine — JNI bridge.
// Exposes the C++ engine core to the Kotlin app. Node identity crosses the
// boundary as NodeId (uint32); strings cross as std::string via GetStringUTFChars.
//
// Threading: engine calls come from the GL render thread and background load
// threads; the bridge serializes through a mutex.

#include <jni.h>
#include <android/log.h>
#include <mutex>
#include <vector>
#include <string>
#include <memory>

#include "nova/engine/Engine.hpp"
#include "nova/scene/SceneManager.hpp"
#include "nova/scene/Node.hpp"
#include "nova/script/Script.hpp"
#include "nova/script/novascript.hpp"
#include "nova/json.hpp"
#include "nova/util/fs.hpp"

#define LOG_TAG "NovaEngineJNI"
#define NLOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define NLOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using namespace nova;

static std::mutex g_mutex;
static NovaEngine* g_engine = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::shared_ptr<Node> findNodeById(NodeId id) {
    if (!g_engine) return nullptr;
    auto scene = g_engine->currentScene();
    if (!scene || !scene->root) return nullptr;
    return scene->root->findById(id);
}

static std::string jstr(JNIEnv* env, jstring s) {
    if (!s) return "";
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string out = c ? c : "";
    if (c) env->ReleaseStringUTFChars(s, c);
    return out;
}

// Collect the whole tree below a node into (id, name, type, depth) rows.
static void collectTree(std::shared_ptr<Node> n, int depth,
                        std::vector<std::vector<std::string>>& rows) {
    if (!n) return;
    std::vector<std::string> row;
    row.push_back(std::to_string(n->id));
    row.push_back(n->name);
    row.push_back(n->type);
    row.push_back(std::to_string(depth));
    rows.push_back(row);
    for (auto& c : n->children) collectTree(c, depth + 1, rows);
}

// ---------------------------------------------------------------------------
// Engine lifecycle
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT jboolean JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeCreate(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_engine) return JNI_TRUE;
    g_engine = new NovaEngine();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeInitialize(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return JNI_FALSE;
    std::string err;
    bool ok = g_engine->initialize(&err);
    if (!ok) NLOGE("initialize failed: %s", err.c_str());
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeShutdown(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_engine) {
        g_engine->shutdown();
        delete g_engine;
        g_engine = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Project management
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT jboolean JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeOpenProject(JNIEnv* env, jobject,
                                                                  jstring jpath) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return JNI_FALSE;
    std::string err, path = jstr(env, jpath);
    bool ok = g_engine->openProject(path, &err);
    if (!ok) NLOGE("openProject failed: %s", err.c_str());
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeSaveProject(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return JNI_FALSE;
    std::string err;
    bool ok = g_engine->saveProject(&err);
    if (!ok) NLOGE("saveProject failed: %s", err.c_str());
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeProjectPath(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return env->NewStringUTF("");
    return env->NewStringUTF(g_engine->projectPath().c_str());
}

// Create a new project directory tree (scenes/, scripts/, assets/, ...).
extern "C" JNIEXPORT jboolean JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeCreateProject(JNIEnv* env, jobject,
                                                                      jstring jpath, jstring jname) {
    std::lock_guard<std::mutex> lk(g_mutex);
    std::string path = jstr(env, jpath), name = jstr(env, jname);
    std::string full = path.empty() ? name : path + "/" + name;
    std::vector<std::string> subdirs = {
        "scenes", "scripts", "assets", "textures", "models", "audio",
        "fonts", "materials", "animations", "prefabs", "plugins",
        "settings", "builds"
    };
    for (auto& d : subdirs) fs::makeDirectories(full + "/" + d);
    json::JsonValue doc = json::JsonValue::object();
    doc.set("name", json::JsonValue(name));
    doc.set("engine", json::JsonValue("NovaEngine"));
    doc.set("version", json::JsonValue("1.0"));
    doc.set("activeScene", json::JsonValue("Main"));
    json::JsonValue scenes = json::JsonValue::array();
    json::JsonValue sc = json::JsonValue::object();
    sc.set("name", json::JsonValue("Main"));
    sc.set("path", json::JsonValue("scenes/Main.novascene"));
    sc.set("script", json::JsonValue(""));
    scenes.push(sc);
    doc.set("scenes", scenes);
    if (!fs::writeFile(full + "/project.json", doc.dump(2))) {
        NLOGE("createProject: cannot write project.json");
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// Scene management
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT jboolean JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeNewScene(JNIEnv* env, jobject, jstring jname) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return JNI_FALSE;
    std::string name = jstr(env, jname), err;
    return g_engine->newScene(name, &err) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeOpenScene(JNIEnv* env, jobject, jstring jname) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return JNI_FALSE;
    std::string name = jstr(env, jname), err;
    bool ok = g_engine->openScene(name, &err);
    if (!ok) NLOGE("openScene failed: %s", err.c_str());
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeSaveScene(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return JNI_FALSE;
    std::string err;
    bool ok = g_engine->saveScene(&err);
    if (!ok) NLOGE("saveScene failed: %s", err.c_str());
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeSceneName(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return env->NewStringUTF("");
    return env->NewStringUTF(g_engine->sceneName().c_str());
}

// ---------------------------------------------------------------------------
// Node operations
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT jint JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeCreateNode(JNIEnv* env, jobject,
                                                                  jstring jtype, jstring jname,
                                                                  jint parentId) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return 0;
    std::string type = jstr(env, jtype), name = jstr(env, jname);
    std::shared_ptr<Node> parent;
    if (parentId != 0) parent = findNodeById((NodeId)parentId);
    if (!parent && !g_engine->currentScene()) return 0;
    auto node = g_engine->createNode(type, name, parent);
    return node ? (jint)node->id : 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeDeleteNode(JNIEnv*, jobject, jint nodeId) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return JNI_FALSE;
    auto n = findNodeById((NodeId)nodeId);
    return n ? g_engine->deleteNode(n.get()) : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeRenameNode(JNIEnv* env, jobject,
                                                                  jint nodeId, jstring jname) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return JNI_FALSE;
    auto n = findNodeById((NodeId)nodeId);
    return n ? g_engine->renameNode(n.get(), jstr(env, jname)) : JNI_FALSE;
}

// Serialize the scene tree into rows: [id, name, type, depth] per line.
extern "C" JNIEXPORT jstring JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeDumpTree(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lk(g_mutex);
    std::vector<std::vector<std::string>> rows;
    if (g_engine && g_engine->currentScene() && g_engine->currentScene()->root)
        collectTree(g_engine->currentScene()->root, 0, rows);
    std::string out;
    for (auto& r : rows) {
        out += r[0] + "\t" + r[1] + "\t" + r[2] + "\t" + r[3] + "\n";
    }
    return env->NewStringUTF(out.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeMoveNode(JNIEnv*, jobject,
                                                                jint nodeId, jint parentId) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return JNI_FALSE;
    auto n = findNodeById((NodeId)nodeId);
    auto parent = findNodeById((NodeId)parentId);
    if (!n || !parent) return JNI_FALSE;
    n->removeSelf();
    return parent->addChild(n) ? JNI_TRUE : JNI_FALSE;
}

// ---------------------------------------------------------------------------
// Inspector properties (id/name/type/pos/rot/scale + string props map)
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT jstring JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeGetNodeInfo(JNIEnv* env, jobject, jint nodeId) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto n = findNodeById((NodeId)nodeId);
    if (!n) return env->NewStringUTF("");
    std::string s = std::to_string(n->id) + "\t" + n->name + "\t" + n->type + "\t"
        + n->props.get("color", "") + "\t"
        + Properties::fmtVec2(n->position2d()) + "\t"
        + std::to_string(n->rotationDeg2d()) + "\t"
        + Properties::fmtVec2(n->scale2d());
    return env->NewStringUTF(s.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeSetNodeTransform(JNIEnv* env, jobject,
                                                                        jint nodeId,
                                                                        jfloat x, jfloat y,
                                                                        jfloat rot,
                                                                        jfloat sx, jfloat sy) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto n = findNodeById((NodeId)nodeId);
    if (!n) return;
    n->setPosition2d(Vec2(x, y));
    n->setRotationDeg2d(rot);
    n->scale3.x = sx;
    n->scale3.y = sy;
}

extern "C" JNIEXPORT void JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeSetNodeProp(JNIEnv* env, jobject,
                                                                   jint nodeId, jstring jkey,
                                                                   jstring jval) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto n = findNodeById((NodeId)nodeId);
    if (!n) return;
    n->props.set(jstr(env, jkey), jstr(env, jval));
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeGetNodeProp(JNIEnv* env, jobject,
                                                                  jint nodeId, jstring jkey) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto n = findNodeById((NodeId)nodeId);
    if (!n) return env->NewStringUTF("");
    return env->NewStringUTF(n->props.get(jstr(env, jkey)).c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeAttachScript(JNIEnv* env, jobject,
                                                                  jint nodeId, jstring jsource) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto n = findNodeById((NodeId)nodeId);
    if (!n) return JNI_FALSE;
    std::string serr;
    auto script = Script::createFromSource(n->type, n->name, jstr(env, jsource), &serr);
    if (!script) return JNI_FALSE;
    n->script = script;
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// Console
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT jstring JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeConsoleDump(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return env->NewStringUTF("");
    std::string out;
    for (auto& l : g_engine->consoleLog()) out += l + "\n";
    return env->NewStringUTF(out.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeConsoleClear(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_engine) g_engine->clearConsole();
}

// ---------------------------------------------------------------------------
// Input + update + render (called from the GL thread)
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeTouch(JNIEnv*, jobject, jint id,
                                                            jfloat x, jfloat y, jboolean down) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_engine) g_engine->setTouch(id, x, y, down ? true : false);
}

extern "C" JNIEXPORT void JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeResize(JNIEnv*, jobject, jint w, jint h) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_engine) g_engine->resize(w, h);
}

extern "C" JNIEXPORT void JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeUpdate(JNIEnv*, jobject, jfloat dt) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_engine) g_engine->update(dt);
}

extern "C" JNIEXPORT void JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeRender(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_engine) g_engine->renderFrame(Vec4(0.09f, 0.09f, 0.12f, 1.0f));
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeStats(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_engine) return 0;
    EngineStats st = g_engine->stats();
    // Pack 64-bit: fps | physicsBodies << 16 | drawCalls << 32 | scriptErrors << 48
    return (jlong)(((int64_t)(int)st.fps & 0xFFFF)
        | ((int64_t)st.physicsBodies & 0xFFFF) << 16
        | ((int64_t)st.totalDrawCalls & 0xFFFF) << 32
        | ((int64_t)st.scriptErrors & 0xFFFF) << 48);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_novaengine_app_engine_NovaEngineBridge_nativeVersion(JNIEnv* env, jobject) {
    return env->NewStringUTF("NovaEngine 1.0.0 (NovaScript VM, native core)");
}
