BS = chr(92)
p = 'src/esmdata.cpp'; s = open(p, 'rb').read().decode('utf-8')
old = '"Landscape' + BS + 'Ground' + BS + 'CommonwealthDefault01_'
new = '"Landscape' + BS * 2 + 'Ground' + BS * 2 + 'CommonwealthDefault01_'
print('before', s.count(old), s.count(new))
if s.count(new) == 0:
    s = s.replace(old, new)
open(p, 'wb').write(s.encode('utf-8'))
s = open(p, 'rb').read().decode('utf-8'); print('after', s.count(new))
