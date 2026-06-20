#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

SYSTEM=0
if [[ "${1:-}" == "--system" ]]; then
    SYSTEM=1
fi

if command -v cc >/dev/null 2>&1; then
    :
elif command -v gcc >/dev/null 2>&1; then
    :
elif command -v clang >/dev/null 2>&1; then
    :
else
    echo "error: no C compiler found (need cc, gcc, or clang)" >&2
    exit 1
fi

echo "Building Viper..."
make all

if [[ "$SYSTEM" -eq 1 ]]; then
    PREFIX="/usr/local"
    echo "Installing to /usr/local (requires sudo)..."
    sudo make install PREFIX="$PREFIX"
else
    PREFIX="${HOME}/.local"
    echo "Installing to ${PREFIX} (no sudo)..."
    make install PREFIX="$PREFIX"
fi

BINDIR="${PREFIX}/bin"
LIBDIR="${PREFIX}/lib/viper"
VIPER_BIN="${BINDIR}/viper"
VIPERRUN_BIN="${BINDIR}/viperrun"

echo ""
echo "Viper installed successfully."
echo "  viper:     ${VIPER_BIN}"
echo "  viperrun:  ${VIPERRUN_BIN}"
echo "  stdlib:    ${LIBDIR}"
echo ""

if [[ ! -x "$VIPER_BIN" ]]; then
    echo "error: install failed — ${VIPER_BIN} not found" >&2
    exit 1
fi

"$VIPER_BIN" --version

ZSHRC="${HOME}/.zshrc"
MARKER="# >>> viper compiler >>>"
END_MARKER="# <<< viper compiler <<<"

setup_shell() {
    if [[ ! -f "$ZSHRC" ]]; then
        touch "$ZSHRC"
    fi
    if grep -q "$MARKER" "$ZSHRC" 2>/dev/null; then
        sed -i.bak "/${MARKER}/,/${END_MARKER}/d" "$ZSHRC"
    fi
    cat >> "$ZSHRC" <<EOF

${MARKER}
# Viper language compiler (overrides Python 'viper' package on PATH)
viper() {
  '${VIPER_BIN}' "\$@"
}
viperrun() {
  '${VIPERRUN_BIN}' "\$@"
}
export PATH="${BINDIR}:\$PATH"
export VIPER_PATH="${LIBDIR}"
${END_MARKER}
EOF
    echo ""
    echo "Updated ${ZSHRC} with viper shell functions."
    echo "IMPORTANT: reload your shell now:"
    echo "  exec zsh"
    echo "  # or: source ~/.zshrc"
    echo "Then: viper --version"
}

CURRENT="$(type -a viper 2>/dev/null | head -1 || command -v viper 2>/dev/null || true)"
if [[ -n "$CURRENT" && "$CURRENT" != "$VIPER_BIN" && "$CURRENT" != "${VIPER_BIN}"* ]]; then
    echo ""
    echo "note: another 'viper' exists on your system (often a Python package)."
    echo "  ${CURRENT}"
    setup_shell
else
    if ! grep -q "$MARKER" "$ZSHRC" 2>/dev/null; then
        setup_shell
    else
        echo ""
        echo "Try: viper --version"
    fi
fi
