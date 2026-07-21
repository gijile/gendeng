# Complete Compilation Guide for Gendeng Mobile DevTools

## 🛠️ Prerequisites

1. **Emscripten SDK** (latest version)
   ```bash
   # Install/update Emscripten
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh  # Linux/Mac
   # or emsdk_env.bat on Windows
   ```

2. **C++ Compiler** (g++, clang)
3. **CMake** (optional, for advanced builds)

---

## 📦 Building the Complete DevTools Engine

### Option 1: Simple Single Command (Recommended)

```bash
# Compile all modules into single WASM bundle
emcc \
  src/devtools_engine.cpp \
  src/console_engine.cpp \
  src/network_inspector.cpp \
  src/css_engine.cpp \
  src/stack_trace_engine.cpp \
  src/heap_profiler_fixed.cpp \
  src/lexer_enhanced.cpp \
  src/storage_inspector.cpp \
  src/css_engine_advanced.cpp \
  -O3 \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s TOTAL_MEMORY=134217728 \
  -s INITIAL_MEMORY=67108864 \
  --bind \
  -o dist/devtools_core.js
```

**Output Files:**
- `dist/devtools_core.js` - JavaScript wrapper
- `dist/devtools_core.wasm` - WebAssembly binary (~2-3MB)

---

### Option 2: Modular Compilation

Compile each module separately for better debugging:

```bash
# Console Engine
emcc src/console_engine.cpp -O3 -s WASM=1 --bind -o dist/console.js

# Network Inspector
emcc src/network_inspector.cpp -O3 -s WASM=1 --bind -o dist/network.js

# CSS Engines (both)
emcc src/css_engine.cpp src/css_engine_advanced.cpp -O3 -s WASM=1 --bind -o dist/css.js

# Stack Trace Engine
emcc src/stack_trace_engine.cpp -O3 -s WASM=1 --bind -o dist/stacktrace.js

# Profilers & Tools
emcc src/heap_profiler_fixed.cpp src/lexer_enhanced.cpp src/storage_inspector.cpp \
  -O3 -s WASM=1 --bind -o dist/tools.js
```

---

### Option 3: Debug Build (Development)

```bash
emcc \
  src/devtools_engine.cpp \
  src/console_engine.cpp \
  src/network_inspector.cpp \
  src/css_engine.cpp \
  src/stack_trace_engine.cpp \
  src/heap_profiler_fixed.cpp \
  src/lexer_enhanced.cpp \
  src/storage_inspector.cpp \
  src/css_engine_advanced.cpp \
  -g4 \
  -s DEMANGLE_SUPPORT=1 \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  --bind \
  -o dist/devtools_core_debug.js
```

**Flags Explanation:**
- `-g4` - Maximum debug information
- `-s DEMANGLE_SUPPORT=1` - Demangle C++ names for debugging
- `-O0` - No optimization (slower, but debuggable)

---

## 🔧 Optimization Flags

### For Production (Maximum Speed)
```bash
-O3 \
-s TOTAL_MEMORY=134217728 \
-s INITIAL_MEMORY=67108864 \
-s WASM_ASYNC_COMPILATION=1 \
-s WASM_PARALLEL_DOWNLOADS=1 \
-s MALLOC=emmalloc
```

### For Mobile (Balanced)
```bash
-O2 \
-s TOTAL_MEMORY=67108864 \
-s INITIAL_MEMORY=33554432 \
-s ALLOW_MEMORY_GROWTH=1 \
-s WASM_ASYNC_COMPILATION=1
```

### For Low-End Devices
```bash
-O1 \
-s TOTAL_MEMORY=33554432 \
-s INITIAL_MEMORY=16777216 \
-s ALLOW_MEMORY_GROWTH=1 \
-s WASM_ASYNC_COMPILATION=1
```

---

## 📋 Complete Flags Reference

