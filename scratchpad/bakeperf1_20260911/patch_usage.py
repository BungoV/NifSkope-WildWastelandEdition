"""BAKEPERF1: --threads in the CLI usage text, above the --slot-fallback entry."""
P = 'src/nifcli.cpp'
s = open(P, encoding='utf-8', newline='').read()
anchor = '\t\t  << "  lodgen ... --terrain-region ... [--slot-fallback]' + chr(92) + 'n"\n'
assert s.count(anchor) == 1, s.count(anchor)
added = (
    '\t\t  << "  lodgen ... --terrain-region ... [--threads N]' + chr(92) + 'n"\n'
    '\t\t  << "                                          how many cores the chunk queue,' + chr(92) + 'n"\n'
    '\t\t  << "                                          the tile bakes and the BC encoders' + chr(92) + 'n"\n'
    '\t\t  << "                                          may use. 0 or absent = the machine;' + chr(92) + 'n"\n'
    '\t\t  << "                                          1 is the EXACT way back (one world,' + chr(92) + 'n"\n'
    '\t\t  << "                                          one cache set, one chunk at a time)' + chr(92) + 'n"\n'
    '\t\t  << "                                          and every output file is' + chr(92) + 'n"\n'
    '\t\t  << "                                          byte-identical either way' + chr(92) + 'n"\n'
)
s = s.replace(anchor, added + anchor)
open(P, 'w', encoding='utf-8', newline='').write(s)
print('usage patched')
