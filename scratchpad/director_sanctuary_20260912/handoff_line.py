p = 'E:/Projects/NifskopeWildWastelandEdition/HANDOFF.md'
b = open(p, 'rb').read()
assert b.count(b'\r') == 0
anchor = b'  LANDED 13:4x: TERRAINFMT1 DONE 13:45:27'
assert b.count(anchor) == 1, b.count(anchor)
line = (b"  BAKED 13:5x (director, end-to-end check on release/NifSkope.exe 12:58:48 sha1 ba7585cb, TERRAINFMT1's): the 9-chunk Sanctuary region\n"
b"  (cells -20 24 -9 35, dim 4) headless, own out-dir scratchpad/director_sanctuary_20260912/out, --road-detail 1, every module on, game down.\n"
b"  THREE COMMANDS, not one: `--lodl <dir>` RETURNS after writing the whole-worldspace .lodl (4.4 s, 35,953,294 B, verify-only 0 of 36864\n"
b"  mismatched) and never reaches the region; `--heightmap <dir>` likewise (1.3 s, 75,497,620 B, BYTE-IDENTICAL to FO4CS's installed reference);\n"
b"  the region bake is its own command (--vt --tex-dir --cover --native --native-mesh-report --road-detail 1): rc 0, 27 s, stage times\n"
b"  'landscape 0.0 s, meshes 19.8 s, textures 5.7 s, impostors 0.0 s', peak 1.78 GB, 17 chunks written 0 empty 0 failed, 26 obj files,\n"
b"  45 VT tiles over two levels (vt_check header/tiles: every tile CRC recomputes, payloads aligned; the one FAIL is V1 'height sheet role 4',\n"
b"  which only a --vt-height bake carries -- the gate's runs use it, this one did not), .lodo 9,657,316 B + .lodi 128,256 B (--native-verify\n"
b"  'pair identity ok', 3526 instances, ladder 1900 of 2982, occluders 0 -- Sanctuary is the vacuous region, see NATIVE1b), 91 road placements\n"
b"  roadDetail 1.000. PICTURE sent 13:55 (images/cmp_sanctuary_identity.png): chunk (-20,24) lit against our own sheets, default (vertex-colour\n"
b"  identity ON, mean RGB 28/24/84, blue-purple) beside a one-chunk --no-terrain-identity rebake (mean 133/118/102; colour sheet byte-identical,\n"
b"  BTR differs at byte 265). No decision taken on the default -- bungo's call, listed below. Nothing running; no Monitors; queue empty.\n")
b = b.replace(anchor, line + anchor, 1)
open(p, 'wb').write(b)
print('ok', len(b), b.count(b'\r'))
