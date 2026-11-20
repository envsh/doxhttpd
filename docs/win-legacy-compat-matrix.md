# Windows Legacy Compatibility Matrix (qltox)

Phase 0/1 planning and bring-up guide for **WinXP/Win2000-compatible qltox** builds.
Status: **repo-side skeleton implemented; controlled SDK/dependency closures, real builds, and target-OS validation pending.**

This document is the execution checklist for the 5 CI targets, with emphasis on:
- **x86/x64 both compatible** with legacy Windows baselines (per requirement)
- **Qt3-priority path** for `tde-qt338sh`-based builds, with Qt4 kept compatible where applicable
- **Plan A**: `build-qltox-win32-qt3-win2k` as Win2000 x86 (0x0500) **PoC** target

> Constraints: do not commit automatically; do not parallelize builds (`-j1`); Qt3-first verification for Qt3-related work; Qt4 must remain build-compatible.

---

## 1. Baselines and targets

| CI job | Arch | Minimum OS baseline | Macros (when `CONFIG+=xpcompat`) | PE OS/Subsys cap | Priority / gate |
|---|---|---|---|---:|---|
| `build-qltox-win32-qt4` | x86 | Windows XP SP3 | `_WIN32_WINNT=0x0501`, `WINVER=0x0501`, `_WIN32_IE=0x0501`, `NTDDI_VERSION=0x05010300` | 5.1 | First practical XP-x86 bring-up (Qt4) |
| `build-qltox-win32-qt3` | x86 | Windows XP SP3 | same as above (`XP_TARGET_OS=winxp`) | 5.1 | **Qt3-priority**; blocked on Windows Qt3 toolchain |
| `build-qltox-win32-qt3-win2k` | x86 | Windows 2000 (SP4 assumed for validation) | `_WIN32_WINNT=0x0500`, `WINVER=0x0500`, `_WIN32_IE=0x0501` (tune if needed), `NTDDI_VERSION=0x05000400` (`XP_TARGET_OS=win2k`) | 5.0 | **PoC (Plan A)**; highest dependency risk |
| `build-qltox-win64-qt4` | x86_64 | Windows XP x64 / Server 2003 x64 | `_WIN32_WINNT=0x0502`, `WINVER=0x0502`, `_WIN32_IE=0x0501`, `NTDDI_VERSION=0x05020200` | 5.2 | Self-built Qt4 x64 + deps closure required |
| `build-qltox-win64-qt3` | x86_64 | Windows XP x64 / Server 2003 x64 | same as above | 5.2 | **PoC-gated** (Qt3 x64 feasibility on 0x0502) |

**Notes**
- "Compatible" requires **application + Qt + all shipped DLLs/plugins + CRT/runtime** to satisfy the same baseline (no unsupported imports, no newer PE subsystem, no incompatible runtime family).
- Win2000 has no x86_64 edition; `0x0500` applies only to `win32-qt3-win2k`.
- A binary targeting `0x0500` may run on XP if deps/APIs allow, but treat `win2k` and `xp` as separate validated closures unless proven identical.

---

## 2. Implemented repo artifacts (Phase 0)

