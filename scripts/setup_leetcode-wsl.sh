#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# VS Code + WSL + LeetCode 初始化脚本
#
# 推荐位置：
#   ~/study/scripts/setup-leetcode-wsl.sh
#
# 目标目录结构：
#   ~/study
#   ├── .vscode/settings.json
#   ├── leetcode/.clangd
#   └── scripts/setup-leetcode-wsl.sh
# ============================================================

# 脚本所在目录，例如 /home/morgana/study/scripts
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

# study 根目录，即 scripts 的上一级
WORKSPACE_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"

LEETCODE_DIR="$WORKSPACE_DIR/leetcode"
VSCODE_SETTINGS_DIR="$WORKSPACE_DIR/.vscode"
VSCODE_SETTINGS_FILE="$VSCODE_SETTINGS_DIR/settings.json"

EXTENSION_ID="LeetCode.vscode-leetcode"

echo "==> 当前用户: $(whoami)"
echo "==> HOME: $HOME"
echo "==> 脚本目录: $SCRIPT_DIR"
echo "==> 工作区目录: $WORKSPACE_DIR"
echo "==> LeetCode 目录: $LEETCODE_DIR"

echo
echo "==> 检查 Node.js"
if ! command -v node >/dev/null 2>&1; then
    echo "未找到 node，请先安装 Node.js："
    echo "sudo apt update && sudo apt install -y nodejs npm"
    exit 1
fi
node -v

echo
echo "==> 检查 code 命令"
if ! command -v code >/dev/null 2>&1; then
    echo "未找到 code 命令。请先从 Windows VS Code 连接一次 WSL，或确认 VS Code Server 已安装。"
    exit 1
fi

echo
echo "==> 创建目录"
mkdir -p "$LEETCODE_DIR"
mkdir -p "$VSCODE_SETTINGS_DIR"
mkdir -p "$HOME/.local/bin"

echo
echo "==> 安装或确认 WSL 侧 LeetCode 扩展"
code --install-extension "$EXTENSION_ID" --force >/dev/null || true

echo
echo "==> 定位 WSL 侧 LeetCode 扩展目录"
EXT_DIR="$(find "$HOME/.vscode-server/extensions" -maxdepth 1 -type d -iname "leetcode.vscode-leetcode-*" 2>/dev/null | sort -V | tail -n 1 || true)"

if [[ -z "$EXT_DIR" ]]; then
    echo "未找到 WSL 侧 LeetCode 扩展目录。"
    echo "请在 VS Code WSL 窗口中确认插件安装在 WSL，而不是只安装在 Windows Local。"
    exit 1
fi

LCV_BIN="$EXT_DIR/node_modules/vsc-leetcode-cli/bin/leetcode"

if [[ ! -f "$LCV_BIN" ]]; then
    echo "找到扩展目录，但未找到内置 CLI："
    echo "$LCV_BIN"
    exit 1
fi

echo "扩展目录: $EXT_DIR"
echo "CLI 路径: $LCV_BIN"

echo
echo "==> 创建 lcv 包装命令"

cat > "$HOME/.local/bin/lcv" <<EOF
#!/usr/bin/env bash
node "$LCV_BIN" "\$@"
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
echo "========================================"
echo "准备登录 leetcode.cn"
echo
echo "1. 在 Windows 浏览器中打开："
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
echo "==> 写入 study 工作区 VS Code 配置"
cat > "$VSCODE_SETTINGS_FILE" <<EOF
{
  "C_Cpp.intelliSenseEngine": "disabled",

  "clangd.path": "/usr/bin/clangd",
  "clangd.arguments": [
    "--background-index"
  ],

  "files.associations": {
    "*.h": "cpp",
    "*.hpp": "cpp"
  },

  "[cpp]": {
    "editor.formatOnSave": true,
    "editor.defaultFormatter": "xaver.clang-format"
  },
  "[c]": {
    "editor.formatOnSave": true,
    "editor.defaultFormatter": "xaver.clang-format"
  },
  "[h]": {
    "editor.formatOnSave": true,
    "editor.defaultFormatter": "xaver.clang-format"
  },
  "[hpp]": {
    "editor.formatOnSave": true,
    "editor.defaultFormatter": "xaver.clang-format"
  },

  "[markdown]": {
    "editor.wordWrap": "on",
    "editor.formatOnSave": false
  },
  "[yaml]": {
    "editor.formatOnSave": false
  },
  "[json]": {
    "editor.formatOnSave": false
  },
  "[jsonc]": {
    "editor.formatOnSave": false
  },

  "editor.tabSize": 4,
  "editor.insertSpaces": true,
  "editor.rulers": [100],

  "terminal.integrated.defaultProfile.linux": "bash",
  "remote.autoForwardPortsFallback": 0,

  "leetcode.endpoint": "leetcode-cn",
  "leetcode.workspaceFolder": "$LEETCODE_DIR",
  "leetcode.defaultLanguage": "cpp",
  "leetcode.filePath": {
    "default": {
      "folder": "",
      "filename": "\${id}.\${kebab-case-name}.\${ext}"
    }
  },
  "leetcode.hideSolved": true,
  "leetcode.hint.commentDescription": false,
  "leetcode.hint.commandShortcut": false,
  "leetcode.hint.configWebviewMarkdown": false
}
EOF

echo "已写入: $VSCODE_SETTINGS_FILE"

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
echo "==> 最终验证"
echo
echo "lcv user:"
lcv user || true

echo
echo "生成的关键文件:"
find "$WORKSPACE_DIR" -maxdepth 3 -type f \( -path "*/.vscode/settings.json" -o -name ".clangd" -o -name "$(basename "$0")" \) -print

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