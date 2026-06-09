#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# VS Code + WSL + LeetCode 初始化脚本
#
# 推荐位置：
#   ~/study/scripts/setup-leetcode-wsl.sh
#
# 目标结构：
#   ~/study
#   ├── leetcode/.clangd
#   └── scripts/setup-leetcode-wsl.sh
#
# LeetCode 插件配置写入：
#   ~/.vscode-server/data/Machine/settings.json
# ============================================================

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
LEETCODE_DIR="$WORKSPACE_DIR/leetcode"

REMOTE_USER_SETTINGS_DIR="$HOME/.vscode-server/data/Machine"
REMOTE_USER_SETTINGS_FILE="$REMOTE_USER_SETTINGS_DIR/settings.json"

EXTENSION_ID="LeetCode.vscode-leetcode"

echo "==> 当前用户: $(whoami)"
echo "==> HOME: $HOME"
echo "==> 脚本目录: $SCRIPT_DIR"
echo "==> study 工作区: $WORKSPACE_DIR"
echo "==> LeetCode 目录: $LEETCODE_DIR"
echo "==> VS Code Remote User Settings: $REMOTE_USER_SETTINGS_FILE"

echo
echo "==> 检查 Node.js"
if ! command -v node >/dev/null 2>&1; then
    echo "未找到 node，请先安装："
    echo "sudo apt update && sudo apt install -y nodejs npm"
    exit 1
fi
node -v

echo
echo "==> 检查 code 命令"
if ! command -v code >/dev/null 2>&1; then
    echo "未找到 code 命令。请先从 Windows VS Code 连接一次 WSL。"
    exit 1
fi

echo
echo "==> 创建目录"
mkdir -p "$LEETCODE_DIR"
mkdir -p "$HOME/.local/bin"
mkdir -p "$REMOTE_USER_SETTINGS_DIR"

echo
echo "==> 安装或确认 WSL 侧 LeetCode 扩展"
code --install-extension "$EXTENSION_ID" --force >/dev/null || true

echo
echo "==> 创建动态 lcv 包装命令"

cat > "$HOME/.local/bin/lcv" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

EXT_DIR="$(find "$HOME/.vscode-server/extensions" -maxdepth 1 -type d -iname "leetcode.vscode-leetcode-*" 2>/dev/null | sort -V | tail -n 1 || true)"

if [[ -z "$EXT_DIR" ]]; then
    echo "未找到 WSL 侧 LeetCode 扩展目录：$HOME/.vscode-server/extensions/leetcode.vscode-leetcode-*"
    exit 1
fi

LCV_BIN="$EXT_DIR/node_modules/vsc-leetcode-cli/bin/leetcode"

if [[ ! -f "$LCV_BIN" ]]; then
    echo "未找到 vsc-leetcode-cli：$LCV_BIN"
    exit 1
fi

exec node "$LCV_BIN" "$@"
EOF

chmod +x "$HOME/.local/bin/lcv"

if ! echo "$PATH" | tr ':' '\n' | grep -qx "$HOME/.local/bin"; then
    echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$HOME/.bashrc"
    export PATH="$HOME/.local/bin:$PATH"
fi

echo "lcv 路径: $(command -v lcv)"

echo
echo "==> 启用 leetcode.cn 插件"
lcv plugin -e leetcode.cn || true

echo
echo "==> 当前 CLI 插件状态"
lcv plugin || true

echo
echo "==> 写入 LeetCode 插件 Remote User Settings"

python3 - "$REMOTE_USER_SETTINGS_FILE" "$LEETCODE_DIR" <<'PY'
import json
import os
import sys
from pathlib import Path

settings_file = Path(sys.argv[1])
leetcode_dir = sys.argv[2]

settings_file.parent.mkdir(parents=True, exist_ok=True)

if settings_file.exists():
    raw = settings_file.read_text(encoding="utf-8").strip()
    if raw:
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            backup = settings_file.with_suffix(".json.bak")
            backup.write_text(raw, encoding="utf-8")
            print(f"原 settings.json 不是合法 JSON，已备份到: {backup}")
            data = {}
    else:
        data = {}
