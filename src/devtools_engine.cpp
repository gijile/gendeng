/**
 * @file devtools_engine.cpp
 * @brief High-Performance Core for Mobile DevTools (WASM)
 * 
 * Compile with Emscripten:
 *   emcc devtools_engine.cpp -O3 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 \
 *        --bind -o devtools_engine.js
 * 
 * Designed for mobile browser limits: zero external dependencies, low-latency,
 * manual memory layout emulation, and direct C++ standard vectors.
 */

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <map>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif

// ==========================================
// 1. AST TOKENIZER & LEXER CORE
// ==========================================

enum class TokenType {
    UNKNOWN,
    KEYWORD,
    IDENTIFIER,
    NUMBER,
    STRING,
    OPERATOR,
    PUNCTUATION,
    WHITESPACE
};

struct Token {
    TokenType type;
    std::string value;
    int start;
    int end;

    // Helper for bindings
    std::string getTypeString() const {
        switch (type) {
            case TokenType::KEYWORD: return "KEYWORD";
            case TokenType::IDENTIFIER: return "IDENTIFIER";
            case TokenType::NUMBER: return "NUMBER";
            case TokenType::STRING: return "STRING";
            case TokenType::OPERATOR: return "OPERATOR";
            case TokenType::PUNCTUATION: return "PUNCTUATION";
            case TokenType::WHITESPACE: return "WHITESPACE";
            default: return "UNKNOWN";
        }
    }
};

class Lexer {
private:
    std::string source;
    size_t cursor = 0;

    bool isWhitespace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    bool isAlpha(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
    }

    bool isDigit(char c) {
        return c >= '0' && c <= '9';
    }

    bool isKeyword(const std::string& val) {
        static const std::vector<std::string> keywords = {
            "const", "let", "var", "function", "return", "if", "else", 
            "for", "while", "console", "log", "network", "import", "class"
        };
        return std::find(keywords.begin(), keywords.end(), val) != keywords.end();
    }

public:
    Lexer(const std::string& src) : source(src) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        size_t len = source.length();

        while (cursor < len) {
            char current = source[cursor];
            size_t startPos = cursor;

            // Whitespace handling
            if (isWhitespace(current)) {
                std::string val = "";
                while (cursor < len && isWhitespace(source[cursor])) {
                    val += source[cursor++];
                }
                tokens.push_back({TokenType::WHITESPACE, val, (int)startPos, (int)cursor});
                continue;
            }

            // String literals
            if (current == '"' || current == '\'') {
                char quote = current;
                std::string val = "";
                val += quote;
                cursor++; // skip starting quote
                while (cursor < len && source[cursor] != quote) {
                    // Simple escape character skip
                    if (source[cursor] == '\\' && cursor + 1 < len) {
                        val += '\\';
                        val += source[cursor + 1];
                        cursor += 2;
                    } else {
                        val += source[cursor++];
                    }
                }
                if (cursor < len) {
                    val += quote;
                    cursor++; // skip ending quote
                }
                tokens.push_back({TokenType::STRING, val, (int)startPos, (int)cursor});
                continue;
            }

            // Numbers
            if (isDigit(current)) {
                std::string val = "";
                while (cursor < len && (isDigit(source[cursor]) || source[cursor] == '.')) {
                    val += source[cursor++];
                }
                tokens.push_back({TokenType::NUMBER, val, (int)startPos, (int)cursor});
                continue;
            }

            // Identifiers and Keywords
            if (isAlpha(current)) {
                std::string val = "";
                while (cursor < len && (isAlpha(source[cursor]) || isDigit(source[cursor]))) {
                    val += source[cursor++];
                }
                TokenType t = isKeyword(val) ? TokenType::KEYWORD : TokenType::IDENTIFIER;
                tokens.push_back({t, val, (int)startPos, (int)cursor});
                continue;
            }

            // Operators (single & double chars)
            if (std::string("+-*/%=!&|^<>").find(current) != std::string::npos) {
                std::string val = "";
                val += current;
                cursor++;
                if (cursor < len && std::string("=&|<>").find(source[cursor]) != std::string::npos) {
                    val += source[cursor++];
                }
                tokens.push_back({TokenType::OPERATOR, val, (int)startPos, (int)cursor});
                continue;
            }

            // Punctuations
            if (std::string("(){}[],;.").find(current) != std::string::npos) {
                std::string val = "";
                val += current;
                cursor++;
                tokens.push_back({TokenType::PUNCTUATION, val, (int)startPos, (int)cursor});
                continue;
            }

            // Unknown character
            std::string val = "";
            val += current;
            cursor++;
            tokens.push_back({TokenType::UNKNOWN, val, (int)startPos, (int)cursor});
        }

