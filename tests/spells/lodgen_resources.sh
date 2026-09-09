#!/bin/bash
#
# THE RESOURCE STACK: what the LOD generator reads, and in whose order.
#
# bungo, 2026-09-06: "Instead of top wins, we use MO2's standard, the last one
# in the order overrides the previous ones." So every check here is about
# PRECEDENCE, and none of them is a count that cannot fail:
#
#   a. --resource modA --resource modB: the generator's read of
#      textures/lod/x_d.dds returns modB's bytes; reversed it returns modA's.
#      Proved by the sha1 the CLI prints, not by "it found something".
#   b. an archive entry: a texture that exists only inside one of the game's
#      Fallout4 - Textures*.ba2 resolves through --resource <that archive> and
#      NOT without it.
#   c. a loose file beats the same path inside an archive WHEREVER the archive
#      sits in the list - both orders are tried.
#   d. --plugins-txt yields the enabled plugins in load order and drops the
#      unstarred (disabled) one and the comment.
#
# USAGE
#   bash tests/spells/lodgen_resources.sh
#   EXE=... DATA=... bash tests/spells/lodgen_resources.sh

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
DATA="${DATA:-X:/Programs/Steam/steamapps/common/Fallout 4/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }

fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }

winpath() { ( cd "$1" 2>/dev/null && pwd -W ) || echo "$1"; }

# ---- fixtures --------------------------------------------------------------
# two mods, the same relative texture, different bytes; a plugins.txt; a Data
# folder for the plugin names to resolve against
"$PY" "$ROOT/tests/spells/lodgen_resources_fixture.py" "$W" || {
	echo "fixture writer failed"; exit 2; }

MODA="$(winpath "$W/modA")"
MODB="$(winpath "$W/modB")"
FDATA="$(winpath "$W/data")"
REL="textures/lod/x_d.dds"

probe() {	# prints "entry|kind|sha1|found"; args are passed through to the CLI
	"$NS" -no-gui lodgen --probe "$REL" "$@" 2>/dev/null | tr -d '\r' | "$PY" -c '
import sys
f = {}
for line in sys.stdin:
    if ":" in line:
        k, v = line.split(":", 1)
        f[k.strip()] = v.strip()
print("%s|%s|%s|%s" % (f.get("entry", ""), f.get("kind", ""), f.get("sha1", ""), f.get("found", "no")))'
}

SHA_A="$("$PY" -c "import hashlib,sys;print(hashlib.sha1(open(sys.argv[1],'rb').read()).hexdigest())" "$W/modA/textures/lod/x_d.dds")"
SHA_B="$("$PY" -c "import hashlib,sys;print(hashlib.sha1(open(sys.argv[1],'rb').read()).hexdigest())" "$W/modB/textures/lod/x_d.dds")"
echo "  modA sha1 $SHA_A"
echo "  modB sha1 $SHA_B"
[ "$SHA_A" != "$SHA_B" ] || { echo "the two fixtures are the same file"; exit 2; }

# ---- a. the last entry overrides the earlier ones ---------------------------
AB="$(probe --resource "$MODA" --resource "$MODB")"
BA="$(probe --resource "$MODB" --resource "$MODA")"
echo "  modA then modB -> $AB"
echo "  modB then modA -> $BA"
if [ "$(echo "$AB" | cut -d'|' -f3)" = "$SHA_B" ] && [ "$(echo "$BA" | cut -d'|' -f3)" = "$SHA_A" ]; then
	ok "a. the LAST resource in the order wins, both ways round"
else
	bad "a. the last resource does not win (got $AB and $BA)"
fi

# ---- b. an archive entry ----------------------------------------------------
ARCHIVE=""
ARCPATH=""
if [ -d "$DATA" ]; then
	for a in "$DATA"/Fallout4\ -\ Textures*.ba2; do
		[ -f "$a" ] || continue
		p="$("$NS" -no-gui lodgen --resource "$a" --list-files 400 2>/dev/null | tr -d '\r' \
			| grep '^file: ' | grep '\.dds$' | head -1 | sed 's/^file: //')"
		if [ -n "$p" ]; then ARCHIVE="$a"; ARCPATH="$p"; break; fi
	done
