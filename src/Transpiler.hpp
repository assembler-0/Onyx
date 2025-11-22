#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <regex>
#include <filesystem>
#include <deque>

namespace onyx {

struct TranspilerConfig {
    bool verbose = false;
    bool keep_comments = true;
};

class Transpiler {
public:
    explicit Transpiler(TranspilerConfig config = {});
    
    bool processFile(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath);

private:
    TranspilerConfig m_config;
    std::string m_source;
    std::deque<std::string> m_outputLines;
    
    // Symbol Table
    struct SymbolTable {
        std::vector<std::unordered_map<std::string, std::string>> scopes;
        void pushScope() { scopes.emplace_back(); }
        void popScope() { if(!scopes.empty()) scopes.pop_back(); }
        void add(const std::string& name, const std::string& type) {
            if(!scopes.empty()) scopes.back()[name] = type;
        }
        std::string lookup(const std::string& name) {
            for(auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
                if(it->count(name)) return it->at(name);
            }
            return "";
        }
    } m_symbols;

    // Context State
    std::unordered_map<std::string, std::string> m_sharedMixins; 
    std::string m_currentResolveType; 
    int m_braceDepth = 0;
    
    // Parsing State
    std::string m_pendingAttribute;
    std::string m_currentStructAttribute;
    bool m_inShared = false;
    int m_sharedStartDepth = 0;
    bool m_inStruct = false;
    int m_structStartDepth = 0;
    std::string m_currentStructName;
    
    // Passes
    void loadFile(const std::filesystem::path& path);
    void pass1_Discovery(const std::string& content); 
    void pass2_Transpilation(const std::string& content); 
    
    // Helpers
    std::string translateType(const std::string& onyxType);
    
    // Line Processors
    std::string processLine(const std::string& line);
    std::string replaceMethodCalls(std::string line);
    std::string replacePipeOperators(std::string line);
};

} // namespace onyx
