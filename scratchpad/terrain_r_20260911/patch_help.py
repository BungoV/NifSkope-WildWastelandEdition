p = 'src/nifcli.cpp'
s = open(p, 'r', encoding='utf-8', newline='').read()

a = ('\t\t  << "         [--vt-compress none|zlib] [--vt-btr] [--no-vt-btr] [--vt-estimate]\\n"\n'
     '\t\t  << "         [--vt-height]                    a fourth R16 height sheet per tile,\\n"')

b = ('\t\t  << "         [--vt-compress none|zlib] [--vt-btr] [--no-vt-btr] [--vt-estimate]\\n"\n'
     '\t\t  << "         [--vt-cover-in-color]            put the ground-cover byte in the\\n"\n'
     '\t\t  << "                                          COLOUR sheet alpha (the object\\n"\n'
     '\t\t  << "                                          family coverage slot) instead of the\\n"\n'
     '\t\t  << "                                          mask alpha. OFF: the colour alpha is\\n"\n'
     '\t\t  << "                                          the one slot .lodm 2.1 defines as\\n"\n'
     '\t\t  << "                                          OPACITY, so a consumer that\\n"\n'
     '\t\t  << "                                          alpha-tests it would punch holes in\\n"\n'
     '\t\t  << "                                          thin grass; and it costs 46,240 bytes\\n"\n'
     '\t\t  << "                                          a tile on every cover-FREE tile.\\n"\n'
     '\t\t  << "         [--vt-height]                    a fourth R16 height sheet per tile,\\n"')

assert s.count(a) == 1, s.count(a)
open(p, 'w', encoding='utf-8', newline='').write(s.replace(a, b))
bb = open(p, 'rb').read()
print('CR', bb.count(b'\r'), 'LF', bb.count(b'\n'))
