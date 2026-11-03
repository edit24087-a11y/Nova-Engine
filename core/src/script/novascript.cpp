// Nova Engine — NovaScript VM implementation.
#include "nova/script/novascript.hpp"
#include "nova/script/Script.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace nova {

// ---------------------------------------------------------------------------
// Singleton accessor (the engine owns one VM instance; scripts share it).
// ---------------------------------------------------------------------------
NovaScriptVM& NovaScriptVM::instance() {
    static NovaScriptVM vm;
    return vm;
}

void NovaScriptVM::clear() {
    functions_.clear();
    globals_.clear();
    error_.clear();
}

void NovaScriptVM::setSource(const std::string& source) { source_ = source; }

bool NovaScriptVM::compile(const std::string& source, std::string* error) {
    clear();
    source_ = source;
    return NovaScriptCompiler::compile(source, this, error);
}

bool NovaScriptVM::resolveFunction(const std::string& name, Chunk* out, std::string* error) {
    auto it = functions_.find(name);
    if (it == functions_.end()) {
        if (error) *error = "function not found: " + name;
        return false;
    }
    *out = it->second;
    return true;
}

NsValue NovaScriptVM::evalConst(const Instr& ins, const Chunk& chunk) {
    switch (ins.op) {
    case Op::LoadConst:
        if (ins.a < 0 || ins.a >= (int)chunk.consts.size()) return NsValue::nil();
        return NsValue::number(chunk.consts[ins.a]);
    case Op::LoadGlobal: {
        if (ins.a < 0 || ins.a >= (int)chunk.strings.size()) return NsValue::nil();
        return getGlobal(chunk.strings[ins.a]);
    }
    default: return NsValue::nil();
    }
}

static bool arithmetic(Op op, const NsValue& a, const NsValue& b, NsValue* out) {
    double x = a.type == NsValue::Type::Num ? a.num
                    : a.type == NsValue::Type::Bool ? (a.boolean ? 1.0 : 0.0) : 0.0;
    double y = b.type == NsValue::Type::Num ? b.num
                    : b.type == NsValue::Type::Bool ? (b.boolean ? 1.0 : 0.0) : 0.0;
    switch (op) {
    case Op::Add:
        if (a.type == NsValue::Type::Str || b.type == NsValue::Type::Str) {
            *out = NsValue::string(a.describe() + b.describe());
            return true;
        }
        *out = NsValue::number(x + y);
        return true;
    case Op::Sub: *out = NsValue::number(x - y); return true;
    case Op::Mul: *out = NsValue::number(x * y); return true;
    case Op::Div: *out = NsValue::number(y != 0.0 ? x / y : 0.0); return true;
    case Op::Mod: *out = NsValue::number(y != 0.0 ? std::fmod(x, y) : 0.0); return true;
    default: return false;
    }
}

static bool compare(Op op, const NsValue& a, const NsValue& b, bool* out) {
    if (op == Op::Eq) { *out = (a.describe() == b.describe()); return true; }
    if (op == Op::Neq) { *out = !(a.describe() == b.describe()); return true; }
    double x = a.type == NsValue::Type::Num ? a.num
                    : a.type == NsValue::Type::Bool ? (a.boolean ? 1.0 : 0.0) : 0.0;
    double y = b.type == NsValue::Type::Num ? b.num
                    : b.type == NsValue::Type::Bool ? (b.boolean ? 1.0 : 0.0) : 0.0;
    switch (op) {
    case Op::Lt: *out = x < y; return true;
    case Op::Le: *out = x <= y; return true;
    case Op::Gt: *out = x > y; return true;
    case Op::Ge: *out = x >= y; return true;
    default: return false;
    }
}

