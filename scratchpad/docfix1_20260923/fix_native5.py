"""DOCFIX1 NATIVE 12: §5 hard-refusal list against lodoRead (src/lodofile.cpp from 1727),
lodiRead (src/lodifile.cpp from 949) and the pairing checks (src/nativeemit.cpp:2976-2989).
The soft row is the hashes against the USER'S LIVE DATA (nativeemit.cpp:3023-3037); the same
hashes compared BETWEEN the two files are a hard pairing refusal (§4 row 0x90)."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/docs/LODGEN_NATIVE_LODO_LODI.md'
OLD_HEAD = b"| class | keys | generator | consumer |\n|---|---|---|---|\n| **hard** | magic, **version (1 and 2 are refused by name)**,"
OLD_SOFT = b"| **soft** | `pluginCorpusHash`, `objectCorpusHash`, `modelCorpusHash`, `cardCorpusHash`, **`loadOrderHash`** mismatch | refuse, name the field | **load anyway, log it, raise a `stale=1` census row, keep rendering** |\n"
NEW_ROWS = (
b"| **hard: pairing** (between the two files) | the two files name different worldspaces; `pluginCorpusHash` or `objectCorpusHash` differs **between the `.lodo` and the `.lodi`**; `loadOrderHash` differs **between the two files** (\xc2\xa74 row 0x90); `lodoIdentity` does not name this `.lodo` (unless `NOLIB`) \xe2\x80\x94 `src/nativeemit.cpp`, every `pairing:` refusal | refuse, name the field | **refuse to load, and never hide the engine's own LOD tree** |\n"
b"| **hard: `.lodo`** (`lodoRead`) | versions **1, 2 and 3 refused by name**, anything but 4; the `LADDER` flag disagreeing with `ladderGroup` / `levelMax`, `levelMax` > 15; `cardCount` > `baseCount`; a base's `fullTriangles` that its own meshes do not recount to, or non-zero on a base with no mesh; mesh flags beyond ALPHA / SWAY / WATERTIGHT; reserved header bytes 0xCE\xe2\x80\xa60xCF and 0xD4\xe2\x80\xa60xFF | refuse, name the field | as above |\n"
b"| **hard: `.lodi`** (`lodiRead`) | versions **1 and 2 refused by name**, anything outside 3\xe2\x80\xa69; a version whose defining table is missing (v5 without the placement-AO blob, v6 without the vertex-AO blob, v7/v9 with neither group table nor sky stream, v8 without the horizon stream); a file carrying a LATER version's header words (v3/v4 with placement-AO words, v3\xe2\x80\x93v6 with v7 words at 0x100/0x110, v7/v9 with v8 words at 0x11C); reserved header bytes by version (from 0xB0 on v3, 0xD4 on v4, 0xF1\xe2\x80\xa60xFF on v5, 0xF1\xe2\x80\xa60xF3 on v6 and later, plus 0x11C\xe2\x80\xa60x1FF on v7/v9, 0x130\xe2\x80\xa60x1FF on v8); instance flag bit 6 below v9 (\xc2\xa74.1); a stored cell outside the quantisation band (\xc2\xa74.1, `lodiCellAgrees`); the vertex-AO, sky and horizon offset tables and their slice lengths; the aggregate rows and their covered list (\xc2\xa74.6) | refuse, name the field | as above |\n"
b"| **soft** (against the user's LIVE data only) | `pluginCorpusHash`, `objectCorpusHash`, `modelCorpusHash`, `cardCorpusHash`, **`loadOrderHash`** recomputed from the running load order and disagreeing with the file \xe2\x80\x94 a mod installed, removed or reordered since the bake | refuse, name the field and the plugin | **load anyway, log it, raise a `stale=1` census row, keep rendering** |\n"
)
NOTE = (b"\n**The rows above are the classes, not every check.** `lodoRead` and `lodiRead` are\n"
b"the complete list, and each refusal names its field in words; a consumer that\n"
b"implements this table and not the readers will accept files they refuse.\n")
b = open(P, 'rb').read(); cr0 = b.count(b'\r\n')
if b.count(OLD_HEAD) != 1 or b.count(OLD_SOFT) != 1:
    print('REFUSE anchors', b.count(OLD_HEAD), b.count(OLD_SOFT)); sys.exit(1)
i = b.index(OLD_HEAD)
# relabel the existing hard row as the general class, keep its content
b = b.replace(OLD_HEAD, b"| class | keys | generator | consumer |\n|---|---|---|---|\n| **hard: both files** | magic, **version (see the per-file rows)**,", 1)
b = b.replace(OLD_SOFT, NEW_ROWS, 1)
j = b.index(NEW_ROWS) + len(NEW_ROWS)
b = b[:j] + NOTE + b[j:]
if b.count(b'\r\n') != cr0: sys.exit(1)
if '--check' in sys.argv: print('check OK'); sys.exit(0)
open(P, 'wb').write(b); print('written', len(b), 'CRLF', b.count(b'\r\n'))
