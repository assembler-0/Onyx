#include "Transpiler.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <set>

namespace onyx {

static const std::regex R_COMMENT(R"(^\s*#)");
static const std::regex R_INCLUDE(R"(^\s*@include\s+\"([^\"]+)\")");
static const std::regex R_SHARED_START(R"(^\s*shared\s+(\w+)\s*[{])");
static const std::regex R_STRUCT_START(R"(^\s*(?:@\[(.*)\]\s*)?struct\s+(\w+)\s*[{])");
static const std::regex R_ATTRIBUTE_LINE(R"(^\s*@\[(.*)\]\s*$)"); // Handle standalone attributes
static const std::regex R_USE_MIXIN(R"(^\s*use\s+(\w+))");
static const std::regex R_FIELD(R"(^\s*(\w+)\s*:\s*([\w\*]+)\s*)");
static const std::regex R_RESOLVE_START(R"(^\s*resolve\s+(\w+)\s*[{])");
static const std::regex R_FUNC_SIGNATURE(R"(^\s*(?:@\[(.*)\]\s*)?(?:(inline|extern|static)\s+)?fn\s+(\w+)\s*\((.*)\)\s*->\s*([\w\*]+)\s*)");
static const std::regex R_VAR(R"(^\s*var\s+(?:(volatile|register|const)\s+)?(\w+)\s*:\s*([\w\*]+)\s*(?:=\s*(.*))?)");
static const std::regex R_IF(R"(^\s*if\s+(.*)\s*[{])");
static const std::regex R_WHILE(R"(^\s*while\s+(.*)\s*[{])");
static const std::regex R_LOOP(R"(^\s*loop\s*[{])");
static const std::regex R_NATIVE_BLOCK(R"(^\s*native\s*[{])");

// Implementation

Transpiler::Transpiler(TranspilerConfig config) : m_config(config) {}

bool Transpiler::processFile(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath) {
    loadFile(inputPath);
    if (m_source.empty()) return false;

    try {
        pass1_Discovery(m_source);
        
        m_symbols.scopes.clear();
        m_symbols.pushScope(); 
        m_braceDepth = 0;
        m_currentResolveType = "";
        m_inShared = false;
        m_inStruct = false;
        m_pendingAttribute = "";
        m_currentStructAttribute = "";
        
        pass2_Transpilation(m_source);
        
        std::ofstream out(outputPath);
        if (!out) return false;
        
        out << "// transpiled from " << inputPath.filename().string() << "\n";
        // Implicit includes removed per user request
        
        for (const auto& line : m_outputLines) {
            out << line << "\n";
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "transpilation Error: " << e.what() << "\n";
        return false;
    }
}

void Transpiler::loadFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) return;
    std::stringstream buffer;
    buffer << in.rdbuf();
    m_source = buffer.str();
}

void Transpiler::pass1_Discovery(const std::string& content) {
    std::stringstream ss(content);
    std::string line;
    std::string currentMixinName;
    std::string currentMixinBody;
    bool insideMixin = false;
    int depth = 0;

    while (std::getline(ss, line)) {
        std::smatch match;
        if (!insideMixin && std::regex_search(line, match, R_SHARED_START)) {
            insideMixin = true;
            currentMixinName = match[1].str();
            currentMixinBody = "";
            depth = 1;
            continue;
        }

        if (insideMixin) {
            for (char c : line) {
                if (c == '{') depth++;
                else if (c == '}') depth--;
            }
            if (depth == 0) {
                insideMixin = false;
                m_sharedMixins[currentMixinName] = currentMixinBody;
            } else {
                currentMixinBody += line + "\n";
            }
        }
    }
}

