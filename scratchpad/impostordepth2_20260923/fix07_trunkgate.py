# fix07_trunkgate.py -- impostor_trunk.sh for bungo's ruling 2026-09-23 13:1x: crisp end = FLAT snap.
P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/impostor_trunk.sh'
b = open(P, 'rb').read(); cr = b.count(b'\r'); s = b.decode('utf-8')
E = [
    # header: the ruling
    ('#   * the slider\'s CRISP end (0, the default) is SNAP: the nearest frame\n'
     '#     alone, no blend, placed at its own depth. It may JUMP at a frame\n'
     '#     change (ruled acceptable), so its T3 and its pop are REPORTED, not\n'
     '#     gated; its trunk width (T1), its trunk count (T2) and its tear share\n'
     '#     are gated;\n',
     '#   * the slider\'s CRISP end (0, the default) is FLAT SNAP (ruled 2026-09-23\n'
     '#     13:1x): the nearest frame alone, no blend, NOT moved by its depth --\n'
     '#     byte-identical to WW_IMPOSTOR_BLEND=0. It may JUMP at a frame change\n'
     '#     (ruled acceptable), so its T3 and its pop are REPORTED, not gated; its\n'
     '#     trunk width (T1) and trunk count (T2) are gated; its tear share is\n'
     '#     gated and is a NAMED KNOWN RED (bungo saw it fail and ruled the flat\n'
     '#     snap anyway, 2026-09-23 13:1x) -- it counts as a failure, like\n'
     '#     native_lighting.sh\'s two legacy_btr reds, and the bar is not lowered.\n'
     '#     The depth-MOVED snap (WW_IMPOSTOR_SNAP=1) is a harness override only;\n'),
    ('#   * the knob rows: the default draws the crisp end and says so; SNAP=1 and\n'
     '#     SLIDER=0 are byte-identical to it; SLIDER=1 is not, and is byte-identical\n'
     '#     to SLIDER=1 SEARCH=16; SEARCH=0 at the smooth end changes it; SLIDER=0.5\n'
     '#     is a third picture; COVFILTER=0 and =1 are byte-identical to the\n'
     '#     default; BLEND=0 (the flat frame) is not the snap.\n',
     '#   * the knob rows: the default draws the crisp end and says so; SLIDER=0\n'
     '#     and BLEND=0 are byte-identical to it (the flat snap); SNAP=1 (the\n'
     '#     depth-moved snap) is not; SLIDER=1 is not, and is byte-identical to\n'
     '#     SLIDER=1 SEARCH=16; SEARCH=0 at the smooth end changes it; SLIDER=0.5\n'
     '#     is a third picture; COVFILTER=0 and =1 are byte-identical to the default.\n'),
    # knob log strings
    ('logsays k_default "slider: 0.00 -- the CRISP end, snap (the default)" "the default draw is the slider\'s crisp end and says so"\n'
     'logsays k_default "frames: ONE, the nearest, with height parallax (the slider\'s crisp end)" "the default draws ONE frame, the nearest, with its parallax"\n'
     'logsays k_default "depth search: off -- the one-step parallax (the slider\'s)" "the crisp end does not search"\n',
     'logsays k_default "slider: 0.00 -- the CRISP end, flat snap (the default)" "the default draw is the slider\'s crisp end and says so"\n'
     'logsays k_default "frames: ONE, the nearest, flat on the card plane (the slider\'s crisp end)" "the default draws ONE frame, the nearest, flat"\n'
     'logsays k_default "depth search: none -- the frame is drawn flat, not moved by its depth" "the crisp end is not moved by its depth"\n'),
    ('logsays k_snap "frames: ONE, the nearest, with height parallax (WW_IMPOSTOR_SNAP=1)" "WW_IMPOSTOR_SNAP=1 is named in the log"\n',
     'logsays k_snap "frames: ONE, the nearest, moved by its depth (WW_IMPOSTOR_SNAP=1, a harness override)" "WW_IMPOSTOR_SNAP=1 is named in the log as the depth-moved harness override"\n'),
    ('same k_default k_snap "WW_IMPOSTOR_SNAP=1 IS the default (the crisp end is snap)"\n',
     'same k_default k_blend0 "the crisp end IS the flat snap (== WW_IMPOSTOR_BLEND=0)"\n'),
    ('differs k_default k_blend0 1000 "FLOOR: the snap is not the flat single frame (it keeps its parallax)"\n',
     'differs k_default k_snap 1000 "FLOOR: the depth-moved snap (WW_IMPOSTOR_SNAP=1) is not the crisp end"\n'),
    # crisp rows
    ('\t# the crisp end (snap): T1, T2 and the tear share gated; T3 and the pop reported (bungo: snap may jump)\n',
     '\t# the crisp end (flat snap): T1, T2 and the tear share gated; T3 and the pop reported (bungo: snap may jump).\n'
     '\t# The tear row is a NAMED KNOWN RED by bungo\'s ruling 2026-09-23 13:1x: still counted, bar not lowered.\n'),
    ('ok "crisp end (snap) el $el: T1 trunk width inside 0.85..1.15 at every azimuth" \\\n'
     '\t\t|| bad "crisp end (snap) el $el: T1 trunk width fails"\n',
     'ok "crisp end (flat snap) el $el: T1 trunk width inside 0.85..1.15 at every azimuth" \\\n'
     '\t\t|| bad "crisp end (flat snap) el $el: T1 trunk width fails"\n'),
    ('ok "crisp end (snap) el $el: T2 never more trunks than the mesh" \\\n'
     '\t\t|| bad "crisp end (snap) el $el: T2 doubled trunk"\n',
     'ok "crisp end (flat snap) el $el: T2 never more trunks than the mesh" \\\n'
     '\t\t|| bad "crisp end (flat snap) el $el: T2 doubled trunk"\n'),
    ('\t\t&& ok "crisp end (snap) el $el: the tear share is within the bar ($T)" || bad "crisp end (snap) el $el: the tear share is over the bar (${T:-no number})"\n',
     '\t\t&& ok "crisp end (flat snap) el $el: the tear share is within the bar ($T)" \\\n'
     '\t\t|| bad "KNOWN RED (bungo\'s ruling 2026-09-23 13:1x: flat snap despite its tear) crisp end (flat snap) el $el: the tear share is over the bar (${T:-no number})"\n'),
]
for old, new in E:
    c = s.count(old)
    assert c == 1, (old[:80], c)
    s = s.replace(old, new)
out = s.encode('utf-8'); assert out.count(b'\r') == cr
open(P, 'wb').write(out); print('patched')
