#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Target {
    std::string name;
    std::vector<std::string> dependencies;
    std::vector<std::string> commands;
    bool always = false;
};

static std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static std::vector<std::string> words(const std::string& s) {
    std::istringstream in(s);
    std::vector<std::string> out;
    std::string word;
    while (in >> word) out.push_back(word);
    return out;
}

class BuildFile {
public:
    std::map<std::string, std::string> variables;
    std::map<std::string, Target> targets;
    std::vector<std::string> order;

    bool load(const fs::path& path) {
        std::ifstream file(path);
        if (!file) {
            std::cerr << "ycomp: cannot open " << path.string() << '\n';
            return false;
        }
        Target* current = nullptr;
        std::string line;
        int lineNo = 0;
        while (std::getline(file, line)) {
            ++lineNo;
            if (trim(line).empty() || trim(line).rfind("#", 0) == 0) continue;
            const bool indented = !line.empty() && (line[0] == ' ' || line[0] == '\t');
            if (indented) {
                if (!current) {
                    std::cerr << path.string() << ':' << lineNo << ": recipe without a target\n";
                    return false;
                }
                current->commands.push_back(trim(line));
                continue;
            }
            current = nullptr;
            auto arrow = line.find("->");
            if (arrow != std::string::npos) {
                std::string key = trim(line.substr(0, arrow));
                std::string value = trim(line.substr(arrow + 2));
                if (key.empty()) {
                    std::cerr << path.string() << ':' << lineNo << ": empty variable name\n";
                    return false;
                }
                variables[key] = value;
                continue;
            }
            auto gt = line.find('>');
            if (gt == std::string::npos) {
                std::cerr << path.string() << ':' << lineNo << ": expected '->' or '>'\n";
                return false;
            }
            std::string lhs = trim(line.substr(0, gt));
            std::string rhs = trim(line.substr(gt + 1));
            bool always = false;
            if (lhs.rfind("!$", 0) == 0) {
                always = true;
                lhs = trim(lhs.substr(2));
            } else if (!lhs.empty() && lhs[0] == '!') {
                always = true;
                lhs = trim(lhs.substr(1));
            }
            if (lhs.empty()) {
                std::cerr << path.string() << ':' << lineNo << ": empty target name\n";
                return false;
            }
            if (!targets.count(lhs)) order.push_back(lhs);
            Target t;
            t.name = lhs;
            t.dependencies = words(expand(rhs));
            t.always = always;
            targets[lhs] = std::move(t);
            current = &targets[lhs];
        }
        return true;
    }

    std::string expand(const std::string& input) const {
        std::string out;
        for (std::size_t i = 0; i < input.size();) {
            if (input.compare(i, 2, "!#") == 0 && i + 2 < input.size() && input[i + 2] == '(') {
                auto end = input.find(')', i + 3);
                if (end == std::string::npos) {
                    out += input.substr(i);
                    break;
                }
                std::string key = input.substr(i + 3, end - (i + 3));
                auto it = variables.find(key);
                if (it != variables.end()) out += it->second;
                i = end + 1;
            } else {
                out += input[i++];
            }
        }
        return out;
    }
};

class Builder {
    BuildFile& build;
    std::set<std::string> visiting;
    std::set<std::string> completed;

    static bool newerThan(const fs::path& output, const fs::path& input) {
        std::error_code ec;
        if (!fs::exists(output, ec) || !fs::exists(input, ec)) return true;
        const auto outputTime = fs::last_write_time(output, ec);
        if (ec) return true;
        const auto inputTime = fs::last_write_time(input, ec);
        if (ec) return true;
        return inputTime > outputTime;
    }

public:
    explicit Builder(BuildFile& b) : build(b) {}

    bool run(const std::string& name) {
        if (completed.count(name)) return true;
        auto it = build.targets.find(name);
        if (it == build.targets.end()) {
            if (!fs::exists(name)) {
                std::cerr << "ycomp: no rule to make target '" << name << "'\n";
                return false;
            }
            return true;
        }
        if (visiting.count(name)) {
            std::cerr << "ycomp: circular dependency involving '" << name << "'\n";
            return false;
        }
        visiting.insert(name);
        Target& target = it->second;
        bool dirty = target.always || !fs::exists(target.name);
        for (const auto& dep : target.dependencies) {
            if (!run(dep)) return false;
            if (fs::exists(target.name) && newerThan(target.name, dep)) dirty = true;
        }
        if (!target.commands.empty() && dirty) {
            for (const auto& command : target.commands) {
                const std::string expanded = build.expand(command);
                std::cout << expanded << '\n';
                std::cout.flush();
                if (std::system(expanded.c_str()) != 0) {
                    std::cerr << "ycomp: command failed for target '" << name << "'\n";
                    return false;
                }
            }
        } else if (!target.commands.empty() && !dirty) {
            std::cout << "ycomp: '" << name << "' is up to date\n";
        }
        visiting.erase(name);
        completed.insert(name);
        return true;
    }
};

int main(int argc, char** argv) {
    fs::path file = "CompilFileY.compy";
    std::string target;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-f" || arg == "--file") {
            if (++i >= argc) {
                std::cerr << "usage: ycomp [-f buildfile] [target]\n";
                return 2;
            }
            file = argv[i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "YComp - a small, make-like build tool\n"
                         "usage: ycomp [-f buildfile] [target]\n"
                         "default build file: CompilFileY.compy\n";
            return 0;
        } else if (target.empty()) {
            target = arg;
        } else {
            std::cerr << "ycomp: unexpected argument '" << arg << "'\n";
            return 2;
        }
    }

    BuildFile build;
    if (!build.load(file)) return 2;
    if (build.order.empty()) {
        std::cerr << "ycomp: build file contains no targets\n";
        return 2;
    }
    if (target.empty()) target = build.order.front();

    Builder builder(build);
    return builder.run(target) ? 0 : 1;
}