| Flag | Value | Purpose |
|------|-------|----------|
| `-O3` | - | Optimization level (O0, O1, O2, O3) |
| `-s WASM=1` | - | Generate WebAssembly |
| `-s ALLOW_MEMORY_GROWTH=1` | - | Allow heap to grow dynamically |
| `-s TOTAL_MEMORY` | bytes | Maximum memory available |
| `-s INITIAL_MEMORY` | bytes | Initial memory allocated |
| `--bind` | - | Generate JavaScript bindings |
| `-s DEMANGLE_SUPPORT=1` | - | Demangle C++ symbols |
| `-g4` | - | Full debug info |
| `-s WASM_ASYNC_COMPILATION=1` | - | Compile WASM asynchronously |

---

## 🚀 Quick Build Script

Create `build.sh`:

```bash
#!/bin/bash

echo "🔨 Building Gendeng Mobile DevTools Engine..."

# Create output directory
mkdir -p dist

# Check if Emscripten is installed
if ! command -v emcc &> /dev/null; then
    echo "❌ Emscripten not found. Please install it first."
    exit 1
fi

echo "Compiling with Emscripten..."

# Determine build type
BUILD_TYPE=${1:-release}  # default: release

if [ "$BUILD_TYPE" = "debug" ]; then
    echo "📝 Debug build..."
    OPTS="-g4 -s DEMANGLE_SUPPORT=1"
else
    echo "⚡ Release build..."
    OPTS="-O3"
fi

# Compile
emcc \
  src/devtools_engine.cpp \
  src/console_engine.cpp \
  src/network_inspector.cpp \
  src/css_engine.cpp \
  src/stack_trace_engine.cpp \
  src/heap_profiler_fixed.cpp \
  src/lexer_enhanced.cpp \
  src/storage_inspector.cpp \
  src/css_engine_advanced.cpp \
  $OPTS \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s TOTAL_MEMORY=134217728 \
  -s INITIAL_MEMORY=67108864 \
  --bind \
  -o dist/devtools_core.js

if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
    echo "📦 Output: dist/devtools_core.js"
    echo "📦 WASM:   dist/devtools_core.wasm"
    du -h dist/devtools_core.*
else
    echo "❌ Build failed!"
    exit 1
fi
```

Run it:
```bash
chmod +x build.sh
./build.sh release  # or ./build.sh debug
```

---

## 📥 Usage in Browser

```html
<!-- Load the compiled module -->
<script async src="dist/devtools_core.js"></script>

<script>
  // Wait for module to load
  Module.onRuntimeInitialized = function() {
    // Create instances
    const console = new Module.ConsoleEngine();
    const network = new Module.NetworkInspector();
    const css = new Module.CSSEngine();
    const heap = new Module.HeapProfilerFixed(10 * 1024 * 1024);
    const lexer = new Module.LexerEnhanced(sourceCode);
    const storage = new Module.StorageInspector();
    
    // Use them!
    console.captureMessage(
      Module.ConsoleLevel.LOG,
      ["Hello from WASM!"],
      "main.js",
      42
    );
    
    console.log("✅ DevTools core loaded!");
  };
</script>
```

---

## 🐛 Troubleshooting

### "emcc: command not found"
```bash
# Activate Emscripten
source /path/to/emsdk/emsdk_env.sh
```

### "WASM compilation failed"
- Check C++ syntax errors
- Ensure all includes are valid
- Try removing `-O3` temporarily (use `-O0`)

### "Out of memory"
- Reduce `TOTAL_MEMORY` and `INITIAL_MEMORY`
- Enable `ALLOW_MEMORY_GROWTH=1`

### "Module not loading in browser"
- Ensure WASM file is in same directory as JS file
- Check CORS headers if loading from different domain
- Check browser console for detailed error

---

## 📊 Build Sizes

Typical output sizes:

| Build | JS | WASM | Total |
|-------|-----|------|-------|
| Release (-O3) | 450KB | 2.1MB | 2.55MB |
| Debug (-g4) | 1.2MB | 3.8MB | 5.0MB |
| Minimal (-O2) | 380KB | 1.8MB | 2.18MB |

WASM will be gzipped to ~600KB in production.

---

## ✨ Next Steps

1. Compile the code
2. Load in your DevTools UI
3. Start capturing console/network/storage data
4. Visualize in real-time dashboard

Happy debugging! 🎉
