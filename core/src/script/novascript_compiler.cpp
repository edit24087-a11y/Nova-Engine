// Nova Engine — NovaScript text compiler (lexer + recursive descent parser).
#include "nova/script/novascript.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <algorithm>

namespace nova {

namespace {

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------
struct Token {
    enum class Kind { Number, String, Ident, Keyword, Punct, Eof };
    Kind kind = Kind::Eof;
    std::string text;
    double num = 0.0;
    int col = 0; // column of first char on its line (indentation)
};

bool isKeyword(const std::string& s) {
    static const char* kws[] = {
        "if", "elif", "else", "while", "for", "func", "return", "break", "continue",
        "true", "false", "nil", "and", "or", "not", "to", "var",
    };
    for (const char* k : kws) {
        if (s == k) return true;
    }
    return false;
}

std::vector<Token> tokenize(const std::string& src, std::string* error) {
    std::vector<Token> out;
    size_t i = 0, n = src.size();
    int col = 0;
    while (i < n) {
        char c = src[i];
        if (std::isspace((unsigned char)c)) {
            if (c == '\n') col = 0;
            else ++col;
            ++i; continue;
        }
        if (c == '/' && i + 1 < n && src[i + 1] == '/') {
            while (i < n && src[i] != '\n') ++i;
            continue;
        }
        if (c == '-' && i + 1 < n && src[i + 1] == '-') {
            while (i < n && src[i] != '\n') ++i;
            continue;
        }
        if (std::isdigit((unsigned char)c) ||
            (c == '.' && i + 1 < n && std::isdigit((unsigned char)src[i + 1]))) {
            Token t;
            t.kind = Token::Kind::Number;
            std::string num;
            while (i < n && (std::isdigit((unsigned char)src[i]) || src[i] == '.' ||
                   src[i] == 'e' || src[i] == 'E' || src[i] == '+' || src[i] == '-')) {
                if ((src[i] == '+' || src[i] == '-') && !(src[i - 1] == 'e' || src[i - 1] == 'E')) break;
                num += src[i++];
            }
            t.num = std::strtod(num.c_str(), nullptr);
            t.text = num;
            t.col = col;
            out.push_back(t);
            continue;
        }
        if (c == '"' || c == '\'') {
            char quote = c;
            Token t;
            t.kind = Token::Kind::String;
            ++i;
            std::string str;
            bool closed = false;
            while (i < n) {
                char s = src[i++];
                if (s == quote) { closed = true; break; }
                if (s == '\\' && i < n) {
                    char e = src[i++];
                    switch (e) {
                    case 'n': str += '\n'; break;
                    case 't': str += '\t'; break;
                    case 'r': str += '\r'; break;
                    default: str += e;
                    }
                } else {
                    str += s;
                }
            }
            if (!closed) {
                if (error) *error = "unterminated string";
                out.clear();
                return out;
            }
            t.text = str;
            t.col = col;
            out.push_back(t);
            continue;
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            Token t;
            std::string id;
            while (i < n && (std::isalnum((unsigned char)src[i]) || src[i] == '_')) id += src[i++];
            t.kind = isKeyword(id) ? Token::Kind::Keyword : Token::Kind::Ident;
            t.text = id;
            t.col = col;
            out.push_back(t);
            continue;
        }
        std::string two = (i + 1 < n) ? src.substr(i, 2) : std::string();
        if (two == "==" || two == "!=" || two == "<=" || two == ">=") {
            Token t;
            t.kind = Token::Kind::Punct;
            t.text = two;
            t.col = col;
            out.push_back(t);
            i += 2;
            continue;
        }
        if (std::string("(){};:=+-*/%<>.,").find(c) != std::string::npos) {
            Token t;
            t.kind = Token::Kind::Punct;
            t.text = std::string(1, c);
            t.col = col;
            out.push_back(t);
            ++i;
            continue;
        }
        if (error) {
            std::ostringstream os;
            os << "unexpected character '" << c << "'";
            *error = os.str();
        }
        out.clear();
        return out;
    }
    out.push_back(Token{Token::Kind::Eof, "", 0.0});
    return out;
}

// ---------------------------------------------------------------------------
// Bytecode emitter
// ---------------------------------------------------------------------------
struct Emitter {
    Chunk* chunk_ = nullptr;
    std::vector<Instr>* code_ = nullptr;

    Emitter() = default;
    void bind(Chunk& c) { chunk_ = &c; code_ = &c.code; }

    void emit(Op op, int32_t a = 0, int32_t b = 0) { code_->push_back({op, a, b}); }

