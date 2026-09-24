p = 'HANDOFF.md'
s = open(p, 'rb').read().decode('utf-8')
assert s.count(chr(13)) == 0
a = '  His NifSkope window: OPEN since 01:29 (pid 60820, no --port) on the'
assert s.count(a) == 1, s.count(a)
new = '''  RULING 02:05 (bungo): "That's it for now, you can run a second agent
  on it then merge" -> LANE UINOTES1 LAUNCHED 01:56 (Opus, Agent tool,
  background) IN A SEPARATE COPY OF THE TREE: E:/Projects/NifskopeWWE_ui
  (robocopy 01:53 of everything but scratchpad/, plus scratchpad *.md
  and *.py; exe 23:26:29 21,484,032 B inside). Brief
  scratchpad/brief_uinotes1.md (both trees); rulings file
  scratchpad/brief_uinotes_20260912.md (items 1-9, 6a/6b/7a/7b). It
  runs beside TILING4 by his word (the one-instance rule: the UI lane
  waits for any --port NifSkope from the main tree; his window is never
  touched). MERGE PROTOCOL (director, after both DONE): read the copy's
  scratchpad/uinotes1_20260912/CHANGED_FILES.txt (A/M list + CR/LF
  counts), confirm none of TILING4's files are on it (lodgen.*,
  nifcli.cpp, lodgenmanager.cpp, btdterrain.*, lodtfile.*,
  docs/LODGEN_*, tests/spells/lodgen_*, bake_impostor_cards.sh), copy
  each listed file from the copy into the main tree (byte copy; line
  endings travel with the file), copy its report/images/skills over,
  then ONE build in the main tree + the UI chain + the lodgen chain,
  splice both lanes' docs (UINOTES1's WW_CHANGES entry after TILING4's),
  send the pictures. If the same file is on both lanes' lists it is a
  hand merge, said so in the report. A liveness monitor (35 min silence
  or DONE, either lane) is armed in this session (task blpb29y44).
  ANSWERED 02:0x (bungo: "right now I can save the edited hkx, right?"):
  yes -- Save writes the clip back to its .hkx as interleaved (the class
  the game loads by name), Save as to a new file; hkxwrite_gates 23/23
  round-trip, HKXPACK re-reads it; an edited clip has NOT been flown in
  game yet (his).
'''
s = s.replace(a, new + a)
open(p, 'wb').write(s.encode('utf-8'))
print('ok', s.count('LANE UINOTES1 LAUNCHED'))