fi
if [ -z "$ARCHIVE" ]; then
	bad "b. no game archive to test with (looked in $DATA)"
else
	echo "  archive $(basename "$ARCHIVE"), path $ARCPATH"
	WITH="$("$NS" -no-gui lodgen --probe "$ARCPATH" --resource "$ARCHIVE" 2>/dev/null | tr -d '\r' | grep '^found:' | cut -d' ' -f2)"
	"$NS" -no-gui lodgen --probe "$ARCPATH" >/dev/null 2>&1
	WITHOUT_RC=$?
	KIND="$("$NS" -no-gui lodgen --probe "$ARCPATH" --resource "$ARCHIVE" 2>/dev/null | tr -d '\r' | grep '^kind:' | cut -d' ' -f2)"
	echo "  with the archive: found=$WITH kind=$KIND; without it: exit $WITHOUT_RC"
	if [ "$WITH" = "yes" ] && [ "$KIND" = "archive" ] && [ "$WITHOUT_RC" -ne 0 ]; then
		ok "b. an archive entry resolves a path only that archive has, and nothing does without it"
	else
		bad "b. the archive entry did not decide it (found=$WITH kind=$KIND without=$WITHOUT_RC)"
	fi

	# ---- c. a loose file beats the archive, wherever the archive sits --------
	mkdir -p "$W/loose/$(dirname "$ARCPATH")"
	printf 'loose wins' > "$W/loose/$ARCPATH"
	SHA_L="$("$PY" -c "import hashlib,sys;print(hashlib.sha1(open(sys.argv[1],'rb').read()).hexdigest())" "$W/loose/$ARCPATH")"
	LOOSE="$(winpath "$W/loose")"
	first="$("$NS" -no-gui lodgen --probe "$ARCPATH" --resource "$LOOSE" --resource "$ARCHIVE" 2>/dev/null | tr -d '\r' | grep -E '^(sha1|kind):' | tr '\n' ' ')"
	second="$("$NS" -no-gui lodgen --probe "$ARCPATH" --resource "$ARCHIVE" --resource "$LOOSE" 2>/dev/null | tr -d '\r' | grep -E '^(sha1|kind):' | tr '\n' ' ')"
	echo "  loose sha1 $SHA_L"
	echo "  archive last : $first"
	echo "  archive first: $second"
	if echo "$first" | grep -q "$SHA_L" && echo "$second" | grep -q "$SHA_L"; then
		ok "c. a loose file beats the same path inside an archive, in either order"
	else
		bad "c. the archive won over a loose file"
	fi
fi

# ---- d. plugins.txt ---------------------------------------------------------
OUT="$("$NS" -no-gui lodgen --plugins-txt "$(winpath "$W")/plugins.txt" --data-root "$FDATA" --print-source 2>/dev/null | tr -d '\r')"
echo "$OUT" | grep -E '^(plugins|plugin [0-9]+):' | sed 's/^/  /'
N="$(echo "$OUT" | grep '^plugins: ' | cut -d' ' -f2)"
P0="$(echo "$OUT" | grep '^plugin 0: ' | sed 's/^plugin 0: //')"
P1="$(echo "$OUT" | grep '^plugin 1: ' | sed 's/^plugin 1: //')"
if [ "$N" = "2" ] && echo "$P0" | grep -qi 'Fallout4.esm' && echo "$P1" | grep -qi 'Test.esp' \
	&& ! echo "$OUT" | grep -qi 'Disabled.esp'; then
	ok "d. plugins.txt yields the enabled plugins in load order and drops the unstarred one"
else
	bad "d. the plugin list is wrong (n=$N, 0=$P0, 1=$P1)"
fi

echo "4 checks, $fails failures"
[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