    int32_t addConst(double v) {
        chunk_->consts.push_back(v);
        return (int32_t)chunk_->consts.size() - 1;
    }
    int32_t addString(const std::string& s) {
        for (size_t i = 0; i < chunk_->strings.size(); ++i) {
            if (chunk_->strings[i] == s) return (int32_t)i;
        }
        chunk_->strings.push_back(s);
        return (int32_t)chunk_->strings.size() - 1;
    }

    int32_t emitJump(Op op) {
        emit(op, 0);
        return (int32_t)code_->size() - 1;
    }
    void patchJump(int32_t idx) {
        code_->at(idx).a = (int32_t)code_->size();
    }
};

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------
class Parser {
public:
    Parser(std::vector<Token> tokens, std::string* error)
        : toks(std::move(tokens)), err(error) {}

    bool parse(std::map<std::string, Chunk>& out) {
        mainEmitter.bind(mainChunk);
        curChunk = &mainChunk;
        emitter = &mainEmitter;
        locals.clear();
        localIndex.clear();
        if (!compileTopLevel()) return false;
        emitter->emit(Op::Halt);
        mainChunk.numLocals = (int)locals.size();
        out["__program__"] = mainChunk;
        for (auto& fn : funcs) {
            out[fn.first] = std::move(fn.second);
        }
        return true;
    }

private:
    std::vector<Token> toks;
    size_t pos = 0;
    std::string* err;

    Chunk mainChunk;
    Emitter mainEmitter;
    Chunk* curChunk = nullptr;
    Emitter* emitter = nullptr;

    std::map<std::string, Chunk> funcs;

    std::vector<std::string> locals;
    std::map<std::string, size_t> localIndex;

    std::vector<int32_t> loopBreakStack;
    std::vector<size_t> loopContStack;

    const Token& peek(size_t ahead = 0) const {
        return toks[std::min(pos + ahead, toks.size() - 1)];
    }
    const Token& next() { return toks[std::min(pos++, toks.size() - 1)]; }
    bool matchKeyword(const char* kw) {
        if (peek().kind == Token::Kind::Keyword && peek().text == kw) { next(); return true; }
        return false;
    }
    bool matchPunct(const char* p) {
        if (peek().kind == Token::Kind::Punct && peek().text == p) { next(); return true; }
        return false;
    }
    bool acceptPunct(const char* p) { return matchPunct(p); }
    bool isPunct(const char* p) const {
        return peek().kind == Token::Kind::Punct && peek().text == p;
    }
    void errorf(const std::string& m) { if (err && err->empty()) *err = m; }

    bool compileTopLevel() {
        while (peek().kind != Token::Kind::Eof) {
            if (!compileStmt()) return false;
        }
        return true;
    }

    bool compileBlockBody() {
        // Called when '{' was already consumed; compiles until matching '}'.
        while (peek().kind != Token::Kind::Eof && !isPunct("}")) {
            if (!compileStmt()) return false;
        }
        if (!acceptPunct("}")) {
            errorf("expected '}' to end block");
            return false;
        }
        return true;
    }