bool NovaScriptVM::invokeBuiltin(const std::string& name, std::vector<NsValue> args,
                                 NovaHost* host, NsValue* result) {
    *result = NsValue::nil();
    if (name == "print") {
        std::string msg;
        for (size_t k = 0; k < args.size(); ++k) {
            if (k) msg += " ";
            msg += args[k].describe();
        }
        if (host) host->log(msg);
    } else if (name == "len") {
        *result = NsValue::number((double)args[0].describe().size());
    } else if (name == "abs") {
        *result = NsValue::number(std::fabs(args[0].num));
    } else if (name == "floor") {
        *result = NsValue::number(std::floor(args[0].num));
    } else if (name == "ceil") {
        *result = NsValue::number(std::ceil(args[0].num));
    } else if (name == "sqrt") {
        *result = NsValue::number(std::sqrt(args[0].num));
    } else if (name == "min") {
        *result = NsValue::number(std::min(args[0].num, args[1].num));
    } else if (name == "max") {
        *result = NsValue::number(std::max(args[0].num, args[1].num));
    } else if (name == "clamp") {
        *result = NsValue::number(nova::clamp(args[0].num, args[1].num, args[2].num));
    } else if (name == "distance") {
        Vec2 a(args[0].num, args[1].num);
        Vec2 b(args[2].num, args[3].num);
        *result = NsValue::number((a - b).length());
    } else if (name == "lerp") {
        *result = NsValue::number(nova::lerp(args[0].num, args[1].num, args[2].num));
    } else if (name == "random") {
        int lo = (int)args[0].num, hi = (int)args[1].num;
        *result = NsValue::number((double)(lo + (std::rand() % (hi - lo + 1))));
    } else if (name == "to_number") {
        try { *result = NsValue::number(std::stod(args[0].describe())); }
        catch (...) { *result = NsValue::number(0.0); }
    } else if (name == "to_string") {
        *result = NsValue::string(args[0].describe());
    } else if (name == "get_node") {
        if (host) *result = NsValue::nodeRef(host->findNode(args[0].describe()));
    } else if (name == "get_prop") {
        if (args[0].type == NsValue::Type::Ref && args[0].ref && host) {
            *result = host->getProp(args[0].ref, args[1].describe());
        }
    } else if (name == "set_prop") {
        if (args[0].type == NsValue::Type::Ref && args[0].ref && host) {
            host->setProp(args[0].ref, args[1].describe(), args[2]);
        }
    } else if (name == "add_child") {
        if (args[0].type == NsValue::Type::Ref && args[1].type == NsValue::Type::Ref &&
            args[1].ref) {
            args[1].ref->addChild(std::shared_ptr<Node>(args[0].ref->shared_from_this()));
        }
    } else if (name == "remove_node") {
        if (args[0].type == NsValue::Type::Ref && args[0].ref) {
            args[0].ref->removeSelf();
        }
    } else if (name == "move_node") {
        if (args[0].type == NsValue::Type::Ref && args[0].ref) {
            Vec3 p = args[0].ref->position;
            p.x += args[1].num;
            p.y += args[2].num;
            args[0].ref->position = p;
        }
    } else if (name == "rotate_node") {
        if (args[0].type == NsValue::Type::Ref && args[0].ref) {
            args[0].ref->rotationDeg.z += args[1].num;
        }
    } else if (name == "scale_node") {
        if (args[0].type == NsValue::Type::Ref && args[0].ref) {
            args[0].ref->scale3 = Vec3(args[0].ref->scale3.x * args[1].num,
                                       args[0].ref->scale3.y * args[2].num, 1.0f);
        }
    } else if (name == "get_position") {
        if (args[0].type == NsValue::Type::Ref && args[0].ref) {
            *result = NsValue::number(args[0].ref->position.x);
        }
    } else if (name == "set_position") {
        if (args[0].type == NsValue::Type::Ref && args[0].ref) {
            args[0].ref->position.x = (float)args[1].num;
            args[0].ref->position.y = (float)args[2].num;
        }
    } else if (name == "emit_signal") {
        if (args[0].type == NsValue::Type::Ref && args[0].ref) {
            args[0].ref->emit(args[1].describe());
        }
    } else if (name == "instance") {
        *result = args[0]; // placeholder for prefab instancing API
    } else {
        return false;
    }
    return true;
}

static const std::vector<std::string>& builtinNames() {
    static const std::vector<std::string> names = {
        "print", "len", "abs", "floor", "ceil", "sqrt", "min", "max",
        "clamp", "distance", "lerp", "random", "to_number", "to_string",
        "get_node", "get_prop", "set_prop", "add_child", "remove_node",
        "move_node", "rotate_node", "scale_node", "new_node",
        "get_position", "set_position", "emit_signal", "instance",
    };
    return names;
}

