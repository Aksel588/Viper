#!/usr/bin/env bash
# Diagnostic: log how 'viper' resolves in the current shell environment.
LOG="/Users/aksel/Desktop/Viper/.cursor/debug-1338c0.log"
SESSION="1338c0"
TS=$(($(date +%s) * 1000))

log_json() {
  local hid="$1" loc="$2" msg="$3" data="$4"
  printf '{"sessionId":"%s","timestamp":%s,"hypothesisId":"%s","location":"%s","message":"%s","data":%s}\n' \
    "$SESSION" "$TS" "$hid" "$loc" "$msg" "$data" >> "$LOG"
}

LOCAL_BIN="${HOME}/.local/bin/viper"
PY_VIPER="/Library/Frameworks/Python.framework/Versions/3.10/bin/viper"
ZSHRC="${HOME}/.zshrc"

# #region agent log
if [[ -x "$LOCAL_BIN" ]]; then
  log_json "H3" "viper-path-debug.sh:install" "local binary exists" "{\"path\":\"$LOCAL_BIN\",\"executable\":true}"
else
  log_json "H3" "viper-path-debug.sh:install" "local binary missing" "{\"path\":\"$LOCAL_BIN\",\"executable\":false}"
fi
# #endregion

# #region agent log
if grep -q "# >>> viper compiler >>>" "$ZSHRC" 2>/dev/null; then
  log_json "H2" "viper-path-debug.sh:zshrc" "viper block present in zshrc" "{\"zshrc\":\"$ZSHRC\"}"
else
  log_json "H2" "viper-path-debug.sh:zshrc" "viper block missing from zshrc" "{\"zshrc\":\"$ZSHRC\"}"
fi
# #endregion

RESOLVED="$(command -v viper 2>/dev/null || echo none)"
# #region agent log
log_json "H1" "viper-path-debug.sh:resolve" "command -v viper in current shell" "{\"resolved\":\"$RESOLVED\",\"shell\":\"$SHELL\",\"zsh_version\":\"${ZSH_VERSION:-}\"}"
# #endregion

# #region agent log
if [[ "$RESOLVED" == "$PY_VIPER" ]]; then
  log_json "H1" "viper-path-debug.sh:python" "Python viper wins (stale PATH or zshrc not sourced)" "{\"python_path\":\"$PY_VIPER\"}"
elif [[ "$RESOLVED" == "$LOCAL_BIN" ]]; then
  log_json "H5" "viper-path-debug.sh:ok" "Compiler binary wins on PATH" "{\"compiler_path\":\"$LOCAL_BIN\"}"
fi
# #endregion

# #region agent log
ZSH_FUNC="$(zsh -ic 'whence -w viper 2>/dev/null' 2>/dev/null | tail -1 || echo none)"
log_json "H4" "viper-path-debug.sh:function" "viper in fresh interactive zsh" "{\"whence\":\"$ZSH_FUNC\"}"
# #endregion

echo "=== viper path diagnostic ==="
echo "command -v viper: $RESOLVED"
echo "local compiler:  $LOCAL_BIN ($([ -x "$LOCAL_BIN" ] && echo OK || echo MISSING))"
echo "fresh zsh whence: $ZSH_FUNC"
echo "log written to:  $LOG"
