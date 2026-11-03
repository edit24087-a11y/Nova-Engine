// Nova Engine — scene manager: in-memory scene registry, save/load to JSON,
// undo/redo command stack, project.json I/O.
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include "nova/scene/Node.hpp"
#include "nova/json.hpp"

namespace nova {

class Scene {
public:
    std::string name;
    NodeId rootId = 1;
    std::shared_ptr<Node> root;

    Scene() {
        root = Node::createNode("Node3D", "Root", rootId, nullptr);
    }
    std::string toJson() const;
    bool loadJson(const std::string& text, std::string* error);
    bool saveToFile(const std::string& path, std::string* error);
    bool loadFromFile(const std::string& path, std::string* error);
    std::shared_ptr<Node> findByPath(const std::string& path);
};

// Command stack for undo/redo of node operations.
class CommandStack {
public:
    struct Command {
        std::string describe;
        std::function<void()> undo;
        std::function<void()> redo;
    };
    void push(Command cmd);
    bool canUndo() const { return undoPos_ > 0; }
    bool canRedo() const { return undoPos_ < (int)stack_.size(); }
    void undo();
    void redo();
    void clear() { stack_.clear(); undoPos_ = 0; }
    size_t size() const { return stack_.size(); }

private:
    std::vector<Command> stack_;
    int undoPos_ = 0;
};

class SceneManager {
public:
    std::map<std::string, std::shared_ptr<Scene>> scenes;
    CommandStack commands;

    std::shared_ptr<Scene> current;

    // Project metadata (project.json).
    json::JsonValue projectMeta;

    bool openProjectMeta(const std::string& path, std::string* error);
    bool saveProjectMeta(const std::string& path, std::string* error);
    std::string projectPath;

    std::shared_ptr<Scene> createScene(const std::string& name);
    std::shared_ptr<Scene> findScene(const std::string& name);
    bool saveScene(const std::string& name, std::string* error);
    bool loadScene(const std::string& name, std::string* error);
    bool removeScene(const std::string& name);

    // Undo/redo-aware node operations on the current scene.
    std::shared_ptr<Node> addNode(const std::string& type, const std::string& name,
                                  std::shared_ptr<Node> parent);
    std::shared_ptr<Node> addNodeAt(const std::string& type, const std::string& name,
                                    std::shared_ptr<Node> parent, NodeId id);
    bool deleteNode(Node* node);
    bool renameNode(Node* node, const std::string& newName);
    bool moveNodeToParent(Node* node, std::shared_ptr<Node> newParent, size_t index = 0);
    static NodeId nextNodeId(Scene* scene);
};

} // namespace nova