        return tokens;
    }
};

// ==========================================
// 2. NETWORK LOG FILTERING ENGINE
// ==========================================

struct NetworkRequestMock {
    std::string url;
    std::string method;
    std::string payload;
    int status;
};

class NetworkEngine {
public:
    // Performs lighting-fast multi-keyword query searches in WASM on payload fields
    static std::vector<int> filterRequests(const std::vector<NetworkRequestMock>& list, const std::string& query) {
        std::vector<int> matchingIndices;
        if (query.empty()) {
            for (size_t i = 0; i < list.size(); ++i) {
                matchingIndices.push_back((int)i);
            }
            return matchingIndices;
        }

        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

        for (size_t i = 0; i < list.size(); ++i) {
            std::string textToSearch = list[i].url + " " + list[i].method + " " + list[i].payload;
            std::transform(textToSearch.begin(), textToSearch.end(), textToSearch.begin(), ::tolower);

            if (textToSearch.find(lowerQuery) != std::string::npos) {
                matchingIndices.push_back((int)i);
            }
        }
        return matchingIndices;
    }
};

// ==========================================
// 3. MEMORY HEAP PROFILER CORE
// ==========================================

struct HeapBlock {
    std::string address;
    size_t size;
    std::string type;
    bool isDeallocated;
};

class HeapProfiler {
private:
    std::vector<HeapBlock> allocations;
    size_t totalAllocatedBytes = 0;
    size_t totalDeallocatedBytes = 0;

public:
    HeapProfiler() {
        // Pre-reserve to avoid reallocations in latency-critical code
        allocations.reserve(10000);
    }

    void registerAllocation(const std::string& addr, size_t bytes, const std::string& type) {
        allocations.push_back({addr, bytes, type, false});
        totalAllocatedBytes += bytes;
    }

    void registerDeallocation(const std::string& addr) {
        for (auto& block : allocations) {
            if (block.address == addr && !block.isDeallocated) {
                block.isDeallocated = true;
                totalDeallocatedBytes += block.size;
                break;
            }
        }
    }

    size_t getActiveMemory() const {
        return totalAllocatedBytes - totalDeallocatedBytes;
    }

    // Calculates heap fragmentation percentage
    // (Fragmented memory = size of inactive blocks sandwiched between active blocks)
    double calculateFragmentation() const {
        if (allocations.empty()) return 0.0;
        
        size_t freeSandwichSize = 0;
        bool activeFound = false;
        size_t tempFree = 0;

        for (const auto& block : allocations) {
            if (!block.isDeallocated) {
                activeFound = true;
                freeSandwichSize += tempFree;
                tempFree = 0;
            } else {
                if (activeFound) {
                    tempFree += block.size;
                }
            }
        }

        size_t activeMem = getActiveMemory();
        if (activeMem == 0) return 0.0;
        return (double)freeSandwichSize / (double)(activeMem + freeSandwichSize) * 100.0;
    }

    std::vector<HeapBlock> getActiveAllocations() const {
        std::vector<HeapBlock> active;
        for (const auto& b : allocations) {
            if (!b.isDeallocated) {
                active.push_back(b);
            }
        }
        return active;
    }

    void clear() {
        allocations.clear();
        totalAllocatedBytes = 0;
        totalDeallocatedBytes = 0;
    }
};