    bool compileStmt() {
        const Token& t = peek();
        if (t.kind == Token::Kind::Keyword) {
            if (t.text == "func") {
                next();
                if (peek().kind != Token::Kind::Ident) {
                    errorf("expected function name");
                    return false;
                }
                std::string fname = next().text;
                if (!acceptPunct("(")) { errorf("expected '(' after function name"); return false; }
                std::vector<std::string> params;
                if (!isPunct(")")) {
                    do {
                        if (peek().kind != Token::Kind::Ident) {
                            errorf("expected parameter name");
                            return false;
                        }
                        params.push_back(next().text);
                    } while (acceptPunct(","));
                }
                if (!acceptPunct(")")) { errorf("expected ')' in function params"); return false; }
                int fcol = peek().col;
                bool colonForm = acceptPunct(":");
                if (!colonForm && !acceptPunct("{")) { errorf("expected '{' or ':' for function body"); return false; }
                Chunk fnChunk;
                fnChunk.params = params;
                Emitter fnEmitter;
                fnEmitter.bind(fnChunk);
                curChunk = &fnChunk;
                emitter = &fnEmitter;
                locals = params;
                localIndex.clear();
                for (size_t i = 0; i < params.size(); ++i) localIndex[params[i]] = i;
                if (colonForm) {
                    if (!compileIndentBlock(fcol)) return false;
                } else if (!compileBlockBody()) return false;
                emitter->emit(Op::Halt);
                fnChunk.numLocals = (int)locals.size();
                funcs[fname] = std::move(fnChunk);
                // Restore enclosing context.
                curChunk = &mainChunk;
                emitter = &mainEmitter;
                return true;
            }
            if (t.text == "var") {
                next();
                if (peek().kind != Token::Kind::Ident) {
                    errorf("expected variable name after 'var'");
                    return false;
                }
                std::string name = next().text;
                if (!acceptPunct("=")) {
                    errorf("expected '=' after variable name");
                    return false;
                }
                if (!compileExpr()) return false;
                if (isMainContext()) {
                    // Top level: variables are globals (readable across runs).
                    emitter->emit(Op::StoreGlobal, emitter->addString(name));
                } else {
                    emitter->emit(Op::StoreLocal, localIndexOrCreate(name));
                }
                acceptPunct(";"); // semicolons optional
                return true;
            }
            if (t.text == "if") {
                int col = t.col;
                next();
                if (!compileExpr()) return false;
                int32_t jFalse = emitter->emitJump(Op::JumpIfFalse);
                if (!compileBlockAfterColon(col)) return false;
                std::vector<int32_t> elifJumps;
                while (matchKeyword("elif")) {
                    int ecol = peek().col;
                    if (!compileExpr()) return false;
                    int32_t j = emitter->emitJump(Op::JumpIfFalse);
                    if (!compileBlockAfterColon(ecol)) return false;
                    elifJumps.push_back(j);
                }
                if (matchKeyword("else")) {
                    int32_t jEnd = emitter->emitJump(Op::Jump);
                    emitter->patchJump(jFalse);
                    if (!compileBlockAfterColon(col)) return false;
                    emitter->patchJump(jEnd);
                } else {
                    emitter->patchJump(jFalse);
                }
                for (int32_t j : elifJumps) emitter->patchJump(j);
                return true;
            }
            if (t.text == "while") {
                int col = t.col;
                next();
                size_t loopStart = emitter->code_->size();
                loopContStack.push_back(loopStart);
                if (!compileExpr()) {
                    loopContStack.pop_back();
                    return false;
                }
                int32_t jFalse = emitter->emitJump(Op::JumpIfFalse);
                loopBreakStack.push_back(jFalse);
                if (!compileBlockAfterColon(col)) {
                    loopBreakStack.pop_back();
                    loopContStack.pop_back();
                    return false;
                }
                loopBreakStack.pop_back();
                emitter->emit(Op::Jump, (int32_t)loopStart);
                emitter->patchJump(jFalse);
                patchBreakJumps(jFalse);
                loopContStack.pop_back();
                return true;
            }
            if (t.text == "for") {
                int col = t.col;
                next();
                if (peek().kind != Token::Kind::Ident) {
                    errorf("expected loop variable name");
                    return false;
                }
                std::string var = next().text;
                size_t slot = localIndexOrCreate(var);
                if (!matchPunct("=")) {
                    errorf("expected '=' in for loop");
                    return false;
                }
                if (!compileExpr()) return false; // start value
                emitter->emit(Op::StoreLocal, slot); // i = start
                if (!matchKeyword("to")) {
                    errorf("expected 'to' in for loop");
                    return false;
                }
                size_t loopStart = emitter->code_->size();
                loopContStack.push_back(loopStart);
                // i < end ?
                emitter->emit(Op::LoadLocal, slot);
                if (!compileExpr()) {
                    loopContStack.pop_back();
                    return false;
                }
                emitter->emit(Op::Lt);
                int32_t jFalse = emitter->emitJump(Op::JumpIfFalse);
                loopBreakStack.push_back(jFalse);
                if (!compileBlockAfterColon(col)) {
                    loopBreakStack.pop_back();
                    loopContStack.pop_back();
                    return false;
                }
                loopBreakStack.pop_back();
                emitter->emit(Op::LoadLocal, slot);
                emitter->emit(Op::LoadConst, emitter->addConst(1.0));
                emitter->emit(Op::Add);
                emitter->emit(Op::StoreLocal, slot);
                emitter->emit(Op::Jump, (int32_t)loopStart);
                emitter->patchJump(jFalse);
                patchBreakJumps(jFalse);
                loopContStack.pop_back();
                return true;
            }
            if (t.text == "return") {
                next();
                if (isPunct(";") || isPunct("}") || peek().kind == Token::Kind::Eof) {
                    emitter->emit(Op::Ret);
                } else {
                    if (!compileExpr()) return false;
                    emitter->emit(Op::Ret);
                }
                acceptPunct(";");
                return true;
            }
            if (t.text == "break") {
                next();
                if (loopBreakStack.empty()) {
                    errorf("'break' outside of loop");
                    return false;
                }
                emitter->emit(Op::Jump, loopBreakStack.back());
                acceptPunct(";");
                return true;
            }
            if (t.text == "continue") {
                next();
                if (loopContStack.empty()) {
                    errorf("'continue' outside of loop");
                    return false;
                }
                emitter->emit(Op::Jump, (int32_t)loopContStack.back());
                acceptPunct(";");
                return true;
            }
            if (t.text == "true" || t.text == "false" || t.text == "nil") {
                if (!compileExpr()) return false;
                acceptPunct(";");
                return true;
            }
        }
        // Assignment or expression statement.
        if (peek().kind == Token::Kind::Ident &&
            (peek(1).kind == Token::Kind::Punct &&
             (peek(1).text == "=" || peek(1).text == "."))) {
            std::string lhs = next().text;
            if (isPunct(".")) {
                next();
                std::string member = next().text;
                if (!matchPunct("=")) {
                    errorf("expected '=' after member name");
                    return false;
                }
                if (!compileExpr()) return false;
                emitLoadName(lhs); // stack order: [value, node] for SetProp
                emitter->emit(Op::SetProp, emitter->addString(member));
                acceptPunct(";");
                return true;
            }
            if (matchPunct("=")) {
                if (!compileExpr()) return false;
                emitStoreName(lhs);
                acceptPunct(";");
                return true;
            }
            errorf("expected '=' or '.' after variable name");
            return false;
        }
        if (!compileExpr()) return false;
        acceptPunct(";");
        return true;
    }

