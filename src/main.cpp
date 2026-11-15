#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Target {
    std::string name;
    std::vector<std::string> dependencies;
    std::vector<std::string> commands;
    bool always = false;
};

static std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

static std::vector<std::string> words(const std::string& value) {
    std::istringstream input(value);
    std::vector<std::string> result;
    std::string word;
    while (input >> word) result.push_back(word);
    return result;
}

class BuildFile {
public:
    std::map<std::string, std::string> variables;
    std::map<std::string, Target> targets;
    std::vector<std::string> order;

    std::string expand(const std::string& input) const {
        std::string result;
        for (std::size_t i = 0; i < input.size();) {
            if (input.compare(i, 2, "!#") == 0 && i + 2 < input.size() && input[i + 2] == '(') {
                const auto end = input.find(')', i + 3);
                if (end == std::string::npos)
                    throw std::runtime_error("unterminated variable reference; expected '!#(NAME)'");
                const std::string key = input.substr(i + 3, end - i - 3);
                if (key.empty())
                    throw std::runtime_error("empty variable reference; expected '!#(NAME)'");
                const auto found = variables.find(key);
                if (found == variables.end())
                    throw std::runtime_error("undefined variable '" + key + "'");
                result += found->second;
                i = end + 1;
            } else {
                result += input[i++];
            }
        }
        return result;
    }

    bool load(const fs::path& path) {
        std::ifstream file(path);
        if (!file) {
            std::cerr << "ycomp: cannot open build file '" << path.string() << "'\n";
            return false;
        }

        Target* current = nullptr;
        std::string line;
        int line_number = 0;
        while (std::getline(file, line)) {
            ++line_number;
            const std::string clean = trim(line);
            if (clean.empty() || clean.rfind("#", 0) == 0) continue;

            const bool indented = !line.empty() && (line[0] == ' ' || line[0] == '\t');
            if (indented) {
                if (!current) {
                    std::cerr << path.string() << ':' << line_number
                              << ": recipe without a target\n";
                    return false;
                }
                current->commands.push_back(clean);
                continue;
            }

            current = nullptr;
            const auto arrow = line.find("->");
            if (arrow != std::string::npos) {
                const std::string key = trim(line.substr(0, arrow));
                const std::string value = trim(line.substr(arrow + 2));
                if (key.empty()) {
                    std::cerr << path.string() << ':' << line_number << ": empty variable name\n";
                    return false;
                }
                variables[key] = value;
                continue;
            }

            const auto separator = line.find('>');
            if (separator == std::string::npos) {
                std::cerr << path.string() << ':' << line_number
                          << ": expected '->' or '>'\n";
                return false;
            }

            std::string name = trim(line.substr(0, separator));
            const std::string dependency_text = trim(line.substr(separator + 1));
            bool always = false;
            if (name.rfind("!$", 0) == 0) {
                always = true;
                name = trim(name.substr(2));
            } else if (!name.empty() && name.front() == '!') {
                std::cerr << path.string() << ':' << line_number
                          << ": invalid target modifier; use '!$' for always-build targets\n";
                return false;
            }
            if (name.empty()) {
                std::cerr << path.string() << ':' << line_number << ": empty target name\n";
                return false;
            }

            if (!targets.count(name)) order.push_back(name);
            Target target;
            target.name = name;
            try {
                target.dependencies = words(expand(dependency_text));
            } catch (const std::exception& error) {
                std::cerr << path.string() << ':' << line_number << ": " << error.what() << '\n';
                return false;
            }
            target.always = always;
            targets[name] = std::move(target);
            current = &targets[name];
        }
        return true;
    }
};

class Builder {
    BuildFile& build;
    std::set<std::string> visiting;
    std::set<std::string> completed;

    static bool newer_than(const fs::path& output, const fs::path& input) {
        std::error_code error;
        if (!fs::exists(output, error) || error) return true;
        if (!fs::exists(input, error) || error) return true;
        const auto output_time = fs::last_write_time(output, error);
        if (error) return true;
        const auto input_time = fs::last_write_time(input, error);
        if (error) return true;
        return input_time > output_time;
    }

public:
    explicit Builder(BuildFile& build_file) : build(build_file) {}

    bool run(const std::string& name) {
        if (completed.count(name)) return true;

        const auto found = build.targets.find(name);
        if (found == build.targets.end()) {
            std::error_code error;
            if (!fs::exists(name, error) || error) {
                std::cerr << "ycomp: no rule to make target '" << name << "'\n";
                return false;
            }
            completed.insert(name);
            return true;
        }

        if (visiting.count(name)) {
            std::cerr << "ycomp: circular dependency involving '" << name << "'\n";
            return false;
        }

        visiting.insert(name);
        Target& target = found->second;
        bool dirty = target.always || !fs::exists(target.name);
        for (const auto& dependency : target.dependencies) {
            if (!run(dependency)) return false;
            if (fs::exists(target.name) && newer_than(target.name, dependency)) dirty = true;
        }

        if (!target.commands.empty() && dirty) {
            for (const auto& command : target.commands) {
                std::string expanded;
                try {
                    expanded = build.expand(command);
                } catch (const std::exception& error) {
                    std::cerr << "ycomp: target '" << name << "': " << error.what() << '\n';
                    return false;
                }
                std::cout << expanded << '\n';
                std::cout.flush();
                if (std::system(expanded.c_str()) != 0) {
                    std::cerr << "ycomp: command failed while building target '" << name << "'\n";
                    return false;
                }
            }
        } else if (!target.commands.empty()) {
            std::cout << "ycomp: '" << name << "' is up to date\n";
        }

        visiting.erase(name);
        completed.insert(name);
        return true;
    }
};

int main(int argc, char** argv) {
    fs::path build_file = "CompilFileY.compy";
    std::string target;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "-f" || argument == "--file") {
            if (++i >= argc) {
                std::cerr << "usage: ycomp [-f buildfile] [target]\n";
                return 2;
            }
            build_file = argv[i];
        } else if (argument == "-h" || argument == "--help") {
            std::cout << "YComp - a small, make-like build tool\n"
                         "usage: ycomp [-f buildfile] [target]\n"
                         "default build file: CompilFileY.compy\n";
            return 0;
        } else if (target.empty()) {
            target = argument;
        } else {
            std::cerr << "ycomp: unexpected argument '" << argument << "'\n";
            return 2;
        }
    }

    BuildFile build;
    if (!build.load(build_file)) return 2;
    if (build.order.empty()) {
        std::cerr << "ycomp: build file contains no targets\n";
        return 2;
    }
    if (target.empty()) target = build.order.front();

    Builder builder(build);
    return builder.run(target) ? 0 : 1;
}