// ==========================================
// 4. MICRO-BENCHMARK PERFORMANCE SIMULATOR
// ==========================================

class PerformanceBenchmark {
public:
    // Utility for microsecond timing calculations
    static double benchmarkJSvsCPlusPlus(int operationsCount) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Simulating highly intensive token parsing / hash operations
        volatile unsigned long long hash = 0;
        for (int i = 0; i < operationsCount; ++i) {
            hash += (i * 33) ^ 0xDEADBEEF;
            hash = (hash << 5) | (hash >> 59); // bitwise rotation
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        return duration.count(); // return time in milliseconds
    }
};


// ==========================================
// 5. TELEMETRY MONITOR & ENGINE
// ==========================================

struct TelemetryEvent {
    std::string id;
    long long timestamp;
    std::string category; // "CPU", "MEMORY", "NETWORK", "UI_RENDER"
    std::string name;
    std::string details;
};

class TelemetryCollector {
private:
    std::vector<TelemetryEvent> events;
    size_t maxCapacity = 200;

public:
    TelemetryCollector() {
        events.reserve(maxCapacity);
    }

    void addEvent(const std::string& id, const std::string& category, const std::string& name, const std::string& details) {
        if (events.size() >= maxCapacity) {
            events.erase(events.begin()); // Simple FIFO
        }
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        events.push_back({id, ms, category, name, details});
    }

    std::vector<TelemetryEvent> getEvents() const {
        return events;
    }

    void clear() {
        events.clear();
    }

    int getEventCount() const {
        return (int)events.size();
    }
};

// ==========================================
// 6. EXECUTION PROFILER (CPU TRACE) ENGINE
// ==========================================

struct ProfileSample {
    std::string functionName;
    double durationUs; // Microseconds
    int callDepth;
    double relativeStartTimeMs;
};

class ExecutionProfiler {
private:
    std::vector<ProfileSample> samples;
    bool recording = false;
    std::chrono::high_resolution_clock::time_point recordingStart;

public:
    void startRecording() {
        recording = true;
        samples.clear();
        recordingStart = std::chrono::high_resolution_clock::now();
    }

    void stopRecording() {
        recording = false;
    }

    void addSample(const std::string& name, double durationUs, int depth) {
        if (!recording) return;
        
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> relTime = now - recordingStart;
        
        samples.push_back({name, durationUs, depth, relTime.count()});
    }

    std::vector<ProfileSample> getSamples() const {
        return samples;
    }

    bool isRecording() const {
        return recording;
    }

    void clear() {
        samples.clear();
    }
};


// ==========================================
// 8. DOM ELEMENT AST & SELECTOR MATCHER ENGINE
// ==========================================

struct DOMNode {
    int id;
    std::string tagName;
    std::string idAttribute;
    std::string className; // Space-separated classes
    std::string innerText;
    int parentId;
    std::vector<int> childIds;
};

class SelectorMatcher {
public:
    static bool matchSelector(const DOMNode& node, const std::string& selector) {
        if (selector.empty()) return false;
        
        // Fast ID Match (zero heap allocations)
        if (selector[0] == '#') {
            if (selector.length() == 1) return false;
            size_t selLen = selector.length() - 1;
            if (node.idAttribute.length() != selLen) return false;
            return node.idAttribute.compare(0, selLen, selector, 1, selLen) == 0;
        }
        
        // Fast Class Match (in-place scanning to completely avoid std::stringstream overhead)
        if (selector[0] == '.') {
            if (selector.length() == 1) return false;
            const std::string& targetClass = node.className;
            std::string classToMatch = selector.substr(1);
            size_t matchLen = classToMatch.length();
            
            size_t pos = 0;
            while ((pos = targetClass.find(classToMatch, pos)) != std::string::npos) {
                bool leftMatch = (pos == 0 || targetClass[pos - 1] == ' ');
                bool rightMatch = (pos + matchLen == targetClass.length() || targetClass[pos + matchLen] == ' ');
                if (leftMatch && rightMatch) return true;
                pos += 1; // Slide forward to check next candidate
            }
            return false;
        }
        
        // Fast Case-Insensitive Tag Name Match (zero heap allocations)
        if (node.tagName.length() != selector.length()) return false;
        for (size_t i = 0; i < selector.length(); ++i) {
            if (std::tolower(static_cast<unsigned char>(node.tagName[i])) != 
                std::tolower(static_cast<unsigned char>(selector[i]))) {
                return false;
            }
        }
        return true;
    }
    
