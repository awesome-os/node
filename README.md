# `@awesome-os/node` — Dual-Line Ghost Node.js Launcher (Node 27 `ABI 153` & Node 26 `ABI 147`)

Drop-in native C11 `node.exe` / `node` launcher (~60 KB, zero shell shims, zero Node bootstrap dependency) that hydrates official debloated Node.js runtimes **once** from [`awesome-os/b8g-ghost`](https://github.com/awesome-os/b8g-ghost) (`cas-v1`) into `~/.b8g/cas/node/<sha256>/`.

## Supported Release Lines & ABI Manifest

| Release Line | Channel | `NODE_MODULE_VERSION` (ABI) | Selection |
| :--- | :--- | :--- | :--- |
| **Node.js 27** (`v27.0.0-alpha.0.1.1+`) | `alpha` (default) | **`153`** | Default (`B8G_NODE_LINE=27`) |
| **Node.js 26** (`v26.13.1+`) | `stable` | **`147`** | `B8G_NODE_LINE=26` |

## Usage

```powershell
# Default: Node 27 (ABI 153)
node -v
node -p "process.versions.modules"   # -> 153

# Switch to Node 26 stable (ABI 147) on demand
$env:B8G_NODE_LINE = "26"
node -v
node -p "process.versions.modules"   # -> 147

# Inspect launcher & CAS state
node --ghost-info
```

## Build (`holo-compiler` / `zig cc`)

```powershell
# Win64 PE
zig cc -target x86_64-windows-gnu -O2 -s src/node_launcher.c -o dist/node.exe

# Linux x64 ELF64
zig cc -target x86_64-linux-musl -O2 -s src/node_launcher.c -o dist/node-linux-x64

# Linux arm64 ELF64
zig cc -target aarch64-linux-musl -O2 -s src/node_launcher.c -o dist/node-linux-arm64
```
