#!/bin/sh
# bake one mesh: bake1.sh EXE MESH OUTDIR N TILE PORT [ENV...]
set -u
EXE="$1"; MESH="$2"; O="$3"; N="$4"; TILE="$5"; P="$6"; shift 6
tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'NifSkope%'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" | tr -d '\r')
[ "$n" = "0" ] || { echo "REFUSED: harness running"; exit 1; }
rm -rf "$O"; mkdir -p "$O"
t0=$(date +%s)
env "$@" WW_IMPOSTOR_BAKE="$O" WW_IMPOSTOR_OCT="$N" WW_IMPOSTOR_TILE="$TILE" WW_WINDOW_AT=1960,40 \
	timeout 1800 "$EXE" "$MESH" --port "$P" > "$O.stdout" 2>&1
echo "bake rc=$? $(( $(date +%s) - t0 ))s $(ls "$O" | wc -l) files; $(grep -E '^aa ' "$O"/*.txt)"