    static std::vector<int> querySelectorAll(const std::vector<DOMNode>& nodes, const std::string& selector) {
        std::vector<int> matchedIds;
        matchedIds.reserve(nodes.size() / 4 + 1); // Pre-allocate initial estimation to avoid resizing overhead on mobile
        for (const auto& node : nodes) {
            if (matchSelector(node, selector)) {
                matchedIds.push_back(node.id);
            }
        }
        return matchedIds;
    }
};


// ==========================================
// 9. VIRTUAL FILE SYSTEM ENGINE (MEMFS SIMULATION)
// ==========================================

struct VirtualFile {
    std::string path;
    std::string name;
    std::string content;
    size_t sizeBytes;
    bool isDir;
    long long lastModified;
};

class MEMFSEngine {
private:
    std::vector<VirtualFile> files;
    static const size_t MAX_FS_SIZE = 2 * 1024 * 1024; // Limit 2 MB to prevent mobile browser OOM crashes
    static const size_t MAX_FILE_COUNT = 50;           // Prevents excessive descriptors

public:
    MEMFSEngine() {
        files.reserve(MAX_FILE_COUNT);
        
        // Seed default system files representing a standard mobile Chromium shell structure
        long long epoch = 1790000000000LL;
        files.push_back({"/sys", "sys", "", 0, true, epoch});
        files.push_back({"/sys/version", "version", "Chromium DevTools WASM Core v1.4.2-memfs", 39, false, epoch});
        files.push_back({"/sys/config.json", "config.json", "{\n  \"enable_jit\": false,\n  \"memfs_buffer_size\": 4194304\n}", 62, false, epoch});
        files.push_back({"/usr", "usr", "", 0, true, epoch});
        files.push_back({"/usr/local", "local", "", 0, true, epoch});
        files.push_back({"/usr/local/init.js", "init.js", "console.log('MEMFS engine active on mobile browser target');", 60, false, epoch});
    }

    std::vector<VirtualFile> getFiles() const {
        return files;
    }

    bool write_file(const std::string& path, const std::string& content, bool isDir) {
        if (path.empty() || path[0] != '/') return false;

        // Size guard for resource protection
        if (getFsMemoryUsage() + content.length() > MAX_FS_SIZE) {
            return false;
        }

        // Extract file name
        size_t lastSlash = path.find_last_of('/');
        std::string name = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
        if (name.empty()) {
            name = path;
        }

        // Check if file already exists
        for (auto& file : files) {
            if (file.path == path) {
                if (file.isDir) return false; // Cannot overwrite directory
                file.content = content;
                file.sizeBytes = content.length();
                auto now = std::chrono::high_resolution_clock::now();
                file.lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                return true;
            }
        }

        // File count boundary safeguard
        if (files.size() >= MAX_FILE_COUNT) {
            return false;
        }

        auto now = std::chrono::high_resolution_clock::now();
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        files.push_back({path, name, content, content.length(), isDir, ms});
        return true;
    }

    std::string read_file(const std::string& path) const {
        for (const auto& file : files) {
            if (file.path == path) {
                return file.content;
            }
        }
        return "";
    }

    bool delete_file(const std::string& path) {
        for (auto it = files.begin(); it != files.end(); ++it) {
            if (it->path == path) {
                // If it is a directory, make sure it is empty (to prevent dangling files)
                if (it->isDir) {
                    for (const auto& f : files) {
                        if (f.path != path && f.path.rfind(path, 0) == 0) {
                            return false; // Directory not empty
                        }
                    }
                }
                files.erase(it);
                return true;
            }
        }
        return false;
    }

