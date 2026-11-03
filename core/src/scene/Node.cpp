// Nova Engine — core node implementation.
#include "nova/scene/Node.hpp"
#include "nova/json.hpp"
#include "nova/script/Script.hpp"
#include <sstream>

namespace nova {

std::shared_ptr<Node> Node::addChild(std::shared_ptr<Node> child) {
    if (!child) return nullptr;
    if (child->parent == this) return child;
    if (child->parent) child->parent->removeChild(child.get());
    child->parent = this;
    children.push_back(child);
    child->onAddedToTree();
    return child;
}

std::shared_ptr<Node> Node::removeChild(Node* child) {
    if (!child) return nullptr;
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (it->get() == child) {
            std::shared_ptr<Node> removed = *it;
            // Orphan the removed node's children (keep their ownership) so they
            // never keep a stale parent link into a detached subtree.
            for (auto& c : removed->children) c->parent = nullptr;
            removed->parent = nullptr;
            children.erase(it);
            return removed;
        }
    }
    return nullptr;
}

bool Node::removeSelf() {
    if (!parent) return false;
    return parent->removeChild(this) != nullptr;
}

void Node::detachAllChildren() {
    for (auto& c : children) c->parent = nullptr;
    children.clear();
}

void Node::reparentChildren() {
    for (auto& c : children) c->parent = this;
}

std::vector<std::shared_ptr<Node>> Node::descendants() {
    std::vector<std::shared_ptr<Node>> out;
    for (auto& c : children) {
        out.push_back(c);
        auto d = c->descendants();
        out.insert(out.end(), d.begin(), d.end());
    }
    return out;
}

std::shared_ptr<Node> Node::findChildByName(const std::string& name) const {
    for (auto& c : children) {
        if (c->name == name) return c;
    }
    return nullptr;
}

std::vector<std::shared_ptr<Node>> Node::ancestors() {
    std::vector<std::shared_ptr<Node>> out;
    Node* p = parent;
    while (p) {
        out.push_back(p->shared_from_this());
        p = p->parent;
    }
    return out;
}

Mat4 Node::worldMatrix() const {
    Mat4 local = localMatrix();
    if (!parent) return local;
    return parent->worldMatrix() * local;
}

void Node::emit(const std::string& signalName) {
    auto it = signals.find(signalName);
    if (it == signals.end()) return;
    for (auto& cb : it->second) cb();
}

void Node::connect(const std::string& signalName, std::function<void()> cb) {
    signals[signalName].push_back(std::move(cb));
}

static Vec3 parseVec3Or(const nova::json::JsonValue& v, Vec3 def) {
    if (v.isArray() && v.asArray().size() >= 3) {
        return Vec3(v.asArray()[0].asFloat(), v.asArray()[1].asFloat(), v.asArray()[2].asFloat());
    }
    return def;
}

std::string Node::toJson() const {
    nova::json::JsonValue root(nova::json::JsonValue::object());
    root.set("id", (double)id);
    root.set("name", name);
    root.set("type", type);
    root.set("enabled", enabled);
    root.set("visible", visible);

    nova::json::JsonValue pos(nova::json::JsonValue::array());
    pos.push(nova::json::JsonValue(position.x));
    pos.push(nova::json::JsonValue(position.y));
    pos.push(nova::json::JsonValue(position.z));
    root.set("position", pos);

    nova::json::JsonValue rot(nova::json::JsonValue::array());
    rot.push(nova::json::JsonValue(rotationDeg.x));
    rot.push(nova::json::JsonValue(rotationDeg.y));
    rot.push(nova::json::JsonValue(rotationDeg.z));
    root.set("rotationDeg", rot);

    nova::json::JsonValue sc(nova::json::JsonValue::array());
    sc.push(nova::json::JsonValue(scale3.x));
    sc.push(nova::json::JsonValue(scale3.y));
    sc.push(nova::json::JsonValue(scale3.z));
    root.set("scale", sc);

    nova::json::JsonValue propsObj(nova::json::JsonValue::object());
    for (const auto& kv : this->props.map) {
        // Try numeric first for compactness.
        try {
            propsObj.set(kv.first, nova::json::JsonValue(std::stod(kv.second)));
        } catch (...) {
            propsObj.set(kv.first, nova::json::JsonValue(kv.second));
        }
    }
    root.set("props", propsObj);

    nova::json::JsonValue scriptObj(nova::json::JsonValue::object());
    if (script) scriptObj.set("source", script->source());
    root.set("script", scriptObj);

    nova::json::JsonValue kids(nova::json::JsonValue::array());
    for (auto& c : children) kids.push(nova::json::JsonValue::parse(c->toJson()));
    root.set("children", kids);

    return root.dump();
}

std::shared_ptr<Node> Node::fromJson(const std::string& jsonText, NodeId rootId,
                                     Node* parent, std::string* error) {
    nova::json::JsonValue root;
    try {
        root = nova::json::JsonValue::parse(jsonText);
    } catch (const std::exception& e) {
        if (error) *error = std::string("parse error: ") + e.what();
        return nullptr;
    }
    if (!root.isObject()) {
        if (error) *error = "node json must be an object";
        return nullptr;
    }
    NodeId id = (NodeId)root.get("id").asInt((int)rootId);
    std::string name = root.get("name").asString("Node");
    std::string type = root.get("type").asString("Node");

    auto node = createNode(type, name, id, error);
    if (!node) return nullptr;

    node->enabled = root.get("enabled").asBool(true);
    node->visible = root.get("visible").asBool(true);

    node->position = parseVec3Or(root.get("position"), Vec3());
    node->rotationDeg = parseVec3Or(root.get("rotationDeg"), Vec3());
    Vec3 sc = parseVec3Or(root.get("scale"), Vec3(1, 1, 1));
    node->scale3 = Vec3(sc.x == 0 ? 1 : sc.x, sc.y == 0 ? 1 : sc.y, sc.z == 0 ? 1 : sc.z);

    const nova::json::JsonValue& props = root.get("props");
    if (props.isObject()) {
        for (const auto& kv : props.asObject()) {
            if (kv.second.isNumber()) {
                node->props.setNum(kv.first, kv.second.asNumber());
            } else if (kv.second.isString()) {
                node->props.set(kv.first, kv.second.asString());
            } else if (kv.second.isBool()) {
                node->props.set(kv.first, kv.second.asBool() ? "true" : "false");
            }
        }
    }
    const nova::json::JsonValue& scriptObj = root.get("script");
    if (scriptObj.isObject()) {
        std::string src = scriptObj.get("source").asString();
        if (!src.empty()) node->script = Script::createFromSource(type, name, src, error);
    }

    // Children are added by the caller (parent sets them up).
    if (parent) {
        parent->addChild(node);
    }
    for (const nova::json::JsonValue& kid : root.get("children").asArray()) {
        std::string childError;
        // Re-serialize child value to string to reuse fromJson.
        std::string childJson = kid.dumpCompact();
        auto child = Node::fromJson(childJson, 0, node.get(), &childError);
        if (!child) {
            if (error && childError.empty()) *error = "failed to load child node";
        }
    }
    return node;
}

std::shared_ptr<Node> Node::createNode(const std::string& type, const std::string& name,
                                       NodeId id, std::string* error) {
    // All node types share the base Node in the core; specialized behavior is
    // driven by `type` + property keys (sprite source, body flags, ...).
    auto node = std::make_shared<Node>(id, name, type);
    if (error) error->clear();
    return node;
}

} // namespace nova
