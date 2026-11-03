// Nova Engine — minimal filesystem helpers implementation.
#include "nova/util/fs.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

namespace nova::fs {

bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool writeFile(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(data.data(), (std::streamsize)data.size());
    return out.good();
}

bool exists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

bool isDirectory(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool makeDirectories(const std::string& path) {
    if (path.empty()) return false;
    std::string cur;
    size_t start = 0;
    if (path[0] == '/') { cur = "/"; start = 1; }
    size_t pos = start;
    while (pos <= path.size()) {
        if (pos == path.size() || path[pos] == '/') {
            if (pos > start) {
                std::string segment = path.substr(start, pos - start);
                if (cur.empty() || cur.back() != '/') cur += "/";
                cur += segment;
                if (!isDirectory(cur)) {
                    if (::mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST) return false;
                }
            }
            if (pos == path.size()) break;
            start = pos + 1;
        }
        ++pos;
    }
    return isDirectory(path);
}

bool removeFile(const std::string& path) { return ::unlink(path.c_str()) == 0; }

bool rename(const std::string& from, const std::string& to) {
    return ::rename(from.c_str(), to.c_str()) == 0;
}

bool copyFile(const std::string& from, const std::string& to) {
    std::string data;
    if (!readFile(from, data)) return false;
    return writeFile(to, data);
}

bool removeDirectory(const std::string& path) {
    if (!isDirectory(path)) return removeFile(path);
    DIR* d = ::opendir(path.c_str());
    if (!d) return false;
    struct dirent* e;
    bool ok = true;
    while ((e = ::readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string child = join(path, name);
        if (isDirectory(child)) ok = removeDirectory(child) && ok;
        else ok = removeFile(child) && ok;
    }
    ::closedir(d);
    if (ok) ok = ::rmdir(path.c_str()) == 0;
    return ok;
}

std::string join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return (a.back() == '/') ? a + b : a + "/" + b;
}

std::string dirName(const std::string& path) {
    size_t p = path.find_last_of('/');
    if (p == std::string::npos) return "";
    if (p == 0) return "/";
    return path.substr(0, p);
}

std::string baseName(const std::string& path) {
    size_t p = path.find_last_of('/');
    return p == std::string::npos ? path : path.substr(p + 1);
}

std::string extension(const std::string& path) {
    std::string base = baseName(path);
    size_t p = base.find_last_of('.');
    if (p == std::string::npos) return "";
    return base.substr(p + 1);
}

std::string stem(const std::string& path) {
    std::string base = baseName(path);
    size_t p = base.find_last_of('.');
    return p == std::string::npos ? base : base.substr(0, p);
}

std::vector<std::string> listDirectory(const std::string& path) {
    std::vector<std::string> out;
    DIR* d = ::opendir(path.c_str());
    if (!d) return out;
    struct dirent* e;
    while ((e = ::readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        out.push_back(name);
    }
    ::closedir(d);
    return out;
}

} // namespace nova::fs