void Transpiler::pass2_Transpilation(const std::string& content) {
    std::stringstream ss(content);
    std::string line;
    bool insideNative = false;

    while (std::getline(ss, line)) {
        if (std::regex_search(line, R_NATIVE_BLOCK)) {
            insideNative = true;
            // Check if it closes on same line
            if (line.find('}') != std::string::npos) {
                insideNative = false;
                // Extract content: native { content } -> content
                std::string raw = line;
                raw = std::regex_replace(raw, R_NATIVE_BLOCK, ""); // Removes start
                size_t endBrace = raw.rfind('}');
                if (endBrace != std::string::npos) raw = raw.substr(0, endBrace);
                
                // Apply indentation for native block line
                int indent = m_braceDepth;
                std::string indentation = std::string(indent * 4, ' ');
                m_outputLines.push_back(indentation + raw);
            }
            continue; // Don't print 'native {'
        }
        if (insideNative) {
            if (line.find('}') != std::string::npos) {
                insideNative = false;
            } else {
                int nativeIndentDepth = m_braceDepth;
                std::string nativeIndentation = std::string(nativeIndentDepth * 4, ' ');
                std::string trimmedLine = std::regex_replace(line, std::regex(R"(^\s+)"), "");
                m_outputLines.push_back(nativeIndentation + trimmedLine);
            }
            continue;
        }

        if (std::regex_search(line, R_COMMENT)) {
            int indent = m_braceDepth;
            // Check if line starts with '}' to dedent? Comments usually follow context.
            // But simple approach:
            std::string indentation = std::string(indent * 4, ' ');
            if (m_config.keep_comments) {
                 // Trim original line to avoid double indent
                 std::string trimmed = std::regex_replace(line, std::regex(R"(^\s+)"), "");
                 m_outputLines.push_back(indentation + std::regex_replace(trimmed, R_COMMENT, "//"));
            }
            continue;
        }
        
        // Attribute lines (capture)
        std::smatch attrMatch;
        if (std::regex_search(line, attrMatch, R_ATTRIBUTE_LINE)) {
             if (!m_pendingAttribute.empty()) m_pendingAttribute += ", ";
             m_pendingAttribute += attrMatch[1].str();
             continue;
        }
        
        std::string processed = processLine(line);
        if (!processed.empty()) m_outputLines.push_back(processed);
    }
}