    size_t getFsMemoryUsage() const {
        size_t total = 0;
        for (const auto& file : files) {
            total += file.content.length() + file.path.length() + file.name.length() + sizeof(VirtualFile);
        }
        return total;
    }

    void clear() {
        files.clear();
    }
};


// ==========================================
// 10. HIGH-PERFORMANCE MOBILE ARENA ALLOCATOR
// ==========================================

class WasmArenaAllocator {
private:
    size_t capacity;
    size_t offset;
    size_t allocationCount;

public:
    WasmArenaAllocator(size_t size) : capacity(size), offset(0), allocationCount(0) {}

    size_t getCapacity() const { return capacity; }
    size_t getOffset() const { return offset; }
    size_t getAllocationCount() const { return allocationCount; }

    bool allocate(size_t bytes) {
        size_t aligned = (bytes + 7) & ~7; // 8-byte alignment for fast mobile memory access
        if (offset + aligned > capacity) {
            return false;
        }
        offset += aligned;
        allocationCount++;
        return true;
    }

    void reset() {
        offset = 0;
        allocationCount = 0;
    }
};


// ==========================================
// 7. EMSCRIPTEN WEB IDL / BINDINGS SETUP
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(devtools_wasm_module) {
    // Expose VirtualFile & MEMFSEngine
    class_<VirtualFile>("VirtualFile")
        .property("path", &VirtualFile::path)
        .property("name", &VirtualFile::name)
        .property("content", &VirtualFile::content)
        .property("sizeBytes", &VirtualFile::sizeBytes)
        .property("isDir", &VirtualFile::isDir)
        .property("lastModified", &VirtualFile::lastModified);

    register_vector<VirtualFile>("VectorVirtualFile");

    class_<MEMFSEngine>("MEMFSEngine")
        .constructor()
        .function("getFiles", &MEMFSEngine::getFiles)
        .function("write_file", &MEMFSEngine::write_file)
        .function("read_file", &MEMFSEngine::read_file)
        .function("delete_file", &MEMFSEngine::delete_file)
        .function("getFsMemoryUsage", &MEMFSEngine::getFsMemoryUsage)
        .function("clear", &MEMFSEngine::clear);

    class_<WasmArenaAllocator>("WasmArenaAllocator")
        .constructor<size_t>()
        .function("getCapacity", &WasmArenaAllocator::getCapacity)
        .function("getOffset", &WasmArenaAllocator::getOffset)
        .function("getAllocationCount", &WasmArenaAllocator::getAllocationCount)
        .function("allocate", &WasmArenaAllocator::allocate)
        .function("reset", &WasmArenaAllocator::reset);

    // Expose DOMNode & SelectorMatcher
    class_<DOMNode>("DOMNode")
        .property("id", &DOMNode::id)
        .property("tagName", &DOMNode::tagName)
        .property("idAttribute", &DOMNode::idAttribute)
        .property("className", &DOMNode::className)
        .property("innerText", &DOMNode::innerText)
        .property("parentId", &DOMNode::parentId)
        .property("childIds", &DOMNode::childIds);

    register_vector<DOMNode>("VectorDOMNode");

    class_<SelectorMatcher>("SelectorMatcher")
        .class_function("matchSelector", &SelectorMatcher::matchSelector)
        .class_function("querySelectorAll", &SelectorMatcher::querySelectorAll);

    // Expose TokenType & Token
    class_<Token>("Token")
        .property("typeStr", &Token::getTypeString)
        .property("value", &Token::value)
        .property("start", &Token::start)
        .property("end", &Token::end);

    register_vector<Token>("VectorToken");

    // Expose Lexer
    class_<Lexer>("Lexer")
        .constructor<std::string>()
        .function("tokenize", &Lexer::tokenize);

    // Expose Heap Block
    class_<HeapBlock>("HeapBlock")
        .property("address", &HeapBlock::address)
        .property("size", &HeapBlock::size)
        .property("type", &HeapBlock::type)
        .property("isDeallocated", &HeapBlock::isDeallocated);

    register_vector<HeapBlock>("VectorHeapBlock");

    // Expose HeapProfiler
    class_<HeapProfiler>("HeapProfiler")
        .constructor()
        .function("registerAllocation", &HeapProfiler::registerAllocation)
        .function("registerDeallocation", &HeapProfiler::registerDeallocation)
        .function("getActiveMemory", &HeapProfiler::getActiveMemory)
        .function("calculateFragmentation", &HeapProfiler::calculateFragmentation)
        .function("getActiveAllocations", &HeapProfiler::getActiveAllocations)
        .function("clear", &HeapProfiler::clear);

    // Expose Network Mock Request
    class_<NetworkRequestMock>("NetworkRequestMock")
        .constructor<>()
        .property("url", &NetworkRequestMock::url)
        .property("method", &NetworkRequestMock::method)
        .property("payload", &NetworkRequestMock::payload)
        .property("status", &NetworkRequestMock::status);

    register_vector<NetworkRequestMock>("VectorNetworkRequestMock");
    register_vector<int>("VectorInt");

    // Expose NetworkEngine static bindings helper
    class_<NetworkEngine>("NetworkEngine")
        .class_function("filterRequests", &NetworkEngine::filterRequests);

    // Expose PerformanceBenchmark helper
    class_<PerformanceBenchmark>("PerformanceBenchmark")
        .class_function("benchmarkJSvsCPlusPlus", &PerformanceBenchmark::benchmarkJSvsCPlusPlus);

    // Expose TelemetryEvent
    class_<TelemetryEvent>("TelemetryEvent")
        .property("id", &TelemetryEvent::id)
        .property("timestamp", &TelemetryEvent::timestamp)
        .property("category", &TelemetryEvent::category)
        .property("name", &TelemetryEvent::name)
        .property("details", &TelemetryEvent::details);

    register_vector<TelemetryEvent>("VectorTelemetryEvent");

    // Expose TelemetryCollector
    class_<TelemetryCollector>("TelemetryCollector")
        .constructor()
        .function("addEvent", &TelemetryCollector::addEvent)
        .function("getEvents", &TelemetryCollector::getEvents)
        .function("clear", &TelemetryCollector::clear)
        .function("getEventCount", &TelemetryCollector::getEventCount);

    // Expose ProfileSample
    class_<ProfileSample>("ProfileSample")
        .property("functionName", &ProfileSample::functionName)
        .property("durationUs", &ProfileSample::durationUs)
        .property("callDepth", &ProfileSample::callDepth)
        .property("relativeStartTimeMs", &ProfileSample::relativeStartTimeMs);

    register_vector<ProfileSample>("VectorProfileSample");

    // Expose ExecutionProfiler
    class_<ExecutionProfiler>("ExecutionProfiler")
        .constructor()
        .function("startRecording", &ExecutionProfiler::startRecording)
        .function("stopRecording", &ExecutionProfiler::stopRecording)
        .function("addSample", &ExecutionProfiler::addSample)
        .function("getSamples", &ExecutionProfiler::getSamples)
        .function("isRecording", &ExecutionProfiler::isRecording)
        .function("clear", &ExecutionProfiler::clear);
}
#endif

// Main entry point for local execution/testing if compiled as standalone binary
int main() {
    std::cout << "--- WASM C++ DevTools Core Engine Verified ---" << std::endl;
    Lexer testLexer("const x = 42; console.log(\"Hello Wasm!\");");
    auto tokens = testLexer.tokenize();
    std::cout << "Tokenization count: " << tokens.size() << std::endl;
    for (const auto& token : tokens) {
        if (token.getTypeString() != "WHITESPACE") {
            std::cout << "  [" << token.getTypeString() << "]: " << token.value << std::endl;
        }
    }
    return 0;
}
