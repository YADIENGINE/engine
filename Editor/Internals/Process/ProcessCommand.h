#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct ProcessCommand {
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::filesystem::path directory;
    std::map<std::string, std::string> environment;
};