else:
    data = {}

# 只写 LeetCode 插件相关配置，避免覆盖你的通用工作区设置
data["leetcode.endpoint"] = "leetcode-cn"
data["leetcode.workspaceFolder"] = leetcode_dir
data["leetcode.defaultLanguage"] = "cpp"
data["leetcode.filePath"] = {
    "default": {
        "folder": "",
        "filename": "${id}.${kebab-case-name}.${ext}"
    }
}
data["leetcode.hideSolved"] = True
data["leetcode.hint.commentDescription"] = False
data["leetcode.hint.commandShortcut"] = False
data["leetcode.hint.configWebviewMarkdown"] = False

# WSL Remote 中不要启用 useWsl，避免 Windows 本体模式和 Remote 模式混用
if "leetcode.useWsl" in data:
    del data["leetcode.useWsl"]

settings_file.write_text(
    json.dumps(data, ensure_ascii=False, indent=2) + "\n",
    encoding="utf-8"
)

print(f"已写入: {settings_file}")
print(f"leetcode.workspaceFolder = {leetcode_dir}")
PY

echo
echo "==> 写入 LeetCode 目录专用 .clangd"

cat > "$LEETCODE_DIR/.clangd" <<'EOF'
CompileFlags:
  Add:
    - -std=c++17
    - -Wall
    - -Wextra
    - -Wno-unused-variable
    - -Wno-unused-parameter
    - -Wno-unused-function
    - -Wno-unused-but-set-variable
    - -Wno-sign-compare
    - -Wno-unknown-pragmas
    - -Wno-unknown-warning-option
    - -Wno-format
    - -Wno-shadow

Diagnostics:
  Suppress:
    - unused-includes
    - missing-includes
  ClangTidy:
    Remove:
      - "*"

InlayHints:
  ParameterNames: No
  DeducedTypes: No
EOF

echo "已写入: $LEETCODE_DIR/.clangd"

echo
echo "========================================"
echo "准备登录 leetcode.cn"
echo
echo "1. Windows 浏览器打开："
echo "   https://leetcode.cn"
echo
echo "2. 确认已登录"
echo
echo "3. F12 -> Network -> 刷新页面"
echo
echo "4. 点击 graphql 或任意 leetcode.cn 请求"
echo
echo "5. 复制 Request Headers 中 Cookie 的值"
echo
echo "Cookie 格式类似："
echo "csrftoken=xxx; LEETCODE_SESSION=xxx; ..."
echo
echo "不要带前缀：Cookie:"
echo "========================================"
echo

read -rp "请输入力扣用户名 login: " LC_LOGIN

echo
echo "请粘贴 leetcode.cn Cookie，然后回车："
read -r LC_COOKIE

if [[ -z "$LC_LOGIN" || -z "$LC_COOKIE" ]]; then
    echo "login 或 cookie 为空，终止。"
    exit 1
fi

echo
echo "==> 执行 WSL 内置 CLI Cookie 登录"
printf "%s\n%s\n" "$LC_LOGIN" "$LC_COOKIE" | lcv user -c

echo
echo "==> 验证登录状态"
lcv user

echo
echo "==> 生成结果"
echo "LeetCode 目录:"
echo "$LEETCODE_DIR"
echo
echo "Remote User Settings:"
echo "$REMOTE_USER_SETTINGS_FILE"
echo
echo "关键文件:"
find "$WORKSPACE_DIR" -maxdepth 3 -type f \( -name ".clangd" -o -name "$(basename "$0")" \) -print
echo "$REMOTE_USER_SETTINGS_FILE"

echo
echo "========================================"
echo "初始化完成"
echo
echo "下一步："
echo "cd \"$WORKSPACE_DIR\""
echo "code ."
echo
echo "如果 VS Code 已打开，执行："
echo "Developer: Reload Window"
echo "========================================"