import io
import sys

p = 'src/nifcli.cpp'
s = io.open(p, encoding='utf-8').read()

OLD = ('\t\t  << "  lodgen ... [--road-composite max-z|blend] [--road-detail 0..1]'
       '\\n"\n')
NEW = ('\t\t  << "  lodgen ... [--road-composite max-z|blend] [--road-detail 0..1]'
       '\\n"\n'
       '\t\t  << "             [--road-ground-paint 0..1]\\n"\n')
if s.count(OLD) != 1:
    sys.exit('help anchor 1: %d' % s.count(OLD))
s = s.replace(OLD, NEW)

OLD2 = ('\t\t  << "                                          road at its 256-unit UV '
        'repeat.\\n"\n')
NEW2 = OLD2 + (
    '\t\t  << "                                          --road-ground-paint is how much\\n"\n'
    '\t\t  << "                                          a shape INSIDE a road model whose\\n"\n'
    '\t\t  << "                                          material lives under\\n"\n'
    '\t\t  << "                                          materials/Landscape/Ground/ paints\\n"\n'
    '\t\t  << "                                          the sheet -- the verge, modelled\\n"\n'
    '\t\t  << "                                          and materialled as terrain. Such\\n"\n'
    '\t\t  << "                                          shapes win 36.1 per cent of the\\n"\n'
    '\t\t  << "                                          road plane on chunk (-20,20) and\\n"\n'
    '\t\t  << "                                          24.9 on (-8,8), and the step where\\n"\n'
    '\t\t  << "                                          they meet the asphalt reads 15.387\\n"\n'
    '\t\t  << "                                          and 8.010 against vanilla 5.362\\n"\n'
    '\t\t  << "                                          and 5.138 (lane ROADS4). 0 leaves\\n"\n'
    '\t\t  << "                                          the landscape colour there and\\n"\n'
    '\t\t  << "                                          also stops that shape suppressing\\n"\n'
    '\t\t  << "                                          ground cover, since the multiply\\n"\n'
    '\t\t  << "                                          is on coverage; 1 is ROADS1s bake.\\n"\n')
if s.count(OLD2) != 1:
    sys.exit('help anchor 2: %d' % s.count(OLD2))
s = s.replace(OLD2, NEW2)

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('help ok  CR', io.open(p, 'rb').read().count(b'\r'))
