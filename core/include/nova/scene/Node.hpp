// Nova Engine — core node system.
// Every scene entity is a Node with a unique id, name, type, property map,
// optional script, and a parent/child hierarchy. Node3D transform is stored
// as position/rotationDeg/scale; Node2D uses 2D variants.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include <memory>
#include <sstream>
#include "nova/math.hpp"

namespace nova {

class Node;
class Scene;
class Script;

using NodeId = uint32_t;
constexpr NodeId INVALID_NODE_ID = 0;

// Lightweight property bag used by the Inspector and serialization.
class Properties {
public:
    std::map<std::string, std::string> map;

    std::string get(const std::string& key, const std::string& def = "") const {
        auto it = map.find(key);
        return it != map.end() ? it->second : def;
    }
    double getNum(const std::string& key, double def = 0.0) const {
        auto it = map.find(key);
        if (it == map.end()) return def;
        try { return std::stod(it->second); } catch (...) { return def; }
    }
    Vec2 getVec2(const std::string& key, Vec2 def = Vec2()) const {
        auto it = map.find(key);
        if (it == map.end()) return def;
        try {
            size_t comma = it->second.find(',');
            if (comma == std::string::npos) return def;
            return Vec2((float)std::stod(it->second.substr(0, comma)),
                        (float)std::stod(it->second.substr(comma + 1)));
        } catch (...) { return def; }
    }
    Vec3 getVec3(const std::string& key, Vec3 def = Vec3()) const {
        auto it = map.find(key);
        if (it == map.end()) return def;
        try {
            size_t c1 = it->second.find(',');
            size_t c2 = it->second.find(',', c1 + 1);
            if (c1 == std::string::npos || c2 == std::string::npos) return def;
            return Vec3((float)std::stod(it->second.substr(0, c1)),
                        (float)std::stod(it->second.substr(c1 + 1, c2 - c1 - 1)),
                        (float)std::stod(it->second.substr(c2 + 1)));
        } catch (...) { return def; }
    }
    void set(const std::string& key, const std::string& v) { map[key] = v; }
    void setNum(const std::string& key, double v) { map[key] = formatDouble(v); }
    static std::string formatDouble(double v) {
        std::ostringstream os;
        os.precision(9);
        os << v;
        return os.str();
    }
    void setVec2(const std::string& key, Vec2 v) { map[key] = fmtVec2(v); }
    void setVec3(const std::string& key, Vec3 v) { map[key] = fmtVec3(v); }
    bool has(const std::string& key) const { return map.count(key) > 0; }
    void remove(const std::string& key) { map.erase(key); }

    static std::string fmtVec2(Vec2 v) { return std::to_string(v.x) + "," + std::to_string(v.y); }
    static std::string fmtVec3(Vec3 v) {
        return std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z);
    }
};

class Node : public std::enable_shared_from_this<Node> {
public:
    NodeId id;
    std::string name;
    std::string type; // "Node", "Node2D", "Node3D", "Sprite2D", "RigidBody2D", ...

    Node(NodeId id_, const std::string& name_, const std::string& type_)
        : id(id_), name(name_), type(type_) {}
    virtual ~Node() = default;
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    // Hierarchy.
    Node* parent = nullptr;
    std::vector<std::shared_ptr<Node>> children;

    std::shared_ptr<Node> addChild(std::shared_ptr<Node> child);
    std::shared_ptr<Node> removeChild(Node* child);
    bool removeSelf();
    void detachAllChildren();
    // Re-points every direct child's parent to this node (used after re-insert).
    void reparentChildren();
    std::vector<std::shared_ptr<Node>> descendants();
    std::shared_ptr<Node> findChildByName(const std::string& name) const;
    std::shared_ptr<Node> findById(NodeId id) {
        if (this->id == id) return shared_from_this();
        for (auto& c : children) {
            auto r = c->findById(id);
            if (r) return r;
        }
        return nullptr;
    }
    std::vector<std::shared_ptr<Node>> ancestors();

    // 3D transform (also used for 2D as a convenience).
    Vec3 position;
    Vec3 rotationDeg; // Euler degrees
    Vec3 scale3 = Vec3(1, 1, 1);
    Mat4 localMatrix() const {
        return Mat4::translation(position) *
               Quat::fromEuler(rotationDeg).toMat4() *
               Mat4::scale(scale3);
    }
    Mat4 worldMatrix() const;

    // 2D convenience.
    Vec2 position2d() const { return Vec2(position.x, position.y); }
    void setPosition2d(Vec2 p) { position.x = p.x; position.y = p.y; }
    float rotationDeg2d() const { return rotationDeg.z; }
    void setRotationDeg2d(float deg) { rotationDeg.z = deg; }
    Vec2 scale2d() const { return Vec2(scale3.x, scale3.y); }

    // Properties (Inspector backing store).
    Properties props;

    // Scripting.
    std::shared_ptr<Script> script;

    // Signals (simple observer callbacks).
    std::map<std::string, std::vector<std::function<void()>>> signals;
    void emit(const std::string& signalName);
    void connect(const std::string& signalName, std::function<void()> cb);

    // Runtime.
    bool enabled = true;
    bool visible = true;
    bool editorOnly = false;

    // Serialization hooks.
    virtual std::string toJson() const;
    static std::shared_ptr<Node> fromJson(const std::string& jsonText, NodeId rootId,
                                          Node* parent, std::string* error);
    // Create a node of the given type ("Node", "Node2D", "Sprite2D", ...).
    static std::shared_ptr<Node> createNode(const std::string& type, const std::string& name,
                                            NodeId id, std::string* error);

    // Built-in static-body flag used by physics nodes.
    virtual bool isPhysicsBody() const { return false; }

protected:
    virtual void onAddedToTree() {}
};

} // namespace nova