| Artifact | Purpose |
|---|---|
| `qltox/qltox.pro` | Opt-in legacy macros when `CONFIG+=xpcompat` or `XP_TARGET_ARCH`/`XP_TARGET_OS` set; guards `-lX11/-ldl` to Unix; adds common Windows system libs for `win32` targets |
| `qltox/build-win32-qt4-xp.sh` | XP x86 Qt4 build template (`-j1`, `xpcompat`, 0x0501) |
| `qltox/build-win64-qt4-xp.sh` | XP x64/2003 Qt4 build template (`-j1`, 0x0502) |
| `qltox/build-win32-qt3-xp.sh` | XP x86 Qt3 build template (`-j1`, 0x0501) |
| `qltox/build-win32-qt3-win2k.sh` | Win2000 x86 Qt3 PoC template (`-j1`, 0x0500) |
| `qltox/build-win64-qt3-xp.sh` | XP x64/2003 Qt3 PoC template (`-j1`, 0x0502) |
| `qltox/winbuild/build-hjson.sh` | Builds hjson-cpp (`libhjson.a` + headers) for the active MSYS2/MinGW toolchain |
| `qltox/winbuild/build-qt4.sh` | Builds Qt 4.8.7 (`win32-g++`, x86/x64) from source; PE 5.1/5.2 via linker flags |
| `qltox/winbuild/build-qt3.sh` | Builds Qt Windows Free Edition 3.3.5-4 (qtwin source, `configure.bat`) for `win32-g++` (x86) and derived `win32-g++-64` (x64, PoC) |
| `qltox/winbuild/package-win.sh` | Collects exe + Qt runtime/plugins/lang and enforces a required-file manifest |
| `scripts/ci_check_pe_xp_compat.sh` | Static PE check: arch, OS/subsystem caps, denylisted APIs, denylisted runtime DLL families (`api-ms-win-*`, `ucrtbase`, `vcruntime14x/msvcp14x`); **advisory only** in CI |
| `.github/workflows/ci.yml` | Single `build-qltox-win` matrix job (5 targets): SDK from cache or self-built, build, manifest-checked packaging, advisory audit, failure diagnostics printed to console, artifacts |
| `docs/win-legacy-compat-matrix.md` | This matrix/checklist |

### 2.1 qmake flags (for manual/controlled builds)
```bash
# XP SP3 x86 (Qt3 or Qt4)
qmake -makefile CONFIG+=xpcompat XP_TARGET_ARCH=x86 XP_TARGET_OS=winxp qltox.pro

# Win2000 x86 PoC (Qt3 only target)
qmake -makefile CONFIG+=xpcompat XP_TARGET_ARCH=x86 XP_TARGET_OS=win2k qltox.pro

# XP x64 / Server 2003
qmake -makefile CONFIG+=xpcompat XP_TARGET_ARCH=x64 XP_TARGET_OS=winxp qltox.pro
```

### 2.2 SDK provisioning (no manual configuration)
CI needs **no** repository variables, secrets or URL settings. The `build-qltox-win`
matrix job resolves the SDK in this order:

1. `actions/cache` hit on `.win-sdk` (key = `qt` + `arch` + hash of `qltox/winbuild/*.sh`)
2. Cache miss → build in-job:
   - `qltox/winbuild/build-hjson.sh $HJSON_ROOT`
   - `qltox/winbuild/build-qt4.sh <arch> $QTDIR <xp-x86|xp-x64>` (source from the USTC Qt mirror, `QT4_SRC_URL` overridable)
   - `qltox/winbuild/build-qt3.sh x86 $QTDIR <xp-x86|win2k-x86>` (`QT3_SRC_URL`/`QT3_FALLBACK_URL` overridable)

Resulting layout (all under `$SDK_ROOT`, default `.win-sdk`):
- `$QTDIR/bin/qmake.exe`, `bin/moc/uic`, `lib`, `include`, Qt DLLs/plugins
- `$HJSON_ROOT/include` + `$HJSON_ROOT/lib/libhjson.a`
- Dependency `pkg-config` files come from MSYS2 packages (`sqlite3`, `curl`, `freetype`, `openal`)

`qltox.pro` consumes `HJSON_ROOT` on Windows and appends `-Wl,--major-subsystem-version`
/`--minor-subsystem-version`/`--major-os-version` per `XP_TARGET_ARCH`/`XP_TARGET_OS`.

---

## 3. Cross-cutting compatibility requirements

### 3.1 Toolchain and runtime
- Prefer **msvcrt-based** MinGW toolchains/runtimes for XP/2003/Win2000 targets; avoid UCRT and `api-ms-win-*`/`vcruntime140+`/`msvcp140+` dependency chains unless explicitly proven compatible for the exact baseline.
- Ensure compiler, binutils (linker subsystem), CRT, Qt, and **every dependency** are built for the same target macros/baseline.
- Lock toolchain versions (GCC/MinGW-w64 release, threading/exception model, libc) to match Qt and deps ABIs.

