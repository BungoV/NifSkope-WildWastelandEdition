import io

BS = chr(92)
NL = BS + 'n'
Q = '"'

p = 'src/nifcli.cpp'
s = io.open(p, 'r', encoding='utf-8', newline='').read()

anchor = ('\t\t  << ' + Q + '                                          Off, the three sheets are byte for'
          + NL + Q + '\n'
          + '\t\t  << ' + Q + '                                          byte what they have always been.'
          + NL + Q + '\n')
assert s.count(anchor) == 1, s.count(anchor)

lines = [
    '  lodgen ... [--roads] [--no-roads] [--road-cover-suppress F]',
    '                                          --roads (ON by default, both',
    '                                          targets) rasterises the placed',
    '                                          road meshes top-down into the far',
    '                                          terrain COLOUR sheet, the way',
    '                                          vanilla does: a STAT whose model',
    '                                          sits under Landscape/Roads or',
    '                                          Landscape/Sidewalks, its own',
    '                                          material diffuse, the topmost',
    '                                          triangle winning, alpha-tested',
    '                                          shapes honouring their cut-out.',
    '                                          The NORMAL sheet is not touched:',
    '                                          vanilla does not put the road in',
    '                                          it (measured). Ground cover under',
    '                                          a road is scaled by',
    '                                          --road-cover-suppress F (default 1',
    '                                          = no grass under the road, 0 =',
    '                                          leave the cover plane alone).',
    '                                          --no-roads is byte-identical to',
    '                                          the bake before roads existed.',
]
add = anchor + ''.join('\t\t  << ' + Q + ln + NL + Q + '\n' for ln in lines)
s = s.replace(anchor, add)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('ok, help lines added:', len(lines))
