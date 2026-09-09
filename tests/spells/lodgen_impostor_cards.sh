#!/bin/bash
#
# Impostor cards: the side view is used.
#
# The bake hook has always photographed a model into _front.png AND _side.png,
# but the card builder converted only the front and put it on both crossed
# quads, so an impostor looked identical from every angle. Now one sheet holds
# front | side and the two quads read opposite halves.
#
# Synthetic cards, so the gate does not depend on a GUI bake: a red front and a
# blue side for the first impostor candidate of a known region. Checks:
#   1. the candidate list is non-empty (else nothing here can be tested)
#   2. the object chunk references the front|side sheet, <id>_fs.DDS
#   3. the sheet is twice the card's width
#   4. its left and right halves differ (front red, side blue survive BC3)
#   5. it is BC3/DXT5 carrying a real alpha, with vanilla's own DDS header --
#      it was BC1 with no DDPF_ALPHAPIXELS until 2026-09-09, which is why every
#      card quad in a chunk drew as an opaque square
#
# USAGE
#   bash tests/spells/lodgen_impostor_cards.sh

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }

fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }

# 1. a candidate: a base in the region whose far LOD slots are empty
# the candidate listing takes lodgen's REGION (a cell box), not a chunk
CAND="$("$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -17 27 --list-impostor-candidates 2>/dev/null \
	| grep -E '^[0-9a-fA-F]{8} ' | head -1 | cut -d' ' -f1)"
if [ -n "$CAND" ]; then ok "an impostor candidate exists in cells (-20,24)..(-17,27): $CAND"; else bad "no impostor candidate listed for cells (-20,24)..(-17,27)"; echo "RESULT FAIL"; exit 1; fi
ID="$(echo "$CAND" | tr 'A-F' 'a-f')"