### 3.2 Subsystem, macros, and imports
- Build defines: `0x0500` (Win2K x86 PoC), `0x0501` (XP x86), `0x0502` (XP x64/2003).
- Validate PE headers and imports for **all shipped binaries**:
```bash
# Static audit (run for every exe/dll in package)
bash scripts/ci_check_pe_xp_compat.sh xp-x86   dist/qltox-win32-qt4/*.{exe,dll}
bash scripts/ci_check_pe_xp_compat.sh xp-x64   dist/qltox-win64-qt4/*.{exe,dll}
bash scripts/ci_check_pe_xp_compat.sh win2k-x86 dist/qltox-win32-qt3-win2k/*.{exe,dll}

# Manual inspection helpers
objdump -f your.exe
objdump -p your.exe | grep -E 'Major(OperatingSystem|Subsystem)Version|Minor(OperatingSystem|Subsystem)Version|DLL Name:'
objdump -p your.exe | grep -E 'GetTickCount64|InitializeCriticalSectionEx|api-ms-win|ucrtbase|vcruntime1|msvcp1'
```
- `scripts/ci_check_pe_xp_compat.sh` is **advisory** in CI (`::warning::`, never fails the job): the denylist is incomplete and produces both false positives and false negatives. The authoritative gate is runtime validation on the target OS.

### 3.3 Dependencies (must be audited as a closure)
`qltox` links/uses (directly or transitively, depending on build options): Qt, curl/OpenSSL (`CPPHTTPLIB_OPENSSL_SUPPORT`), SQLite3, FreeType/HarfBuzz, OpenAL, hjson-cpp, qrencode, gumbo (vendored), X11/dl on Unix only, platform libs on Windows.
- Rebuild or select dependency binaries for each baseline; do not mix modern prebuilt deps into legacy targets.
- OpenSSL/curl/TLS: legacy OS crypto stacks and modern TLS requirements conflict; prefer dependency versions/protocol configs that can run on target while meeting functional needs (record exact versions and ciphers/backends; validate at runtime).
- Prefer minimizing external DLL count for deployment reliability (static or bundled audited DLLs as feasible).

### 3.4 Packaging
- Package must include: `qltox.exe`, required Qt DLLs/plugins, all third-party runtime DLLs, `lang/*.json` and any resources/assets required at runtime.
- Audit every packaged PE file (not only the main exe). CI skeleton attempts best-effort DLL copy from build output and QTDIR/deps bin dirs; refine per SDK.

---

## 4. Per-target bring-up plans

### 4.1 `build-qltox-win32-qt4` (XP SP3 x86, 0x0501) — **first Qt4 baseline**
**Goal:** reproducible XP-x86 Qt4 build with full PE audit.

**Recommended candidate closure (to validate):**
- Qt 4.8.7 x86 MinGW build known to target XP-era environments (e.g. official `qt-opensource-windows-x86-mingw482-4.8.7` or audited Fsu0413 x86 MinGW 4.9.4 Qt4.8.7 package), **plus** ABI-matched MinGW32 toolchain (msvcrt).
- All deps rebuilt/selected for 0x0501 + same ABI: curl, OpenSSL (legacy-compatible branch/version as required), SQLite3, FreeType, HarfBuzz, OpenAL-soft/runtime, hjson-cpp, qrencode.
- Avoid MSYS2/modern MXE binaries unless individually audited for 5.1/imports/runtime.

**Bring-up steps:**
1. CI builds Qt 4.8.7 x86 from source (`qltox/winbuild/build-qt4.sh x86`), then qltox via `qltox/build-win32-qt4-xp.sh`.
2. Confirm `qltox/winbuild/package-win.sh` finds `qt4` runtime (`QtCore4.dll`, `QtGui4.dll`) and `scripts/ci_check_pe_xp_compat.sh xp-x86` produces no warnings.
3. Fix `qltox.pro` discovery or script env if needed (pkg-config paths, OpenSSL/OpenAL locations) **without breaking other targets**.
4. Ensure `qltox/build-win32-qt4-xp.sh` produces `qltox.exe`; include required DLLs/plugins/lang in `dist/qltox-win32-qt4`.
5. Pass `scripts/ci_check_pe_xp_compat.sh xp-x86` for all packaged binaries.
6. Validate on XP SP3 x86 (VM/real): startup, UI, network/TLS flows used by qltox, media/audio as applicable.

**Key risks:** hidden transitive deps importing Vista+ APIs; OpenSSL/curl TLS compatibility on XP.

