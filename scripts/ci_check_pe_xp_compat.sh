#!/usr/bin/env bash
# Static XP/Win2000-compatibility check for Windows PE binaries (CI gate).
#
# Usage:
#   ci_check_pe_xp_compat.sh <win2k-x86|xp-x86|xp-x64|x86|x64> <binary.exe|dll> [binary2 ...]
#
# Targets:
#   xp-x86    | x86        -> Windows XP SP3 x86 baseline         (OS/subsystem 5.1)
#   xp-x64    | x64        -> Windows XP x64 / Server 2003 x64   (OS/subsystem 5.2)
#   win2k-x86 | win2000-x86| 2k-x86 -> Windows 2000 x86          (OS/subsystem 5.0)
#
# Checks (per file):
#   - PE machine/format matches target arch
#   - Major/Minor OperatingSystemVersion and SubsystemVersion do not exceed baseline
#   - Import table denylist for APIs commonly unavailable on the target baseline
#   - Import table denylist for runtime families incompatible with XP/Win2000 baselines
#     (e.g. api-ms-win-*, ucrtbase, vcruntime140+, msvcp140+) as hard errors
#
# Notes:
# - Denylists are necessary but NOT exhaustive (especially for Win2000 vs XP-only APIs).
# - Always validate on real/VM target systems and audit all shipped EXE/DLLs.
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
usage: ci_check_pe_xp_compat.sh <win2k-x86|xp-x86|xp-x64|x86|x64> <binary> [binary...]

Targets:
  xp-x86, x86      Windows XP SP3 x86 (5.1)
  xp-x64, x64      Windows XP x64 / Server 2003 x64 (5.2)
  win2k-x86        Windows 2000 x86 (5.0, PoC)
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

mode="${1:-}"
shift || true

case "$mode" in
  x86|xp-x86)      target="xp-x86" ;;
  x64|xp-x64)      target="xp-x64" ;;
  win2k-x86|win2000-x86|2k-x86) target="win2k-x86" ;;
  *)
    usage
    exit 2
    ;;
esac

if [[ $# -lt 1 ]]; then
  echo "error: no binary specified" >&2
  usage
  exit 2
fi

case "$target" in
  xp-x86)
    want_machine='i386|I386|80386|8664?'
    want_os_major=5; want_os_minor=1
    want_sub_major=5; want_sub_minor=1
    ;;
  xp-x64)
    want_machine='x86-64|AMD64|x64'
    want_os_major=5; want_os_minor=2
    want_sub_major=5; want_sub_minor=2
    ;;
  win2k-x86)
    want_machine='i386|I386|80386|8664?'
    want_os_major=5; want_os_minor=0
    want_sub_major=5; want_sub_minor=0
    ;;
esac

# Common Vista+ imports that break XP/2003/Win2000 targets (non-exhaustive; extend as needed).
forbidden_base_re='GetTickCount64|InitializeCriticalSectionEx|InitializeConditionVariable|SleepConditionVariableCS|SleepConditionVariableSRW|WakeConditionVariable|WakeAllConditionVariable|InitializeSRWLock|AcquireSRWLockExclusive|AcquireSRWLockShared|ReleaseSRWLockExclusive|ReleaseSRWLockShared|TryAcquireSRWLockExclusive|TryAcquireSRWLockShared|GetFileInformationByHandleEx|SetFileInformationByHandle|GetFinalPathNameByHandleW|GetLogicalProcessorInformation|GetLogicalProcessorInformationEx|CancelIoEx|GetOverlappedResultEx|SetThreadErrorMode|GetCurrentThreadStackLimits|CreateFile2|CompareStringEx|LCMapStringEx|GetLocaleInfoEx|GetDateFormatEx|GetTimeFormatEx|LocaleNameToLCID|LCIDToLocaleName|FlsAlloc|FlsFree|FlsGetValue|FlsSetValue|ResolveLocaleName|QueryFullProcessImageNameA|QueryFullProcessImageNameW|K32EnumProcesses|K32EnumProcessModules|K32GetModuleFileNameExA|K32GetModuleFileNameExW'

# Heuristic denylist for APIs commonly unavailable on stock Windows 2000 (often XP/XP-SPx/2003+).
# Subsystem/OS version 5.0 is required but not sufficient; availability varies by SP/hotfixes.
forbidden_win2k_extra_re='AddVectoredExceptionHandler|RemoveVectoredExceptionHandler|GetDllDirectoryA|GetDllDirectoryW|SetDllDirectoryA|SetDllDirectoryW|GetModuleHandleExA|GetModuleHandleExW|GetProcessHandleCount|GetProcessId|GetSystemTimes|IsWow64Process|RtlCaptureStackBackTrace|SetThreadStackGuarantee|EncodePointer|DecodePointer|HeapSetInformation|GetSystemWow64DirectoryA|GetSystemWow64DirectoryW|GetFinalPathNameByHandleA'

# Runtime/DLL families that typically indicate a toolchain/runtime newer than legacy XP/Win2000 baselines.
# (UCRT/api-ms-win-core-* and VC14+ runtime deps usually break unless a specially supported setup is proven.)
forbidden_runtime_re='(^|[[:space:]])api-ms-win-|ucrtbase\.dll|vcruntime1[4-9][0-9]?\.dll|msvcp1[4-9][0-9]?\.dll|vccorlib1[4-9][0-9]?\.dll|concrt1[4-9][0-9]?\.dll'