    // Break statements emit Jump with the enclosing JumpIfFalse's index as
    // operand. Once that condition jump is patched, redirect the break jumps
    // to the same target.
    void patchBreakJumps(int32_t jFalseIdx) {
        int32_t target = emitter->code_->at(jFalseIdx).a;
        for (auto& i : *emitter->code_) {
            if (i.op == Op::Jump && i.a == jFalseIdx) i.a = target;
        }
    }

    bool compileBlockOrStmt() {
        if (acceptPunct("{")) return compileBlockBody();
        return compileStmt();
    }

    // Indentation block: statements after a ':' opener whose column is greater
    // than the opener's column. Stops at dedent, '}', or EOF.
    bool compileIndentBlock(int openerCol) {
        while (true) {
            const Token& t = peek();
            if (t.kind == Token::Kind::Eof) break;
            if (isPunct("}")) break;
            if (t.col <= openerCol) break;
            if (!compileStmt()) return false;
        }
        return true;
    }

    // Block after a condition/param list: braces, colon+indent, or single stmt.
    bool compileBlockAfterColon(int openerCol) {
        if (acceptPunct(":")) return compileIndentBlock(openerCol);
        return compileBlockOrStmt();
    }

    // Name resolution: locals declared via var/params, otherwise globals.
    // At the top level (program chunk) everything is global, so results are
    // readable across script runs.
    bool isMainContext() const { return curChunk == &mainChunk; }
    void emitLoadName(const std::string& name) {
        if (isMainContext()) {
            emitter->emit(Op::LoadGlobal, emitter->addString(name));
            return;
        }
        auto it = localIndex.find(name);
        if (it != localIndex.end()) {
            emitter->emit(Op::LoadLocal, (int32_t)it->second);
        } else {
            emitter->emit(Op::LoadGlobal, emitter->addString(name));
        }
    }
    void emitStoreName(const std::string& name) {
        if (isMainContext()) {
            emitter->emit(Op::StoreGlobal, emitter->addString(name));
            return;
        }
        auto it = localIndex.find(name);
        if (it != localIndex.end()) {
            emitter->emit(Op::StoreLocal, (int32_t)it->second);
        } else {
            emitter->emit(Op::StoreGlobal, emitter->addString(name));
        }
    }
    size_t localIndexOrCreate(const std::string& name) {
        auto it = localIndex.find(name);
        if (it != localIndex.end()) return it->second;
        locals.push_back(name);
        size_t idx = locals.size() - 1;
        localIndex[name] = idx;
        return idx;
    }

