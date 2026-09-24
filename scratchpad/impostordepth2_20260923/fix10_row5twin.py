# fix10_row5twin.py -- impostor_draw.sh row 5 on the director's ruling (2026-09-23):
# the 4x4 row stays at its 0.50 floor as a NAMED known red; a twin row 5t measures the
# shipped grid (8x8 at 2k, bungo 09:3x) at the crisp end (flat snap), same 0.50 floor.
P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/impostor_draw.sh'
b = open(P, 'rb').read(); cr = b.count(b'\r'); s = b.decode('utf-8')
E = [
 ('\t\tbad "5 silhouette IoU mean $got < floor $IOU_FLOOR (16 views)"\n'
  '\tfi\n'
  'else\n'
  '\tbad "5 WW_IMPOSTOR_PREVIEW=iou did not finish"\n'
  'fi\n',
  '\t\tbad "5 KNOWN RED (director 2026-09-23: 4x4 flat snap is below the outline floor by construction; not the shipped grid) silhouette IoU mean $got < floor $IOU_FLOOR (16 views)"\n'
  '\tfi\n'
  'else\n'
  '\tbad "5 WW_IMPOSTOR_PREVIEW=iou did not finish"\n'
  'fi\n'
  '\n'
  '# ---------------------------------------------------------------------------\n'
  '# 5t. ROW 5\'s TWIN ON THE SHIPPED GRID (lane IMPOSTORDEPTH2, director ruling\n'
  '#     2026-09-23). The crisp end is the FLAT snap (bungo 13:1x): one frame, not\n'
  '#     moved by its depth, so on a 4x4 card the nearest frame can sit far off the\n'
  '#     view and row 5 above reads 0.3920 -- a named known red. The row must\n'
  '#     measure what ships, and the ruled default tree grid is 8x8 at 2k (bungo\n'
  '#     09:3x). Same harness, same 16 views, same 0.50 floor, at the default.\n'
  '#     Measured on exe 6b8ed793: 0.7029 (TreeMapleInstitute06Green, BC7 sheets).\n'
  '#     IMPOSTOR_LODM_8 / IMPOSTOR_NIF_8 point it elsewhere; missing = FAIL.\n'
  '# ---------------------------------------------------------------------------\n'
  'LODM8="${IMPOSTOR_LODM_8:-$root/scratchpad/impostordepth2_20260923/n8_2k_bc7/cards/000531b3_oct.lodm}"\n'
  'NIF8="${IMPOSTOR_NIF_8:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleInstitute06Green.nif}"\n'
  'if [ ! -f "$LODM8" ] || [ ! -f "$NIF8" ]; then\n'
  '\tbad "5t no 8x8 card or mesh ($LODM8 / $NIF8) -- REFUSED rather than skipped"\n'
  'else\n'
  '\t: > "$work/ww_impostor_iou8.log"\n'
  '\tWW_IMPOSTOR_PREVIEW=iou \\\n'
  '\tWW_IMPOSTOR_LODM="$( cygpath -m "$LODM8" )" \\\n'
  '\tWW_IMPOSTOR_LOG="$work/ww_impostor_iou8.log" \\\n'
  '\tWW_WINDOW_AT=1960,40 \\\n'
  '\tWW_RENDER_SIZE="${IMPOSTOR_SIZE:-1024x1024}" \\\n'
  '\t"$exe" --port "$port" "$NIF8" >> "$log" 2>&1\n'
  '\tgot8=$( sed -n \'s/^iou mean //p\' "$work/ww_impostor_iou8.log" | tail -1 )\n'
  '\tcrisp8=$( grep -c "the CRISP end, flat snap" "$work/ww_impostor_iou8.log" )\n'
  '\tif [ -z "$got8" ]; then\n'
  '\t\tbad "5t the harness wrote no \'iou mean\' line on the 8x8 card"\n'
  '\telif [ "${crisp8:-0}" -lt 1 ]; then\n'
  '\t\tbad "5t the 8x8 run did not draw the crisp end (flat snap) -- the log does not say so"\n'
  '\telif "$PY" -c "import sys; sys.exit(0 if float(\'$got8\') >= float(\'$IOU_FLOOR\') else 1)"; then\n'
  '\t\tok "5t 8x8 card, crisp end (flat snap): silhouette IoU mean $got8 >= floor $IOU_FLOOR (16 views)"\n'
  '\telse\n'
  '\t\tbad "5t 8x8 card, crisp end (flat snap): silhouette IoU mean $got8 < floor $IOU_FLOOR (16 views)"\n'
  '\tfi\n'
  'fi\n'),
]
for o, n in E:
    assert s.count(o) == 1, o[:60]
    s = s.replace(o, n)
out = s.encode('utf-8'); assert out.count(b'\r') == cr
open(P, 'wb').write(out); print('patched')
