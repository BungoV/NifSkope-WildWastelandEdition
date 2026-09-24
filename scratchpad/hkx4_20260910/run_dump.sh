#!/bin/bash
# Lane HKX4b: run release/gltfexport_dump.exe from the MSYS2 shell (Qt6Core.dll
# is on its PATH, not Git-Bash's). All arguments are passed straight through.
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
exec release/gltfexport_dump.exe "$@"