bool NovaScriptVM::execChunk(NovaHost* host, std::string* error) {
    ip_ = 0;
    stack_.clear();
    const Chunk* chunk = &execChunk_;
while (ip_ < code_.size()) {
        const Instr& ins = code_[ip_++];
        if (getenv("NOVA_TRACE")) {
            const char* opNames[] = {"Nop","LoadConst","LoadLocal","LoadGlobal","StoreLocal","StoreGlobal","Add","Sub","Mul","Div","Mod","Eq","Neq","Lt","Le","Gt","Ge","And","Or","Not","Jump","JumpIfFalse","JumpIfTrue","Call","LoadString","Ret","Halt","GetProp","SetProp","NewNode","AddChild","RemoveNode"};
            std::fprintf(stderr, "ip=%zu op=%-14s a=%d b=%d stack=%zu\n", ip_-1, opNames[(int)ins.op], ins.a, ins.b, stack_.size());
        }
        switch (ins.op) {
        case Op::Nop: break;
        case Op::LoadConst: case Op::LoadGlobal:
            stack_.push_back(evalConst(ins, *chunk));
            break;
        case Op::LoadString: {
            if (ins.a < 0 || ins.a >= (int)chunk->strings.size()) {
                if (error) *error = "bad string index";
                return false;
            }
            stack_.push_back(NsValue::string(chunk->strings[ins.a]));
            break;
        }
        case Op::LoadLocal:
            if (ins.a < 0 || ins.a >= (int)locals_.size()) stack_.push_back(NsValue::nil());
            else stack_.push_back(locals_[ins.a]);
            break;
        case Op::StoreLocal:
            if (ins.a < 0 || ins.a >= (int)locals_.size()) {
                if (error) *error = "store to unbound local slot";
                return false;
            }
            if (stack_.empty()) { if (error) *error = "store with empty stack"; return false; }
            locals_[ins.a] = stack_.back();
            stack_.pop_back();
            break;
        case Op::StoreGlobal: {
            if (stack_.empty()) { if (error) *error = "store with empty stack"; return false; }
            if (ins.a < 0 || ins.a >= (int)chunk->strings.size()) {
                if (error) *error = "bad global name index"; return false;
            }
            setGlobal(chunk->strings[ins.a], stack_.back());
            stack_.pop_back();
            break;
        }
        case Op::Add: case Op::Sub: case Op::Mul: case Op::Div: case Op::Mod: {
            if (stack_.size() < 2) { if (error) *error = "binary op needs 2 operands"; return false; }
            NsValue b = stack_.back(); stack_.pop_back();
            NsValue a = stack_.back(); stack_.pop_back();
            NsValue out;
            if (!arithmetic(ins.op, a, b, &out)) { if (error) *error = "bad arithmetic"; return false; }
            stack_.push_back(std::move(out));
            break;
        }
        case Op::Eq: case Op::Neq: case Op::Lt: case Op::Le: case Op::Gt: case Op::Ge: {
            if (stack_.size() < 2) { if (error) *error = "compare needs 2 operands"; return false; }
            NsValue b = stack_.back(); stack_.pop_back();
            NsValue a = stack_.back(); stack_.pop_back();
            bool res;
            if (!compare(ins.op, a, b, &res)) { if (error) *error = "bad compare"; return false; }
            stack_.push_back(NsValue::ofBool(res));
            break;
        }
        case Op::And: {
            if (stack_.size() < 2) { if (error) *error = "and needs 2 operands"; return false; }
            NsValue b = stack_.back(); stack_.pop_back();
            NsValue a = stack_.back(); stack_.pop_back();
            stack_.push_back(NsValue::ofBool(a.truthy() && b.truthy()));
            break;
        }
        case Op::Or: {
            if (stack_.size() < 2) { if (error) *error = "or needs 2 operands"; return false; }
            NsValue b = stack_.back(); stack_.pop_back();
            NsValue a = stack_.back(); stack_.pop_back();
            stack_.push_back(NsValue::ofBool(a.truthy() || b.truthy()));
            break;
        }
        case Op::Not: {
            if (stack_.empty()) { if (error) *error = "not needs 1 operand"; return false; }
            NsValue a = stack_.back(); stack_.pop_back();
            stack_.push_back(NsValue::ofBool(!a.truthy()));
            break;
        }
        case Op::Jump:
            ip_ = (size_t)ins.a;
            break;
        case Op::JumpIfFalse: {
            if (stack_.empty()) { if (error) *error = "jump needs 1 operand"; return false; }
            NsValue v = stack_.back(); stack_.pop_back();
            if (!v.truthy()) ip_ = (size_t)ins.a;
            break;
        }
        case Op::JumpIfTrue: {
            if (stack_.empty()) { if (error) *error = "jump needs 1 operand"; return false; }
            NsValue v = stack_.back(); stack_.pop_back();
            if (v.truthy()) ip_ = (size_t)ins.a;
            break;
        }
        case Op::GetProp: {
            if (stack_.empty()) { if (error) *error = "get_prop needs node"; return false; }
            NsValue nodeVal = stack_.back(); stack_.pop_back();
            if (ins.a < 0 || ins.a >= (int)chunk->strings.size()) {
                if (error) *error = "bad prop name index"; return false;
            }
            std::string key = chunk->strings[ins.a];
            if (nodeVal.type == NsValue::Type::Ref && nodeVal.ref && host) {
                stack_.push_back(host->getProp(nodeVal.ref, key));
            } else {
                stack_.push_back(NsValue::nil());
            }
            break;
        }
        case Op::SetProp: {
            if (stack_.size() < 2) { if (error) *error = "set_prop needs value+node"; return false; }
            NsValue nodeVal = stack_.back(); stack_.pop_back();
            if (ins.a < 0 || ins.a >= (int)chunk->strings.size()) {
                if (error) *error = "bad prop name index"; return false;
            }
            std::string key = chunk->strings[ins.a];
            if (nodeVal.type == NsValue::Type::Ref && nodeVal.ref && host) {
                host->setProp(nodeVal.ref, key, stack_.back());
                stack_.pop_back();
            } else {
                stack_.pop_back(); // discard value
            }
            break;
        }
        case Op::NewNode: {
            if (ins.a < 0 || ins.a >= (int)chunk->strings.size() ||
                ins.b < 0 || ins.b >= (int)chunk->strings.size()) {
                if (error) *error = "bad new_node operands"; return false;
            }
            std::string type = chunk->strings[ins.a];
            std::string name = chunk->strings[ins.b];
            static NodeId counter = 1000;
            auto node = Node::createNode(type, name, ++counter, error);
            if (!node) return false;
            stack_.push_back(NsValue::nodeRef(node.get()));
            break;
        }
        case Op::AddChild: {
            if (stack_.size() < 2) { if (error) *error = "add_child needs child+parent"; return false; }
            NsValue child = stack_.back(); stack_.pop_back();
            NsValue parent = stack_.back(); stack_.pop_back();
            if (child.type == NsValue::Type::Ref && parent.type == NsValue::Type::Ref &&
                parent.ref && child.ref) {
                parent.ref->addChild(std::shared_ptr<Node>(child.ref->shared_from_this()));
            }
            break;
        }
        case Op::RemoveNode: {
            if (stack_.empty()) { if (error) *error = "remove needs node"; return false; }
            NsValue nodeVal = stack_.back(); stack_.pop_back();
            if (nodeVal.type == NsValue::Type::Ref && nodeVal.ref) {
                nodeVal.ref->removeSelf();
            }
            break;
        }
        case Op::Call: {
            int nameIdx = ins.a;
            int argc = ins.b;
            if (stack_.size() < argc) { if (error) *error = "call arg count mismatch"; return false; }
            std::vector<NsValue> args(argc);
            for (int i = argc - 1; i >= 0; --i) {
                args[i] = stack_.back();
                stack_.pop_back();
            }
            if (nameIdx < 0 || nameIdx >= (int)chunk->strings.size()) {
                if (error) *error = "bad call name index"; return false;
            }
            std::string fname = chunk->strings[nameIdx];
            if (fname == "__program__") {
                if (error) *error = "recursive __program__ call"; return false;
            }
            // Builtins first.
            bool handled = false;
            NsValue result;
            for (const auto& b : builtinNames()) {
                if (b != fname) continue;
                handled = invokeBuiltin(fname, args, host, &result);
                break;
            }
            if (handled) {
                stack_.push_back(result);
                break;
            }
            Chunk target;
            if (!resolveFunction(fname, &target, error)) {
                if (error) *error = "call to undefined function: " + fname;
                return false;
            }
            if (recursion_ > 32) {
                if (error) *error = "recursion limit exceeded";
                return false;
            }
            Frame frame;
            frame.locals = std::move(locals_);
            frame.chunk = *chunk; // caller's chunk snapshot
            frame.ip = ip_;
            callStack_.push_back(std::move(frame));

            locals_.assign(target.numLocals, NsValue::nil());
            for (size_t i = 0; i < target.params.size() && i < args.size(); ++i) {
                locals_[i] = args[i];
            }
            // Copy into a VM member so the pointer stays valid across the
            // whole execution (scoped locals die with their case block).
            execChunk_ = std::move(target);
            code_ = execChunk_.code;
            chunk = &execChunk_;
            ip_ = 0;
            break;
        }
        case Op::Ret: {
            if (callStack_.empty()) {
                ip_ = code_.size();
                break;
            }
            Frame f = std::move(callStack_.back());
            callStack_.pop_back();
            --recursion_;
            locals_.swap(f.locals);
            execChunk_ = std::move(f.chunk); // restore caller's chunk
            code_ = execChunk_.code;
            chunk = &execChunk_;
            ip_ = f.ip;
            break;
        }
        case Op::Halt:
            ip_ = code_.size();
            break;
        default:
            if (error) *error = "unknown opcode";
            return false;
        }
        if (ip_ > code_.size()) {
            if (error) *error = "instruction pointer overflow";
            return false;
        }
    }
    return true;
}