std::string Transpiler::processLine(const std::string& line) {
    // Trim leading whitespace
    std::string out = std::regex_replace(line, std::regex(R"(^\s+)"), "");
    std::smatch match;
    
    // Handle Includes
    if (std::regex_search(out, match, R_INCLUDE)) {
        // @include "..." -> #include "..."
        // Typically includes are at top level, braceDepth 0.
        return "#include \"" + match[1].str() + "\"";
    }

    // Strict brace detection
    bool opensBrace = std::regex_search(out, std::regex(R"(\{\s*$)")) || std::regex_search(out, std::regex(R"(^\s*\{)"));
    bool startsClosing = std::regex_search(out, std::regex(R"(^\})"));
    bool closesBrace = std::regex_search(out, std::regex(R"(^\s*\})")); // Same as startsClosing for now

    // Calculate Indentation
    int printDepth = m_braceDepth;
    if (startsClosing && printDepth > 0) printDepth--;

    // Un-indent for resolve blocks
    if (!m_currentResolveType.empty() && printDepth > 0) {
        printDepth--;
    }

    std::string indentation = std::string(printDepth * 4, ' ');

    // Replace self. with self->
    if (out.find("self.") != std::string::npos) {
        out = std::regex_replace(out, std::regex("self\\."), "self->");
    }
    
    // --- Closing Contexts ---
    if (closesBrace) {
        m_braceDepth--;
        
        if (m_inShared && m_braceDepth == m_sharedStartDepth) {
            m_inShared = false;
            if (m_braceDepth >= 0) m_symbols.popScope();
            return ""; // Don't print closing brace for shared mixin
        }
        if (m_inStruct && m_braceDepth == m_structStartDepth) {
            m_inStruct = false;
            out = "} " + m_currentStructName;
            if (!m_currentStructAttribute.empty()) {
                out += " __attribute__((" + m_currentStructAttribute + "))";
                m_currentStructAttribute = "";
            }
            out += ";";
            return indentation + out;
        }
        
        // Resolve Block End
        if (!m_currentResolveType.empty() && m_braceDepth == 0) {
             m_currentResolveType = "";
             if (m_braceDepth >= 0) m_symbols.popScope();
             return indentation + "// end resolve";
        }
        
        if (m_braceDepth >= 0) m_symbols.popScope();
    }

    if (std::regex_search(out, match, R_SHARED_START)) {
        m_inShared = true;
        m_sharedStartDepth = m_braceDepth;
        std::string attrMsg = "";
        if (!m_pendingAttribute.empty()) {
            attrMsg = " [attributes: " + m_pendingAttribute + "]";
            m_pendingAttribute = ""; // Consumed
        }
        if (opensBrace) m_braceDepth++;
        return indentation + "// shared " + match[1].str() + " (elided)" + attrMsg;
    }
    if (m_inShared) {
        if (opensBrace) m_braceDepth++;
        return ""; 
    }

    if (std::regex_search(out, match, R_STRUCT_START)) {
        m_inStruct = true;
        m_structStartDepth = m_braceDepth;
        // Group 1: Attr, Group 2: Name
        std::string attr = match[1].str();
        if (attr.empty()) attr = m_pendingAttribute;
        else if (!m_pendingAttribute.empty()) attr = m_pendingAttribute + ", " + attr;
        m_pendingAttribute = ""; // Consumed
        m_currentStructAttribute = attr;

        m_currentStructName = match[2].str();
        if (opensBrace) m_braceDepth++;
        return indentation + "typedef struct " + m_currentStructName + " {";
    }
    
    if (std::regex_search(out, match, R_USE_MIXIN)) {
        std::string mixinName = match[1].str();
        if (m_sharedMixins.count(mixinName)) {
            std::stringstream bodySS(m_sharedMixins[mixinName]);
            std::string bodyLine, injected;
            while(std::getline(bodySS, bodyLine)) {
                 if (bodyLine.empty()) continue;
                 std::smatch fieldMatch;
                 if (std::regex_search(bodyLine, fieldMatch, R_FIELD)) {
                     injected += indentation + translateType(fieldMatch[2].str()) + " " + fieldMatch[1].str() + ";\n";
                 }
            }
            // R_USE_MIXIN doesn't open braces.
            return injected;
        }
    }

    if (m_inStruct && !std::regex_search(out, R_FUNC_SIGNATURE) && !closesBrace) { 
        if (std::regex_search(out, match, R_FIELD)) {
             return indentation + translateType(match[2].str()) + " " + match[1].str() + ";";
        }
    }

    if (std::regex_search(out, match, R_RESOLVE_START)) {
        m_currentResolveType = match[1].str();
        if (opensBrace) m_braceDepth++;
        return indentation + "// resolve " + m_currentResolveType;
    }

    if (std::regex_search(out, match, R_FUNC_SIGNATURE)) {
        bool isDefinition = line.find('{') != std::string::npos;

        // Common logic for both declaration and definition
        std::string attr = match[1].str();
        if (isDefinition) { // Only consume pending attributes for definitions for now
             if (attr.empty()) attr = m_pendingAttribute;
             else if (!m_pendingAttribute.empty()) attr = m_pendingAttribute + ", " + attr;
             m_pendingAttribute = "";
        }

        std::string mods = match[2].str();
        std::string name = match[3].str();
        std::string args = match[4].str();
        std::string ret = translateType(match[5].str());

        std::vector<std::string> cArgs;

        if (!m_currentResolveType.empty()) {
            name = m_currentResolveType + "_" + name;
            m_symbols.add("self", m_currentResolveType);
            cArgs.push_back(m_currentResolveType + "* self");
        }

        std::regex argSplit(R"(\s*,\s*)");
        std::sregex_token_iterator iter(args.begin(), args.end(), argSplit, -1);
        std::sregex_token_iterator end;
        for (; iter != end; ++iter) {
            std::string arg = *iter;
            if (arg.empty()) continue;
            std::smatch argMatch;
            static const std::regex R_ARG(R"((\w+)\s*:\s*([\w\*]+))");
            if (std::regex_search(arg, argMatch, R_ARG)) {
                std::string argName = argMatch[1].str();
                if (!m_currentResolveType.empty() && argName == "self") {
                    continue;
                }
                cArgs.push_back(translateType(argMatch[2].str()) + " " + argName);
                if(isDefinition) m_symbols.add(argName, translateType(argMatch[2].str()));
            }
        }
        std::string cArgStr;
        for (size_t i=0; i<cArgs.size(); ++i) {
            cArgStr += cArgs[i] + (i < cArgs.size()-1 ? ", " : "");
        }
        
        std::string attrPrefix = "";
        if (!attr.empty()) attrPrefix = "__attribute__((" + attr + ")) ";
        
        std::string result = indentation + attrPrefix + (mods.empty() ? "" : mods + " ") + ret + " " + name + "(" + cArgStr + ")";

        if (isDefinition) {
            if (opensBrace) m_braceDepth++;
            m_symbols.pushScope();
            return result + " {";
        } else {
            return result + ";";
        }
    }

    if (std::regex_search(out, match, R_VAR)) {
        std::string mods = match[1].str();
        std::string name = match[2].str();
        std::string type = translateType(match[3].str());
        std::string val = match[4].str();
        m_symbols.add(name, match[3].str()); // Store Original Type (Packet) for lookup

        std::string res = indentation + (mods.empty() ? "" : mods + " ") + type + " " + name;
        if (!val.empty()) {
            val = replaceMethodCalls(val);
            val = replacePipeOperators(val);
            res += " = " + val;
        }
        res += ";";
        return res;
    }

    out = replaceMethodCalls(out);
    out = replacePipeOperators(out);
    
    if (std::regex_search(out, match, R_IF)) {
        if (opensBrace) m_braceDepth++;
        return indentation + "if (" + match[1].str() + ") {";
    }
    if (std::regex_search(out, match, R_WHILE)) {
        if (opensBrace) m_braceDepth++;
        return indentation + "while (" + match[1].str() + ") {";
    }
    if (std::regex_search(out, match, R_LOOP)) {
        if (opensBrace) m_braceDepth++;
        return indentation + "while (1) {";
    }
    
    // Auto-semicolon for expressions
    bool isWhitespace = std::regex_match(out, std::regex(R"(^\s*$)"));
    bool isStatement = !out.empty() && !isWhitespace && !opensBrace && !closesBrace && out.back() != ';';
    if (isStatement) {
        out += ";";
    }

    if (opensBrace && !std::regex_search(out, R_SHARED_START) && !std::regex_search(out, R_STRUCT_START) && 
        !std::regex_search(out, R_RESOLVE_START) && !std::regex_search(out, R_FUNC_SIGNATURE) && 
        !std::regex_search(out, R_IF) && !std::regex_search(out, R_WHILE) && !std::regex_search(out, R_LOOP)) {
            m_braceDepth++;
    }
    
    // Fallback for generic lines (expressions, assignments, etc)
    // Ensure we don't return empty line if it was just braces
    if (out == "}" || out == "};") return indentation + out; 
    if (out == "{") return indentation + out;

    return indentation + out;
}

