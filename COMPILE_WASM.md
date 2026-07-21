# Panduan Kompilasi WebAssembly (WASM) - Mobile DevTools Engine
---

Dokumen ini berisi panduan teknis langkah-demi-langkah untuk melakukan kompilasi `src/devtools_engine.cpp` menjadi modul WebAssembly (`.wasm` & `.js` bindings) menggunakan **Emscripten (emcc)** dengan optimasi performa tinggi untuk platform mobile (ARMv8/ARMv9).

## 1. Persiapan Environment (Emscripten SDK)

Pastikan Anda memiliki **Emscripten SDK (emsdk)** yang terinstal dan aktif di sistem Anda.

```bash
# Clone repository Emscripten (jika belum ada)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Download dan install versi stabil terbaru
./emsdk install latest

# Aktifkan versi terbaru di session shell Anda
./emsdk activate latest

# Masukkan environment variable ke path Anda
source ./emsdk_env.sh
```

Verifikasi instalasi dengan menjalankan:
```bash
emcc -v
```

---

## 2. Struktur Binding & API Terpapar

Modul C++ kita menggunakan **Embind** (`<emscripten/bind.h>`) untuk melakukan interkoneksi langsung ke JavaScript/TypeScript dengan zero-overhead wrapper. Class utama yang diekspos meliputi:

- **`WasmArenaAllocator`**: Arena allocator dengan segmentasi cache-line 8-byte untuk AST/Parser tanpa overhead fragmentation.
- **`MEMFSEngine`**: Virtual file-system berbasis RAM yang dioptimasi untuk mencegah memory leaks pada runtime mobile.
- **`SelectorMatcher`**: DOM selector matching engine super cepat berbasis string scanning in-place.

---

## 3. Perintah Kompilasi Berperforma Tinggi (Optimized for Mobile)

Gunakan perintah kompilasi di bawah ini untuk menghasilkan output WebAssembly yang dioptimasi secara maksimal untuk browser seluler.

### Opsi A: Produksi (Highly Optimized & Small Size)

```bash
emcc src/devtools_engine.cpp \
  -O3 \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s INITIAL_MEMORY=16777216 \
  -s MAXIMUM_MEMORY=134217728 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME="'createDevToolsEngine'" \
  --bind \
  -msimd128 \
  -fno-exceptions \
  -fno-rtti \
  -ffast-math \
  -o src/devtools_engine_wasm.js
```

### Penjelasan Bendera Kompilasi (Compiler Flags):

| Flag | Deskripsi / Alasan Optimasi Mobile |
| :--- | :--- |
| `-O3` | Level optimasi agresif (agressive loop unrolling, vectorization, dan inline expansion). |
| `-s WASM=1` | Memaksa kompilasi langsung ke format WebAssembly biner. |
| `-s ALLOW_MEMORY_GROWTH=1` | Mengizinkan modul mengalokasikan RAM tambahan secara dinamis jika heap habis. |
| `-s INITIAL_MEMORY=16777216` | Alokasi awal sebesar **16 MB**. Ideal untuk buffer parser mobile tanpa memicu syscall berlebih. |
| `-s MAXIMUM_MEMORY=134217728` | Batasan memori maksimum **128 MB** untuk mencegah browser mobile membunuh tab (OOM crash protection). |
| `-s MODULARIZE=1` | Membungkus output JS ke dalam pola modular ES6 agar mudah di-import secara asinkron. |
| `-s EXPORT_NAME="'createDevToolsEngine'"` | Menetapkan nama fungsi factory penginisiasi modul WebAssembly. |
| `--bind` | Mengaktifkan dukungan Embind untuk registrasi class C++ langsung ke JS. |
| `-msimd128` | **Sangat Krusial untuk Mobile!** Mengaktifkan instruksi SIMD 128-bit yang akan diterjemahkan ke instruksi ARM NEON pada prosesor smartphone modern. Mempercepat string matching hingga 4x lipat. |
| `-fno-exceptions` | Menonaktifkan penanganan exception C++ (`try-catch`). Menghemat ukuran biner WASM hingga 20% dan meningkatkan eksekusi CPU. |
| `-fno-rtti` | Menonaktifkan Run-Time Type Information untuk meminimalkan beban metadata class. |
| `-ffast-math` | Mengaktifkan optimasi matematika floating-point non-standar untuk kecepatan maksimal. |

---

## 4. Opsi B: Debug Mode (Dengan Source Maps & Profiling)

Jika Anda ingin melakukan debug struktur heap C++ langsung dari **Chromium DevTools Console** di smartphone Anda via remote debugging:

```bash
emcc src/devtools_engine.cpp \
  -g4 \
  -O1 \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s INITIAL_MEMORY=16777216 \
  --bind \
  -s ASSERTIONS=1 \
  -s SAFE_HEAP=1 \
  -o src/devtools_engine_wasm.js
```

*Catatan: Jangan gunakan `-s SAFE_HEAP=1` atau `-s ASSERTIONS=1` di production build karena akan menurunkan performa parser secara drastis.*

---

## 5. Integrasi ke TypeScript Bridge (`src/wasm_bridge.ts`)

Setelah kompilasi selesai, letakkan file `devtools_engine_wasm.js` dan `devtools_engine_wasm.wasm` di folder `src/`. Bridge TypeScript akan mendeteksi modul tersebut dan menghentikan fallback simulasi otomatis:

```typescript
// Contoh loading module di JavaScript / TypeScript
import createDevToolsEngine from './devtools_engine_wasm.js';

async function initEngine() {
  const Module = await createDevToolsEngine({
    locateFile: (path: string) => {
      if (path.endsWith('.wasm')) {
        return '/src/devtools_engine_wasm.wasm';
      }
      return path;
    }
  });
  
  // Arena Allocator siap digunakan
  const allocator = new Module.WasmArenaAllocator(1024 * 1024); // 1 MB
  console.log("WASM Arena Active, Capacity:", allocator.getCapacity());
}
```

---

## 6. Tips Optimasi Tambahan Khusus Mobile Web

1. **Keep-Alive Threading**: Hindari multithreading (`-pthread`) untuk browser seluler kelas bawah kecuali Anda yakin target perangkat mendukung SharedArrayBuffer (memerlukan header COOP & COEP di server penampung).
2. **Memory Alignment**: Pastikan struct data Anda disejajarkan pada kelipatan 8-byte (misalnya menggunakan compiler attribute `alignas(8)`) agar prosesor ARM dapat memuat data ke cache-line dalam satu siklus clock tanpa penalty overhead pemisahan beban memori.