### 4.2 `build-qltox-win32-qt3` (XP SP3 x86, 0x0501) — **Qt3 priority**
**Goal:** Windows x86 `tde-qt338sh`-based qltox build for XP SP3.

**Prerequisites (blocking):**
- Windows-portable Qt Windows Free Edition 3.3.5-4 (qtwin) built with MSYS2 MinGW32 targeting 0x0501 (or 0x0500 for the Win2000 PoC).
- Matching dependency closure for 0x0501 (`sqlite3`, `curl`, `freetype`, `openal` from MSYS2 packages; ABI/XP audit still required).

**Bring-up steps:**
1. CI builds Qt Windows Free Edition 3.3.5-4 x86 in-job (`qltox/winbuild/build-qt3.sh x86`), so nothing has to be hosted or configured.
2. Validate Qt3 tools run in MSYS2/MINGW32 and `qltox.pro` Qt3 detection path works.
3. Build via `qltox/build-win32-qt3-xp.sh` (`-j1`), resolve Windows/MinGW-specific compile or link issues while preserving Qt3/Qt4 compatibility.
4. Package+audit (`xp-x86`) all binaries; validate on XP SP3 x86.
5. After Qt3 path is stable, re-check Qt4 x86 path for regressions if shared code changed.

**Key risks:** `tde-qt338sh` Windows port completeness, qmake/mkspec behavior, C++98/Qt3 constraints in Windows-specific code.

### 4.3 `build-qltox-win32-qt3-win2k` (Win2000 x86, 0x0500) — **PoC (Plan A)**
**Goal:** determine feasibility and, if possible, produce a Win2000-runnable Qt3 qltox artifact.

**Prerequisites (blocking):**
- Qt3 x86 toolchain/deps capable of clean 0x0500 targeting (may overlap with XP x86 or require separate closure).
- Dependency set with no Win2000-incompatible APIs/imports/runtime (highest uncertainty: OpenSSL/curl/OpenAL and transitive libs).

**Bring-up steps:**
1. CI builds the same Qt 3.3.5-4 with `XP_TARGET_OS=win2k` (PE 5.0) — no separate hosted SDK.
2. Build via `qltox/build-win32-qt3-win2k.sh`; iterate on deps and flags until `win2k-x86` audit passes for all shipped binaries.
3. Extend `scripts/ci_check_pe_xp_compat.sh` denylist based on observed imports/failures (XP-only APIs, additional runtime issues).
4. Runtime validation on Windows 2000 x86 (recommend SP4 baseline): load/startup, core workflows, document unsupported features if deps force degradation.
5. Record go/no-go and exact closure in `docs/win-legacy-compat-matrix.md` validation log.

**Key risks:** no viable modern TLS stack on stock Win2000; third-party libs dropped 0x0500; heuristic audit insufficient. Treat as **PoC until runtime-proven**.

### 4.4 `build-qltox-win64-qt4` (XP x64 / Server 2003, 0x0502)
**Goal:** reproducible x64 legacy build despite lack of off-the-shelf XP-x64-compatible Qt4.8.7 bundle.

**Prerequisites (blocking):**
- Self-built `x86_64-w64-mingw32` toolchain environment targeting 0x0502 (msvcrt) and Qt 4.8.7 x64 built from `qt-everywhere-opensource-src-4.8.7` for that toolchain.
- Full 0x0502 dependency closure (curl/OpenSSL/SQLite/FreeType/HarfBuzz/OpenAL/qrencode/hjson/gumbo as needed).

**Bring-up steps:**
1. CI builds Qt 4.8.7 x64 in-job (`qltox/winbuild/build-qt4.sh x64`, `win32-g++-64` derived from `win32-g++`), including Qt host tools/plugins.
2. Enable job, resolve x64-specific build issues in `qltox.pro`/deps discovery; keep serial builds.
3. Package all x64 runtime DLLs/plugins; pass `xp-x64` audit on every binary.
4. Validate on XP x64 / Windows Server 2003 x64.

**Key risks:** Qt4.8.7 x64 MinGW portability patches, dependency availability for 0x0502, WoW64/driver differences irrelevant (native x64) but API baseline strict.

### 4.5 `build-qltox-win64-qt3` (XP x64 / Server 2003, 0x0502) — **PoC-gated**
**Goal:** prove or disprove Qt3/tde-qt338sh viability for x64 + 0x0502.