bool NovaScriptVM::execLoop(NovaHost* host, std::string* error) {
    stack_.clear();
    locals_.clear();
    callStack_.clear();
    recursion_ = 0;
    Chunk mainChunk;
    if (!resolveFunction("__program__", &mainChunk, error)) {
        mainChunk.code.push_back({Op::Halt, 0, 0});
    }
    execChunk_ = std::move(mainChunk);
    code_ = execChunk_.code;
    locals_.assign(execChunk_.numLocals, NsValue::nil());
    return execChunk(host, error);
}

bool NovaScriptVM::run(NovaHost* host, std::string* error) {
    error_.clear();
    bool ok = execLoop(host, error);
    if (error && !error_.empty() && error->empty()) *error = error_;
    return ok;
}

bool NovaScriptVM::callFunction(const std::string& name, NovaHost* host,
                                std::vector<NsValue> args, std::string* error) {
    Chunk target;
    if (!resolveFunction(name, &target, error)) return false;
    if (recursion_ > 32) {
        if (error) *error = "recursion limit exceeded";
        return false;
    }
    Frame frame;
    frame.locals = std::move(locals_);
    frame.chunk = execChunk_; // snapshot current context
    frame.ip = code_.size();
    callStack_.push_back(std::move(frame));

    locals_.assign(target.numLocals, NsValue::nil());
    for (size_t i = 0; i < target.params.size() && i < args.size(); ++i) {
        locals_[i] = args[i];
    }
    execChunk_ = std::move(target);
    code_ = execChunk_.code;
    bool ok = execChunk(host, error);
    if (!callStack_.empty()) {
        Frame f = std::move(callStack_.back());
        callStack_.pop_back();
        locals_.swap(f.locals);
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Script base + factory
// ---------------------------------------------------------------------------
std::shared_ptr<Script> Script::createFromSource(const std::string& nodeType,
                                                 const std::string& nodeName,
                                                 const std::string& source,
                                                 std::string* error) {
    struct NovaScriptImpl : public Script {
        std::string src;
        bool compiled = false;
        NovaScriptImpl(std::string s) : src(std::move(s)) {}
        std::string source() const override { return src; }
        bool compile(std::string* error) override {
            compiled = NovaScriptVM::instance().compile(src, error);
            return compiled;
        }
        void run(NovaHost* host, std::string* error) override {
            if (!compiled) compile(error);
            if (node()) NovaScriptVM::instance().setGlobal("self", NsValue::nodeRef(node()));
            NovaScriptVM::instance().run(host, error);
        }
    };
    auto script = std::make_shared<NovaScriptImpl>(source);
    if (error) error->clear();
    if (!script->compile(error)) {
        return nullptr;
    }
    (void)nodeType; (void)nodeName;
    return script;
}

} // namespace nova