std::string Transpiler::translateType(const std::string& onyxType) {
    if (onyxType == "i32") return "int";
    if (onyxType == "u32") return "uint32_t";
    if (onyxType == "u8") return "uint8_t";
    if (onyxType == "f32") return "float";
    if (onyxType == "f64") return "double";
    if (onyxType == "bool") return "bool";
    if (onyxType == "str") return "char*";
    if (onyxType == "ptr") return "void*";
    if (onyxType == "void") return "void";
    if (onyxType.back() == '*') return translateType(onyxType.substr(0, onyxType.size()-1)) + "*";
    return onyxType;
}

std::string Transpiler::replaceMethodCalls(std::string line) {
    static const std::regex R_METHOD_CALL(R"((["\w>]+)\.(\w+)\(([^)]*)\))");
    std::string current = line;
    std::smatch match;
    while (std::regex_search(current, match, R_METHOD_CALL)) {
        std::string varName = match[1].str();
        std::string methodName = match[2].str();
        std::string args = match[3].str();
        std::string typeName = m_symbols.lookup(varName);
        std::string replacement;
        if (!typeName.empty()) {
            std::string newArgs = "&" + varName; // Naive address-of
            if (!args.empty()) newArgs += ", " + args;
            replacement = typeName + "_" + methodName + "(" + newArgs + ")";
            current = match.prefix().str() + replacement + match.suffix().str();
        } else {
            // Stop if we can't resolve, to avoid infinite loops or bad replacement
             break;
        }
    }
    return current;
}

std::string Transpiler::replacePipeOperators(std::string line) {
    static const std::regex R_PIPE(R"((.+?)\s*\|>\s*([\w]+)\(([^)]*)\))");
    std::string current = line;
    std::smatch match;
    while (std::regex_search(current, match, R_PIPE)) {
        std::string arg1 = match[1].str();
        std::string func = match[2].str();
        std::string otherArgs = match[3].str();
        
        // Trim whitespace from the piped argument
        arg1 = std::regex_replace(arg1, std::regex(R"(^\s+|\s+$)"), "");

        std::string finalArg = arg1;
        std::string typeName = m_symbols.lookup(arg1);
        // Heuristic: If the function name starts with the variable's type name,
        // it's probably a resolve-style function call needing a pointer.
        if (!typeName.empty() && func.rfind(typeName + "_", 0) == 0) {
             finalArg = "&" + arg1;
        }
        
        std::string replacement;
        std::regex placeholder_regex(R"(\b_\b)");

        if (std::regex_search(otherArgs, placeholder_regex)) {
            // Placeholder exists: replace all occurrences of `_`
            std::string newArgs = std::regex_replace(otherArgs, placeholder_regex, finalArg);
            replacement = func + "(" + newArgs + ")";
        } else {
            // No placeholder: prepend as the first argument (default behavior)
            std::string newArgs = finalArg;
            if (!otherArgs.empty()) {
                newArgs += ", " + otherArgs;
            }
            replacement = func + "(" + newArgs + ")";
        }
        
        current = match.prefix().str() + replacement + match.suffix().str();
    }
    return current;
}

} // namespace onyx