    // Expression grammar (precedence climbing).
    bool compileExpr() { return compileOr(); }
    bool compileOr() {
        if (!compileAnd()) return false;
        while (matchKeyword("or")) {
            if (!compileAnd()) return false;
            emitter->emit(Op::Or);
        }
        return true;
    }
    bool compileAnd() {
        if (!compileNot()) return false;
        while (matchKeyword("and")) {
            if (!compileNot()) return false;
            emitter->emit(Op::And);
        }
        return true;
    }
    bool compileNot() {
        if (matchKeyword("not")) {
            if (!compileNot()) return false;
            emitter->emit(Op::Not);
            return true;
        }
        return compileCmp();
    }
    bool compileCmp() {
        if (!compileAdd()) return false;
        while (peek().kind == Token::Kind::Punct) {
            std::string p = peek().text;
            Op op = Op::Nop;
            if (p == "==") op = Op::Eq;
            else if (p == "!=") op = Op::Neq;
            else if (p == "<") op = Op::Lt;
            else if (p == "<=") op = Op::Le;
            else if (p == ">") op = Op::Gt;
            else if (p == ">=") op = Op::Ge;
            if (op != Op::Nop) {
                next();
                if (!compileAdd()) return false;
                emitter->emit(op);
            } else {
                break;
            }
        }
        return true;
    }
    bool compileAdd() {
        if (!compileMul()) return false;
        while (peek().kind == Token::Kind::Punct &&
               (peek().text == "+" || peek().text == "-")) {
            std::string op = next().text;
            if (!compileMul()) return false;
            emitter->emit(op == "+" ? Op::Add : Op::Sub);
        }
        return true;
    }
    bool compileMul() {
        if (!compileUnary()) return false;
        while (peek().kind == Token::Kind::Punct &&
               (peek().text == "*" || peek().text == "/" || peek().text == "%")) {
            std::string op = next().text;
            if (!compileUnary()) return false;
            emitter->emit(op == "*" ? Op::Mul : op == "/" ? Op::Div : Op::Mod);
        }
        return true;
    }
    bool compileUnary() {
        if (peek().kind == Token::Kind::Punct && peek().text == "-") {
            next();
            if (!compileUnary()) return false;
            emitter->emit(Op::LoadConst, emitter->addConst(0.0));
            emitter->emit(Op::Sub);
            return true;
        }
        return compilePrimary();
    }
    bool compilePrimary() {
        const Token& t = peek();
        if (t.kind == Token::Kind::Number) {
            next();
            emitter->emit(Op::LoadConst, emitter->addConst(t.num));
            return true;
        }
        if (t.kind == Token::Kind::String) {
            next();
            emitter->emit(Op::LoadString, emitter->addString(t.text));
            return true;
        }
        if (t.kind == Token::Kind::Keyword) {
            if (t.text == "true") { next(); emitter->emit(Op::LoadConst, emitter->addConst(1.0)); return true; }
            if (t.text == "false") { next(); emitter->emit(Op::LoadConst, emitter->addConst(0.0)); return true; }
            if (t.text == "nil") { next(); emitter->emit(Op::LoadConst, emitter->addConst(0.0)); return true; }
        }
        if (matchPunct("(")) {
            if (!compileExpr()) return false;
            if (!acceptPunct(")")) { errorf("expected ')'"); return false; }
            return true;
        }
        if (t.kind == Token::Kind::Ident) {
            next();
            std::string name = t.text;
            if (acceptPunct("(")) {
                int argc = 0;
                if (!isPunct(")")) {
                    do {
                        if (!compileExpr()) return false;
                        ++argc;
                    } while (acceptPunct(","));
                }
                if (!acceptPunct(")")) { errorf("expected ')' after arguments"); return false; }
                emitter->emit(Op::Call, emitter->addString(name), argc);
                return true;
            }
            emitLoadName(name);
            while (acceptPunct(".")) {
                if (peek().kind != Token::Kind::Ident) {
                    errorf("expected property name after '.'");
                    return false;
                }
                std::string member = next().text;
                emitter->emit(Op::GetProp, emitter->addString(member));
            }
            return true;
        }
        errorf(std::string("unexpected token in expression: kind=") +
               std::to_string((int)t.kind) + " text='" + t.text + "'");
        return false;
    }
};

} // namespace

bool NovaScriptCompiler::compile(const std::string& source, NovaScriptVM* vm, std::string* error) {
    if (error) error->clear();
    std::vector<Token> toks = tokenize(source, error);
    if (error && !error->empty()) return false;
    std::map<std::string, Chunk> program;
    Parser parser(std::move(toks), error);
    if (!parser.parse(program)) {
        if (error && error->empty()) *error = "syntax error";
        return false;
    }
    vm->installFunctions(std::move(program));
    return true;
}

} // namespace nova
