// Nova Engine — script abstraction.
// Scripts attach to nodes; the engine compiles + runs them through the
// embedded NovaScript bytecode VM. C++ extensions can register their own
// Script subclasses through the factory.
#pragma once
#include <string>
#include <memory>
#include "nova/scene/Node.hpp"

namespace nova {

class NovaHost;

class Script {
public:
    Script() = default;
    virtual ~Script() = default;

    virtual std::string source() const = 0;
    virtual bool compile(std::string* error) = 0;
    virtual void run(NovaHost* host, std::string* error) = 0;

    // Node this script is attached to (set before on_ready runs).
    void setNode(Node* node) { node_ = node; }
    Node* node() const { return node_; }

    static std::shared_ptr<Script> createFromSource(const std::string& nodeType,
                                                    const std::string& nodeName,
                                                    const std::string& source,
                                                    std::string* error);

    // Called once when the node enters the tree.
    virtual void onReady(NovaHost* host) { std::string err; run(host, &err); }

private:
    Node* node_ = nullptr;
};

} // namespace nova
