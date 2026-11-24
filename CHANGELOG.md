# Changelog

All notable changes across the **`@awesome-os/node`** Ghost CAS launcher and its tracked upstream Node.js release lines (**Node 27 `ABI 153`** & **Node 26 `ABI 147`**).

---

## `v0.1.0` — 2026-11-24

### `@awesome-os/node` Ghost Launcher & CAS Manifest
- **Dual-line ABI support:** Native ~25 KB C11 Ghost CAS launcher (`src/node_launcher.c`) hydrating official debloated Node.js runtimes once from [`awesome-os/b8g-ghost`](https://github.com/awesome-os/b8g-ghost) (`cas-v1`) into `~/.b8g/cas/node/<sha256>/`.
  - **Default (`B8G_NODE_LINE=27`):** Node.js `v27.0.0-alpha.0.1.1` (`NODE_MODULE_VERSION = 153`, V8 `15.6.75.50-node.14`).
  - **Stable (`B8G_NODE_LINE=26`):** Node.js `v26.13.1` (`NODE_MODULE_VERSION = 147`, V8 `14.6.202.34-node.40`).
- **Zero shims / zero bootstrap:** Self-hydrates using OS-native `curl` + `tar` (`zstd`) with verbatim `argv` forwarding and exit-code propagation.

### Upstream Node.js 27 Alpha Highlights (`v27.0.0-alpha.0.0.0` → `v27.0.0-alpha.0.1.1`)

#### V8 `14.6` → `15.6.75.50`
- **New JS built-ins (default on):** `Math.sumPrecise()`, `Intl.Locale.prototype.variants`, `Iterator.prototype.join()`, `Iterator.prototype.includes()`, `Iterator.zip()`, `Iterator.zipKeyed()`.
- **Experimental flags:** `import defer` (`--js-defer-import-eval`), Immutable `ArrayBuffer` (`--js-immutable-arraybuffer`), RegExp buffer boundaries (`--js-regexp-buffer-boundaries`).
- **Heap & memory model:** Default `--max-old-space-size` is now 50% of physical RAM clamped to `256 MB – 4 GB`; `Buffer` and `ArrayBuffer` memory now counts toward the heap limit.
- **Serialization & compile cache:** `v8.serialize()`, `structuredClone()`, and `postMessage()` use a new wire format (not readable by Node ≤26); `v8.cachedDataVersionTag()` and compile caches now rotate on every V8 patch.
- **Behavior & performance:** Fewer microtasks for `await`; `Array.prototype.sort()` uses PowerSort for large arrays; `Math` uses LLVM `libm`; `asm.js` no longer compiles to WebAssembly; `--experimental-wasm-*` renamed to `--wasm-*`.

#### Core APIs, Crypto & Networking
- **`fs`:** Native C++ `fs.glob` implementation (`#65392`); `fs.mkdtemp` returns a `Buffer` when given a `Buffer` prefix.
- **`crypto` / OpenSSL `4.1.0`:** Upgraded to OpenSSL 4.1.0, removed legacy OpenSSL 1.x / engine / bundled FIPS paths; added X.509 certificate generation with CSR support (`#67245`), cross-isolate X.509 chain cloning, and cached HMAC/digest contexts (`#67109`).
- **`http` / `http2` / `dns`:** `req.headers` uses a `null` prototype (`#62900`); default `keepAliveTimeout` raised to `65s` (`#62782`); HTTP parser batches body spans (`#67187`); HTTP/2 default window sizes increased (`#64623`); concurrent identical `dns.lookup()` calls are coalesced (`#62599`).
- **`worker_threads` & SEA:** Workers preload streams into the startup snapshot (`#66311`); Single Executable Applications generate a compile cache for VFS modules (`#66768`).
- **`child_process` & `domain`:** Added `subprocess.timedOut` (`#66639`); `domain` ported onto `AsyncLocalStorage` and runtime-deprecated.
- **Toolchain & platform (`alpha.0.1.1`):** Linux PIE enabled by default (`#67386`); macOS `bin/node` notarized in tarballs (`#67143`); Intel macOS downgraded to experimental.
- **Bundled dependencies:** `amaro 1.3.0`, `undici 8.12.4`, `simdjson 5.1.1`, `nghttp2 1.71.0`, `nghttp3 1.19.0`, `ngtcp2 1.26.0`, `llhttp 9.4.4`, `npm 12.2.2`.