forbidden_re="$forbidden_base_re"
forbidden_label="Vista+/newer-than-target API"
if [[ "$target" == "win2k-x86" ]]; then
  forbidden_re="${forbidden_base_re}|${forbidden_win2k_extra_re}"
  forbidden_label="API unsupported on Win2000 target (Vista+ or common XP/2003+ heuristic)"
fi

fail=0
have_objdump=0
command -v objdump >/dev/null 2>&1 && have_objdump=1
if [[ "$have_objdump" -ne 1 ]]; then
  echo "error: objdump not found in PATH" >&2
  exit 127
fi

for bin in "$@"; do
  echo "== XP/legacy compat check ($target): $bin =="
  if [[ ! -f "$bin" ]]; then
    echo "error: missing file: $bin" >&2
    fail=1
    continue
  fi

  dump_p="$(objdump -p "$bin" 2>&1 || true)"
  dump_f="$(objdump -f "$bin" 2>&1 || true)"
  if [[ -z "$dump_p" && -z "$dump_f" ]]; then
    echo "error: objdump produced no output for $bin" >&2
    fail=1
    continue
  fi
  dump="$dump_p"$'\n'"$dump_f"

  machine_line="$(echo "$dump_f"$'\n'"$dump_p" | grep -Ei '^[[:space:]]*(Machine:|architecture:|file format)' | head -n 5 || true)"
  if [[ -z "$machine_line" ]]; then
    echo "error: could not determine PE machine/format" >&2
    fail=1
  elif ! echo "$machine_line" | grep -Eiq "($want_machine|pei-i386|pei-x86-64)"; then
    echo "error: unexpected PE machine/format for $target target:" >&2
    echo "$machine_line" | sed 's/^/  /' >&2
    fail=1
  else
    # Ensure format aligns with intended arch when recognizable
    if [[ "$target" == "xp-x64" ]] && echo "$machine_line" | grep -Eiq 'pei-i386([^-\]|$)|i386'; then
      if ! echo "$machine_line" | grep -Eiq 'x86-64|pei-x86-64|AMD64'; then
        echo "error: expected x64 PE, detected x86 format" >&2
        echo "$machine_line" | sed 's/^/  /' >&2
        fail=1
      fi
    fi
    if [[ "$target" != "xp-x64" ]] && echo "$machine_line" | grep -Eiq 'pei-x86-64|x86-64|AMD64'; then
      echo "error: expected x86 PE, detected x64 format" >&2
      echo "$machine_line" | sed 's/^/  /' >&2
      fail=1
    fi
  fi

  os_ver="$(echo "$dump" | awk '/MajorOperatingSystemVersion/{print $2; exit}')"
  os_min="$(echo "$dump" | awk '/MinorOperatingSystemVersion/{print $2; exit}')"
  sub_ver="$(echo "$dump" | awk '/MajorSubsystemVersion/{print $2; exit}')"
  sub_min="$(echo "$dump" | awk '/MinorSubsystemVersion/{print $2; exit}')"

  if [[ -n "${os_ver:-}" && -n "${sub_ver:-}" ]]; then
    os_min="${os_min:-0}"; sub_min="${sub_min:-0}"
    echo "info: PE version OS=${os_ver}.${os_min} Subsystem=${sub_ver}.${sub_min} (limit ${want_os_major}.${want_os_minor}/${want_sub_major}.${want_sub_minor})"
    if (( os_ver > want_os_major || (os_ver == want_os_major && os_min > want_os_minor) )); then
      echo "error: OS version ${os_ver}.${os_min} exceeds ${want_os_major}.${want_os_minor}" >&2
      fail=1
    fi
    if (( sub_ver > want_sub_major || (sub_ver == want_sub_major && sub_min > want_sub_minor) )); then
      echo "error: Subsystem version ${sub_ver}.${sub_min} exceeds ${want_sub_major}.${want_sub_minor}" >&2
      fail=1
    fi
  else
    echo "warn: could not parse subsystem/OS version fields (verify manually)" >&2
  fi

  imports="$(echo "$dump_p" | awk '/DLL Name:/{p=1} p{print}' || true)"
  [[ -z "$imports" ]] && imports="$(echo "$dump" | awk '/DLL Name:/{p=1} p{print}' || true)"
  if [[ -n "$imports" ]]; then
    bad="$(echo "$imports" | grep -Eo "$forbidden_re" | sort -u || true)"
    if [[ -n "$bad" ]]; then
      echo "error: forbidden imports detected ($forbidden_label):" >&2
      echo "$bad" | sed 's/^/  - /' >&2
      fail=1
    fi

    bad_rt="$(echo "$imports" | grep -Eio "$forbidden_runtime_re" | sed 's/^[[:space:]]*//' | sort -u || true)"
    if [[ -n "$bad_rt" ]]; then
      echo "error: runtime/DLL dependencies likely incompatible with legacy target:" >&2
      echo "$bad_rt" | sed 's/^/  - /' >&2
      fail=1
    fi
  fi

  if [[ "$fail" -eq 0 ]]; then
    echo "ok: $bin"
  fi
done

exit "$fail"
