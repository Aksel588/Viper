#!/usr/bin/env bash
# Diagnostic: show how 'viper' resolves in the current shell environment.
set -euo pipefail

LOCAL_BIN="${HOME}/.local/bin/viper"
LOCAL_RUN="${HOME}/.local/bin/viperrun"
ZSHRC="${HOME}/.zshrc"
MARKER="# >>> viper compiler >>>"

echo "=== viper path diagnostic ==="
echo ""

RESOLVED="$(command -v viper 2>/dev/null || echo none)"
echo "command -v viper:  ${RESOLVED}"

if [[ -n "${ZSH_VERSION:-}" ]]; then
  WHENCE="$(whence -w viper 2>/dev/null || echo none)"
  echo "whence -w viper:   ${WHENCE}"
fi

echo ""
echo "local compiler:    ${LOCAL_BIN} ($([ -x "$LOCAL_BIN" ] && echo OK || echo MISSING))"
echo "local viperrun:    ${LOCAL_RUN} ($([ -x "$LOCAL_RUN" ] && echo OK || echo MISSING))"

if grep -q "$MARKER" "$ZSHRC" 2>/dev/null; then
  echo "zshrc viper block: present in ${ZSHRC}"
else
  echo "zshrc viper block: missing from ${ZSHRC}"
fi

echo ""
if [[ "$RESOLVED" == "none" ]]; then
  echo "status: viper not found on PATH"
elif [[ -x "$RESOLVED" ]]; then
  FILE_TYPE="$(file -b "$RESOLVED" 2>/dev/null || echo unknown)"
  echo "resolved type:     ${FILE_TYPE}"
  if head -1 "$RESOLVED" 2>/dev/null | grep -q python; then
    echo ""
    echo "WARNING: resolved 'viper' is a Python script, not the Viper compiler."
    echo "Fix: run ./install.sh, then source ~/.zshrc (or exec zsh)"
    echo "Or:  ${LOCAL_BIN} --version"
  elif [[ "$RESOLVED" == "$LOCAL_BIN" ]] || [[ "${WHENCE:-}" == *function* ]]; then
    echo ""
    echo "status: Viper compiler is correctly configured."
  else
    echo ""
    echo "NOTE: another 'viper' is on PATH before the compiler."
    echo "Fix: run ./install.sh, then source ~/.zshrc (or exec zsh)"
  fi
else
  echo "status: unexpected resolution — ${RESOLVED}"
fi