# 2. synthetic cards: red front, blue side, 32x64
mkdir -p "$W/cards"
"$PY" - "$W/cards" "$ID" <<'PYEOF'
import sys, struct, zlib
d, id_ = sys.argv[1], sys.argv[2]
def png(path, w, h, rgba):
    raw = b''.join(b'\x00' + bytes(rgba) * w for _ in range(h))
    def chunk(t, data): return struct.pack('>I', len(data)) + t + data + struct.pack('>I', zlib.crc32(t + data) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
png(d + '/' + id_ + '_front.png', 32, 64, (255, 0, 0, 255))
png(d + '/' + id_ + '_side.png', 32, 64, (0, 0, 255, 255))
open(d + '/' + id_ + '.txt', 'w').write('card 64 128 0 0 128\n')
PYEOF
[ -s "$W/cards/${ID}_front.png" ] || { bad "could not write the synthetic cards"; echo "RESULT FAIL"; exit 1; }

# 3. bake the object chunk with the cards. A card only stands in where the
#    requested LOD slot is EMPTY, and a candidate is a base whose FAR slots are
#    empty - so the chunk has to be a far one: dim 16, whose origin for these
#    cells is (-32,16). At dim 4 the near model exists and no card is used.
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --impostors "$W/cards" \
	--data-root "$DATA" -o "$W/chunk.bto" >/dev/null 2>&1
[ -s "$W/chunk.bto" ] || { bad "the object chunk was not written"; echo "RESULT FAIL"; exit 1; }
ok "object chunk written with --impostors"

if grep -aq "${ID}_fs.DDS" "$W/chunk.bto"; then ok "the chunk references the front|side sheet ${ID}_fs.DDS"; else bad "the chunk does not reference ${ID}_fs.DDS"; fi

# 4. cards from ring 0: at dim 4 the same base HAS a mesh for its ring and keeps it;
#    with --impostors-from-level 0 it stands on the card instead, and the chunk says so
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -20 24 --dim 4 --no-ao --impostors "$W/cards" \
	--data-root "$DATA" -o "$W/near.bto" > "$W/near.log" 2>&1
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -20 24 --dim 4 --no-ao --impostors "$W/cards" \
	--impostors-from-level 0 --data-root "$DATA" -o "$W/near_cards.bto" > "$W/near_cards.log" 2>&1
if [ -s "$W/near.bto" ] && [ -s "$W/near_cards.bto" ]; then
	if grep -aq "${ID}_fs.DDS" "$W/near.bto"; then bad "at dim 4 the base stood on the card without being asked"; else ok "at dim 4 the base keeps its own mesh by default"; fi
	if grep -aq "${ID}_fs.DDS" "$W/near_cards.bto"; then ok "with --impostors-from-level 0 the base stands on the card at dim 4"; else bad "no card in the dim-4 chunk with --impostors-from-level 0"; fi
	NC="$(grep -ao "[0-9]* placements on cards in place of their ring's mesh" "$W/near_cards.log" | head -1)"
	echo "  ${NC:-no report of cards in place of meshes}"
	[ -n "$NC" ] && ok "the chunk report counts the placements on cards" || bad "the report does not count cards in place of meshes"
	S1=$(stat -c %s "$W/near.bto"); S2=$(stat -c %s "$W/near_cards.bto")
	echo "  dim-4 chunk: $S1 bytes with meshes, $S2 bytes with cards from ring 0"
	[ "$S2" -lt "$S1" ] && ok "the chunk shrinks when cards replace the meshes" || bad "the chunk did not shrink"
else
	bad "a dim-4 chunk was not written"; tail -2 "$W/near.log" "$W/near_cards.log"
fi

SHEET="$W/cards/${ID}_fs.DDS"
if [ -s "$SHEET" ]; then ok "the sheet was written beside the cards"; else bad "no sheet at $SHEET"; echo "RESULT FAIL"; exit 1; fi

"$PY" - "$SHEET" <<'PYEOF'
import sys, struct
b = open(sys.argv[1], 'rb').read()
h, w = struct.unpack_from('<II', b, 12)
fourcc = b[84:88]
pfflags = struct.unpack_from('<I', b, 80)[0]
flags = struct.unpack_from('<I', b, 8)[0]
caps = struct.unpack_from('<I', b, 108)[0]
print('  %s   the sheet is twice the card width (%dx%d)' % ('ok  ' if w == 64 and h == 64 else 'FAIL', w, h))
# THE ALPHA. This sheet went out as DXT1 with no DDPF_ALPHAPIXELS until
# 2026-09-09, and every card quad in a chunk drew as an OPAQUE SQUARE. Vanilla's
# own alpha-tested tree LOD textures (Textures/LOD/Trees/MapleBranchesLOD_d.dds,
# ElmBranchesLOD_d.dds) are DXT5 with dwFlags 0x000A1007, pfflags 0x4 and
# caps 0x401008; those are the four numbers checked, not a guess at a format.
fmtOk = (fourcc == b'DXT5' and pfflags == 0x4 and flags == 0x000A1007 and caps == 0x401008)
print('  %s   the sheet is DXT5 with vanilla\'s own header (fourCC %s, dwFlags 0x%08X, pfflags 0x%X, caps 0x%X)'
      % ('ok  ' if fmtOk else 'FAIL', fourcc.decode('latin1'), flags, pfflags, caps))
# BC3: 16 bytes a 4x4 block (8 alpha, 8 colour), 16 blocks a row at 64 wide;
# compare block 0 (left half) with block 8 (right half)
off = 4 + 124 + (20 if fourcc == b'DX10' else 0)
BB = 16 if fourcc == b'DXT5' else 8
left = b[off:off + BB]; right = b[off + BB * 8:off + BB * 9]
print('  %s   left and right halves differ (front vs side)' % ('ok  ' if left != right else 'FAIL'))
# and the alpha block CARRIES something: the synthetic cards are fully opaque,
# so every alpha endpoint must read 255. A BC1 sheet has no alpha block at all
# and this offset would be colour, which is red (not 0xFF,0xFF).
a0, a1 = left[0], left[1]
alphaOk = (a0 == 255 and a1 == 255)
print('  %s   the alpha block is present and reads opaque on an opaque card (endpoints %d, %d)'
      % ('ok  ' if alphaOk else 'FAIL', a0, a1))
sys.exit(0 if (w == 64 and h == 64 and left != right and fmtOk and alphaOk) else 1)
PYEOF
[ $? -eq 0 ] || fails=$((fails + 1))

[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
