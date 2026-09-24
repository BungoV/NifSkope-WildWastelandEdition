# fix08_row15.py -- impostor_draw.sh row 15: the default is now the FLAT snap, so the
# "parallax ON" run must name the parallax path explicitly (WW_IMPOSTOR_SLIDER=1).
P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/impostor_draw.sh'
b = open(P, 'rb').read(); cr = b.count(b'\r'); s = b.decode('utf-8')
BS = chr(92)
E = [
 ('photo_iou() {   # $1 = WW_IMPOSTOR_BLEND, $2 = log name; sets $pn (counted) and $pi (mean)\n'
  '\t: > "$work/$2"\n'
  '\tWW_IMPOSTOR_PREVIEW=orbit ' + BS + '\n',
  'photo_iou() {   # $1 = WW_IMPOSTOR_BLEND, $2 = log name, $3 = WW_IMPOSTOR_SLIDER; sets $pn (counted) and $pi (mean)\n'
  '\t: > "$work/$2"\n'
  '\tenv WW_IMPOSTOR_SLIDER="$3" ' + BS + '\n'
  '\tWW_IMPOSTOR_PREVIEW=orbit ' + BS + '\n'),
 ('\tphoto_iou 0 "ww_impostor_photo_off.log"; nOff="$pn"; iOff="$pi"\n'
  '\tphoto_iou 1 "ww_impostor_photo_on.log";  nOn="$pn";  iOn="$pi"\n',
  '\t# RE-PINNED (lane IMPOSTORDEPTH2, bungo 2026-09-23 13:1x): the default draw is\n'
  '\t# now the FLAT snap, so a BLEND=1 run at the default slider would be flat too and\n'
  '\t# this row a trivial pass. The ON run names the parallax path: slider 1.\n'
  '\tphoto_iou 0 "ww_impostor_photo_off.log" 0; nOff="$pn"; iOff="$pi"\n'
  '\tphoto_iou 1 "ww_impostor_photo_on.log" 1;  nOn="$pn";  iOn="$pi"\n'),
]
for o, n in E:
    assert s.count(o) == 1, o[:60]
    s = s.replace(o, n)
out = s.encode('utf-8'); assert out.count(b'\r') == cr
open(P, 'wb').write(out); print('patched')
