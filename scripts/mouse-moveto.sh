#!/usr/bin/env bash
# mouse-moveto.sh — 相对移动光标（for deskflow keyboard control）
# Deskflow 靠光标撞屏幕边缘切换主机，本脚本用于一次把光标推过屏幕边界。
# 用法: mouse-moveto.sh l|r [像素]     默认 2000px
#       mouse-moveto.sh left|right [像素]
set -u

usage() {
    echo "用法: $(basename "$0") l|r [px]    # 例: $(basename "$0") r 2000" >&2
    exit 2
}

[ $# -ge 1 ] || usage
dir="$1"
px="${2:-2000}"
case "$dir" in
    l|L|left|LEFT)   sign=-1 ;;
    r|R|right|RIGHT) sign=1  ;;
    *) usage ;;
esac
case "$px" in
    ''|*[!0-9]*) echo "像素必须是正整数: $px" >&2; exit 2 ;;
esac
dx=$(( sign * px ))

os="$(uname -s 2>/dev/null || echo unknown)"

move_x11() {
    if command -v xdotool >/dev/null 2>&1; then
        xdotool mousemove_relative -- "$dx" 0
        return $?
    fi
    if command -v xte >/dev/null 2>&1; then
        xte "mousermove $dx 0"
        return $?
    fi
    echo "X11 需要 xdotool 或 xte（apt install xdotool / xautomation）" >&2
    return 1
}

move_wayland() {
    if command -v ydotool >/dev/null 2>&1; then
        ydotool mousemove "$dx" 0
        return $?
    fi
    echo "Wayland 需要 ydotool + ydotoold（协议不允许应用直接 warp 光标）" >&2
    return 1
}

move_macos() {
    if command -v cliclick >/dev/null 2>&1; then
        cliclick "m:+$dx,+0"
        return $?
    fi
    # 回退：CoreGraphics（系统自带 python3 + ctypes）
    /usr/bin/python3 - "$dx" <<'PY'
import ctypes, sys
dx = float(sys.argv[1])
cg = ctypes.CDLL('/System/Library/Frameworks/ApplicationServices.framework/ApplicationServices')

class CGPoint(ctypes.Structure):
    _fields_ = [('x', ctypes.c_double), ('y', ctypes.c_double)]

cg.CGEventCreate.restype = ctypes.c_void_p
cg.CGEventCreate.argtypes = [ctypes.c_void_p]
cg.CGEventGetLocation.restype = CGPoint
cg.CGEventGetLocation.argtypes = [ctypes.c_void_p]
cg.CGWarpMouseCursorPosition.restype = ctypes.c_int
cg.CGWarpMouseCursorPosition.argtypes = [CGPoint]
cg.CGAssociateMouseAndMouseCursorPosition.restype = ctypes.c_int
cg.CGAssociateMouseAndMouseCursorPosition.argtypes = [ctypes.c_int]

ev = cg.CGEventCreate(None)
p = cg.CGEventGetLocation(ev)
cg.CGWarpMouseCursorPosition(CGPoint(p.x + dx, p.y))
cg.CGAssociateMouseAndMouseCursorPosition(1)
PY
}

move_windows() {
    if command -v powershell.exe >/dev/null 2>&1; then
        powershell.exe -NoProfile -Command "
            Add-Type -AssemblyName System.Windows.Forms;
            \$p=[System.Windows.Forms.Cursor]::Position;
            [System.Windows.Forms.Cursor]::Position = New-Object System.Drawing.Point((\$p.X + $dx), \$p.Y)
        "
        return $?
    fi
    echo "Windows 需要 powershell.exe 在 PATH 中（Git Bash/MSYS 环境）" >&2
    return 1
}

case "$os" in
    Darwin)
        move_macos
        ;;
    MINGW*|MSYS*|CYGWIN*)
        move_windows
        ;;
    Linux*)
        if [ "${XDG_SESSION_TYPE:-}" = "wayland" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; then
            move_wayland
        elif [ -n "${DISPLAY:-}" ]; then
            move_x11
        else
            echo "未检测到 DISPLAY（X11）或 WAYLAND_DISPLAY，无法移动光标" >&2
            exit 1
        fi
        ;;
    *)
        if [ "${OS:-}" = "Windows_NT" ]; then
            move_windows
        else
            echo "不支持的平台: $os" >&2
            exit 1
        fi
        ;;
esac
