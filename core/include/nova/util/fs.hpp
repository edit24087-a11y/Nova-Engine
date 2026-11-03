// Nova Engine — minimal filesystem helpers (C++17).
#pragma once
#include <string>
#include <vector>
#include <functional>

namespace nova::fs {

bool readFile(const std::string& path, std::string& out);
bool writeFile(const std::string& path, const std::string& data);
bool exists(const std::string& path);
bool isDirectory(const std::string& path);
bool makeDirectories(const std::string& path);
bool removeFile(const std::string& path);
bool removeDirectory(const std::string& path); // recursive
bool rename(const std::string& from, const std::string& to);
std::string join(const std::string& a, const std::string& b);
std::string dirName(const std::string& path);
std::string baseName(const std::string& path);
std::string extension(const std::string& path);
std::string stem(const std::string& path);
std::vector<std::string> listDirectory(const std::string& path); // names only
bool copyFile(const std::string& from, const std::string& to);

} // namespace nova::fs