**PoC questions (must answer before full bring-up):**
- Can `tde-qt338sh` (and required tools/libs) be built for x86_64 Windows/MinGW-w64 with 0x0502 compatibility?
- Are pointer-size/64-bit assumptions in Qt3 patches and qltox/qlcomp code compatible?
- Can required dependencies be provided as a coherent 0x0502 x64 closure?

**Steps:** the matrix entry runs non-blocking (`poc: true`). `build-qt3.sh` derives `mkspecs/win32-g++-64` from `win32-g++` and builds Qt 3.3.5-4 with the x86_64 toolchain. Expect pointer-size/`long` assumptions in Qt3 code to need patches; audit the produced binaries, then runtime-check on Server 2003/XP x64. Until runtime-proven, the target stays PoC.

---

## 5. Execution checklist (when implementing with SDKs available)

### 5.1 Per job bring-up
- [ ] First run populates the `.win-sdk` cache (SDK self-build); subsequent runs hit cache.
- [ ] `push` build succeeds with `-j1` and no manual repository variables.
- [ ] `qltox.pro`/scripts adjusted only as needed for target SDK discovery; no regressions to existing Linux/mac/Qt3-Linux paths.
- [ ] Package includes all runtime DLLs, Qt plugins, and `resources/lang`, and passes the `package-win.sh` manifest.
- [ ] `scripts/ci_check_pe_xp_compat.sh` warnings reviewed for **every** packaged `exe/dll` (advisory).
- [ ] Build log shows baseline, SDK identity (`qtdir=`), and binary hashes (printed by `package-win.sh`).
- [ ] VM/real validation completed and logged.

### 5.2 Verification order (recommended, Qt3-priority aware)
1. `win32-qt4` (0x0501) — validate CI/audit/packaging pipeline end-to-end.
2. `win32-qt3` (0x0501) — Qt3 Windows baseline (**build/verify Qt3 first**).
3. `win32-qt3-win2k` (0x0500) — PoC extension.
4. `win64-qt4` (0x0502).
5. `win64-qt3` (0x0502) — after PoC go-decision.

(If Qt3 Windows toolchain becomes ready before Qt4 x86 closure, prioritize step 2 before/alongside step 1, then keep Qt4 in sync.)

---

## 6. Open blockers (must be closed or explicitly waived)

- [ ] **SDK builds reproducible**: `qltox/winbuild/build-qt4.sh` / `build-qt3.sh` succeed on a clean `windows-2022` runner and cache correctly.
- [ ] **Windows x86 Qt 3.3.5-4 (qtwin)**: build/port status, 0x0501 and 0x0500 capability (GCC compatibility patches).
- [ ] **Qt4.8.7 x64 0x0502 closure**: self-build recipe and audited deps.
- [ ] **Qt3 x64 0x0502**: feasibility PoC outcome.
- [ ] **Win2000 0x0500 deps**: TLS/crypto/network/audio feasibility and supported feature subset.
- [ ] **PE/import coverage**: extend denylist from real artifacts; audit all shipped DLLs/plugins.
- [ ] **Runtime environments** available for acceptance: XP SP3 x86, XP x64/Server 2003 x64, Win2000 x86.

---

## 7. Validation log

| Date | Job/target | Baseline | SDK/toolchain/deps (rev/hash) | Static audit | Runtime env | Result | Notes |
|---|---|---|---|---|---|---|---|
| YYYY-MM-DD | build-qltox-win32-qt4 | xp-x86 5.1 |  | pass/fail (N files) | XP SP3 x86 | pass/fail |  |
| YYYY-MM-DD | build-qltox-win32-qt3 | xp-x86 5.1 |  |  | XP SP3 x86 |  |  |
| YYYY-MM-DD | build-qltox-win32-qt3-win2k | win2k-x86 5.0 |  |  | Win2000 x86 (SP4) |  | PoC scope/limits |
| YYYY-MM-DD | build-qltox-win64-qt4 | xp-x64 5.2 |  |  | XP x64 / 2003 x64 |  |  |
| YYYY-MM-DD | build-qltox-win64-qt3 | xp-x64 5.2 |  |  | XP x64 / 2003 x64 |  | PoC gate |