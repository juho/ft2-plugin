#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOL_SRC="$SCRIPT_DIR/../../src/helpdata/code/ft2hlp_to_h.c"
TOOL="$SCRIPT_DIR/ft2hlp_to_h"

# Compile tool if needed
if [ ! -f "$TOOL" ] || [ "$TOOL_SRC" -nt "$TOOL" ]; then
    cc -o "$TOOL" "$TOOL_SRC"
fi

cd "$SCRIPT_DIR"

# Convert to DOS line endings (tool expects CRLF)
perl -i -pe 's/\r?\n/\r\n/' FT2_PLUGIN.HLP

./ft2hlp_to_h FT2_PLUGIN.HLP
mv ft2_help_data.h ft2_plugin_help_data.h
echo "Generated ft2_plugin_help_data.h"

