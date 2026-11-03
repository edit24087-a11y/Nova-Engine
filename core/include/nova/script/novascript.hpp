// Nova Engine — NovaScript: embedded bytecode scripting language.
//
// A small stack-based VM with a text compiler. Supports local/global
// variables, arithmetic/comparison/logic operators, if/elif/else, while/for,
// break/continue, named functions, member-style property access
// (`node.prop` reads, `node.prop = v` writes), and engine API calls
// (get_node, add_child, move_node, print, ...).
//
// Visual scripting (Phase 5) compiles node graphs into NovaScript bytecode,
// so the same VM executes both text scripts and graph scripts.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include <memory>
#include "nova/scene/Node.hpp"

namespace nova {

// ---------------------------------------------------------------------------
// Value
// ---------------------------------------------------------------------------
struct NsValue {
    enum class Type { Nil, Num, Str, Bool, Ref };
    Type type = Type::Nil;
    double num = 0.0;
    std::string str;
    bool boolean = false;
    Node* ref = nullptr; // Node reference (Type::Ref)

    static NsValue nil() { return NsValue(); }
    static NsValue number(double v) { NsValue r; r.type = Type::Num; r.num = v; return r; }
    static NsValue string(const std::string& s) { NsValue r; r.type = Type::Str; r.str = s; return r; }
    static NsValue ofBool(bool b) { NsValue r; r.type = Type::Bool; r.boolean = b; return r; }
    static NsValue nodeRef(Node* n) { NsValue r; r.type = Type::Ref; r.ref = n; return r; }

    bool truthy() const {
        if (type == Type::Bool) return boolean;
        if (type == Type::Num) return num != 0.0;
        if (type == Type::Str) return !str.empty();
        if (type == Type::Ref) return ref != nullptr;
        return false;
    }
    std::string describe() const {
        switch (type) {
        case Type::Nil: return "nil";
        case Type::Num: return std::to_string(num);
        case Type::Str: return str;
        case Type::Bool: return boolean ? "true" : "false";
        case Type::Ref: return ref ? ref->name : "nil";
        }
        return "nil";
    }
};

// ---------------------------------------------------------------------------
// Host interface — the engine implements this so scripts can touch the world.
// ---------------------------------------------------------------------------
class NovaHost {
public:
    virtual ~NovaHost() = default;
    virtual void log(const std::string& msg) = 0;
    virtual Node* findNode(const std::string& path) = 0; // "/Player" or "Player"
    virtual NsValue getProp(Node* node, const std::string& key) = 0;
    virtual void setProp(Node* node, const std::string& key, const NsValue& value) = 0;
    // Optional free-form API call (used for engine functions like move_node).
    virtual NsValue callApi(const std::string& name, const std::vector<NsValue>& args) {
        (void)name; (void)args; return NsValue::nil();
    }
};

// ---------------------------------------------------------------------------
// Bytecode
// ---------------------------------------------------------------------------
enum class Op : uint8_t {
    Nop,
    LoadConst,    // operand: const index
    LoadLocal,    // operand: slot
    LoadGlobal,   // operand: name index
    StoreLocal,   // operand: slot
    StoreGlobal,  // operand: name index
    Add, Sub, Mul, Div, Mod,
    Eq, Neq, Lt, Le, Gt, Ge,
    And, Or, Not,
    Jump,         // operand: target
    JumpIfFalse,  // operand: target (pops value)
    JumpIfTrue,   // operand: target (pops value)
    Call,         // operands: name index, argc
    LoadString,   // operand: string index -> string value
    Ret,
    Halt,
    GetProp,      // pops: node; operand a: prop-name index -> value
    SetProp,      // pops: value, node; operand a: prop-name index
    NewNode,      // operands: type idx, name idx -> node ref
    AddChild,     // pops: child, parent
    RemoveNode,   // pops: node
};

struct Instr {
    Op op;
    int32_t a = 0;
    int32_t b = 0;
};

struct Chunk {
    std::vector<Instr> code;
    std::vector<double> consts;
    std::vector<std::string> strings;
    std::vector<std::string> params; // function parameters
    int numLocals = 0;               // local slots (params + var declarations)
};

// ---------------------------------------------------------------------------
// VM
// ---------------------------------------------------------------------------
class NovaScriptVM {
public:
    static NovaScriptVM& instance();

    void clear();
    bool compile(const std::string& source, std::string* error);
    std::string lastError() const { return error_; }

    void setSource(const std::string& source);
    const std::string& source() const { return source_; }

    // Execute the whole program.
    bool run(NovaHost* host, std::string* error);

    // Execute a named function (must exist in global table).
    bool callFunction(const std::string& name, NovaHost* host, std::vector<NsValue> args,
                      std::string* error);

    const std::map<std::string, Chunk>& functions() const { return functions_; }
    void installFunctions(std::map<std::string, Chunk> funcs) { functions_ = std::move(funcs); }
    const std::map<std::string, NsValue>& globals() const { return globals_; }
    void setGlobal(const std::string& name, NsValue v) { globals_[name] = std::move(v); }
    NsValue getGlobal(const std::string& name) const {
        auto it = globals_.find(name);
        return it == globals_.end() ? NsValue::nil() : it->second;
    }

    NovaScriptVM() = default;

private:
    std::string source_;
    std::string error_;
    std::map<std::string, Chunk> functions_;
    std::map<std::string, NsValue> globals_;

    // Execution state. Each frame owns a copy of its caller's chunk so nested
    // calls cannot corrupt the caller's code/string/const tables.
    struct Frame {
        std::vector<NsValue> locals;
        Chunk chunk;
        size_t ip = 0;
    };
    std::vector<Instr> code_;
    std::vector<NsValue> stack_;
    std::vector<NsValue> locals_;
    std::vector<Frame> callStack_;
    size_t ip_ = 0;
    int recursion_ = 0;
    Chunk execChunk_; // holds the chunk currently being executed (stable address)

    bool execLoop(NovaHost* host, std::string* error);
    bool execChunk(NovaHost* host, std::string* error); // executes code_/execChunk_
    NsValue evalConst(const Instr& ins, const Chunk& chunk);
    bool resolveFunction(const std::string& name, Chunk* out, std::string* error);
    bool invokeBuiltin(const std::string& name, std::vector<NsValue> args, NovaHost* host,
                       NsValue* result);
};

// ---------------------------------------------------------------------------
// Text compiler (lexer + parser) — produces VM bytecode programs.
// ---------------------------------------------------------------------------
class NovaScriptCompiler {
public:
    // Compile text source into a VM program (global chunk + named functions).
    static bool compile(const std::string& source, NovaScriptVM* vm, std::string* error);
};

} // namespace nova
