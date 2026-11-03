// Nova Engine — scene manager implementation.
#include "nova/scene/SceneManager.hpp"
#include "nova/util/fs.hpp"
#include <algorithm>
#include <cctype>

namespace nova {

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------
std::string Scene::toJson() const {
    json::JsonValue meta(json::JsonValue::object());
    meta.set("name", name);
    meta.set("rootId", (double)rootId);
    if (root) {
        // Wrap the node tree JSON under the "root" key.
        std::string nodeJson = root->toJson();
        meta.set("root", json::JsonValue::parse(nodeJson));
    }
    return meta.dump();
}

bool Scene::loadJson(const std::string& text, std::string* error) {
    json::JsonValue root;
    try {
        root = json::JsonValue::parse(text);
    } catch (const std::exception& e) {
        if (error) *error = std::string("scene parse error: ") + e.what();
        return false;
    }
    name = root.get("name").asString("Scene");
    rootId = (NodeId)root.get("rootId").asInt(1);
    std::string rootJson;
    const json::JsonValue& rootNode = root.get("root");
    if (rootNode.isObject()) rootJson = rootNode.dumpCompact();
    else if (rootNode.isString()) rootJson = rootNode.asString();
    if (rootJson.empty()) {
        if (error) *error = "scene json has no root node";
        return false;
    }
    auto loaded = Node::fromJson(rootJson, rootId, nullptr, error);
    if (!loaded) return false;
    this->root = loaded;
    return true;
}

bool Scene::saveToFile(const std::string& path, std::string* error) {
    std::string json = toJson();
    if (!fs::writeFile(path, json)) {
        if (error) *error = "cannot write scene file: " + path;
        return false;
    }
    return true;
}

bool Scene::loadFromFile(const std::string& path, std::string* error) {
    std::string text;
    if (!fs::readFile(path, text)) {
        if (error) *error = "cannot read scene file: " + path;
        return false;
    }
    return loadJson(text, error);
}

std::shared_ptr<Node> Scene::findByPath(const std::string& path) {
    if (!root) return nullptr;
    if (path.empty() || path == "/") return root;
    std::string p = path;
    if (p.front() == '/') p = p.substr(1);
    Node* cur = root.get();
    size_t start = 0;
    while (start <= p.size()) {
        size_t slash = p.find('/', start);
        std::string part = p.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!part.empty()) {
            if (std::isdigit((unsigned char)part[0])) {
                // Numeric id path.
                try {
                    auto found = cur->findById((NodeId)std::stoul(part));
                    if (!found) return nullptr;
                    cur = found.get();
                } catch (...) {
                    return nullptr;
                }
            } else {
                auto child = cur->findChildByName(part);
                if (!child) return nullptr;
                cur = child.get();
            }
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return cur->shared_from_this();
}

// ---------------------------------------------------------------------------
// CommandStack
// ---------------------------------------------------------------------------
void CommandStack::push(Command cmd) {
    if (undoPos_ < (int)stack_.size()) stack_.erase(stack_.begin() + undoPos_, stack_.end());
    stack_.push_back(std::move(cmd));
    ++undoPos_;
}

void CommandStack::undo() {
    if (!canUndo()) return;
    --undoPos_;
    stack_[undoPos_].undo();
}

void CommandStack::redo() {
    if (!canRedo()) return;
    stack_[undoPos_].redo();
    ++undoPos_;
}

// ---------------------------------------------------------------------------
// SceneManager
// ---------------------------------------------------------------------------
std::shared_ptr<Scene> SceneManager::createScene(const std::string& name) {
    auto scene = std::make_shared<Scene>();
    scene->name = name;
    scenes[name] = scene;
    if (!current) current = scene;
    return scene;
}

std::shared_ptr<Scene> SceneManager::findScene(const std::string& name) {
    auto it = scenes.find(name);
    return it == scenes.end() ? nullptr : it->second;
}

bool SceneManager::saveScene(const std::string& name, std::string* error) {
    auto scene = findScene(name);
    if (!scene) {
        if (error) *error = "scene not found: " + name;
        return false;
    }
    if (projectPath.empty()) {
        if (error) *error = "no project path set";
        return false;
    }
    std::string path = fs::join(fs::join(projectPath, "scenes"), name + ".scene.json");
    return scene->saveToFile(path, error);
}

bool SceneManager::loadScene(const std::string& name, std::string* error) {
    if (projectPath.empty()) {
        if (error) *error = "no project path set";
        return false;
    }
    std::string path = fs::join(fs::join(projectPath, "scenes"), name + ".scene.json");
    auto scene = std::make_shared<Scene>();
    if (!scene->loadFromFile(path, error)) return false;
    scenes[name] = scene;
    current = scene;
    return true;
}

bool SceneManager::removeScene(const std::string& name) {
    auto it = scenes.find(name);
    if (it == scenes.end()) return false;
    if (current == it->second) current = nullptr;
    scenes.erase(it);
    return true;
}

bool SceneManager::openProjectMeta(const std::string& path, std::string* error) {
    try {
        projectMeta = json::JsonValue::parseFile(path);
    } catch (const std::exception& e) {
        if (error) *error = std::string("project parse error: ") + e.what();
        return false;
    }
    projectPath = fs::dirName(path);
    return true;
}

bool SceneManager::saveProjectMeta(const std::string& path, std::string* error) {
    if (!fs::writeFile(path, projectMeta.dump(2))) {
        if (error) *error = "cannot write project.json: " + path;
        return false;
    }
    return true;
}

std::shared_ptr<Node> SceneManager::addNode(const std::string& type, const std::string& name,
                                            std::shared_ptr<Node> parent) {
    return addNodeAt(type, name, parent, nextNodeId(current.get()));
}

std::shared_ptr<Node> SceneManager::addNodeAt(const std::string& type, const std::string& name,
                                              std::shared_ptr<Node> parent, NodeId id) {
    auto node = Node::createNode(type, name, id, nullptr);
    if (!node) return nullptr;
    if (parent) parent->addChild(node);
    // Undoable add.
    std::shared_ptr<Node> keepParent = parent;
    CommandStack::Command cmd;
    cmd.describe = "add " + name;
    cmd.undo = [this, node, keepParent]() {
        node->detachAllChildren();
        for (auto& c : node->children) c->parent = nullptr;
        if (keepParent) keepParent->removeChild(node.get());
        node->parent = nullptr;
    };
    cmd.redo = [node, keepParent]() {
        if (keepParent && !node->parent) keepParent->addChild(node);
    };
    commands.push(std::move(cmd));
    return node;
}

bool SceneManager::deleteNode(Node* node) {
    if (!node || !node->parent) return false;
    auto parent = std::shared_ptr<Node>(node->parent->shared_from_this());

    // Snapshot (parent, index) for every node in the subtree, pre-order.
    struct Link {
        std::shared_ptr<Node> parent;
        size_t index;
        std::shared_ptr<Node> child;
    };
    std::vector<Link> links;
    std::function<void(Node*, Node*)> collect = [&](Node* n, Node* par) {
        if (par) {
            size_t idx = 0;
            for (size_t i = 0; i < par->children.size(); ++i) {
                if (par->children[i].get() == n) { idx = i; break; }
            }
            links.push_back({std::shared_ptr<Node>(par->shared_from_this()), idx,
                             std::shared_ptr<Node>(n->shared_from_this())});
        }
        for (auto& c : n->children) collect(c.get(), n);
    };
    collect(node, node->parent);

    // Perform the delete.
    node->detachAllChildren();
    parent->removeChild(node);

    CommandStack::Command cmd;
    cmd.describe = "delete " + node->name;
    cmd.undo = [links]() {
        for (const auto& l : links) {
            if (!l.parent) continue;
            size_t idx = l.index;
            if (idx > l.parent->children.size()) idx = l.parent->children.size();
            l.parent->children.insert(l.parent->children.begin() + idx, l.child);
            l.child->parent = l.parent.get();
            l.child->reparentChildren();
        }
    };
    cmd.redo = [parent, node]() {
        node->detachAllChildren();
        parent->removeChild(node);
    };
    commands.push(std::move(cmd));
    return true;
}

bool SceneManager::renameNode(Node* node, const std::string& newName) {
    if (!node) return false;
    std::string old = node->name;
    node->name = newName;
    CommandStack::Command cmd;
    cmd.describe = "rename " + old + " -> " + newName;
    cmd.undo = [node, old]() { node->name = old; };
    cmd.redo = [node, newName]() { node->name = newName; };
    commands.push(std::move(cmd));
    return true;
}

bool SceneManager::moveNodeToParent(Node* node, std::shared_ptr<Node> newParent, size_t index) {
    if (!node || !newParent || node == newParent.get()) return false;
    // Prevent moving into own descendant.
    for (auto& d : node->descendants()) {
        if (d.get() == newParent.get()) return false;
    }
    auto oldParent = node->parent ? std::shared_ptr<Node>(node->parent->shared_from_this()) : nullptr;
    size_t oldIndex = 0;
    if (oldParent) {
        for (size_t i = 0; i < oldParent->children.size(); ++i) {
            if (oldParent->children[i].get() == node) { oldIndex = i; break; }
        }
    }
    if (oldParent) oldParent->removeChild(node);
    node->parent = nullptr;
    if (index > newParent->children.size()) index = newParent->children.size();
    newParent->children.insert(newParent->children.begin() + index,
                               std::shared_ptr<Node>(node->shared_from_this()));
    node->parent = newParent.get();
    node->reparentChildren();

    CommandStack::Command cmd;
    cmd.describe = "move " + node->name;
    cmd.undo = [node, oldParent, oldIndex]() {
        if (node->parent) node->parent->removeChild(node);
        node->parent = nullptr;
        if (oldParent) {
            oldParent->children.insert(oldParent->children.begin() + oldIndex,
                                       std::shared_ptr<Node>(node->shared_from_this()));
            node->parent = oldParent.get();
            node->reparentChildren();
        }
    };
    cmd.redo = [node, newParent, index]() {
        if (node->parent) node->parent->removeChild(node);
        node->parent = nullptr;
        size_t idx = index;
        if (idx > newParent->children.size()) idx = newParent->children.size();
        newParent->children.insert(newParent->children.begin() + idx,
                                   std::shared_ptr<Node>(node->shared_from_this()));
        node->parent = newParent.get();
    };
    commands.push(std::move(cmd));
    return true;
}

NodeId SceneManager::nextNodeId(Scene* scene) {
    NodeId maxId = 1;
    if (scene && scene->root) {
        std::function<void(Node*)> visit = [&](Node* n) {
            maxId = std::max(maxId, n->id);
            for (auto& c : n->children) visit(c.get());
        };
        visit(scene->root.get());
    }
    return maxId + 1;
}

} // namespace nova
