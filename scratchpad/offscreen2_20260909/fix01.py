import sys
p = 'tests/spells/render_shot.sh'
b = open(p, 'rb').read()
before_cr = b.count(b'\r')
t = b.decode()

subs = [
    ('"$(winlog_opaque $L) of $(winlog_records $L) records at opacity > 0"',
     '"$(winlog_opaque $L) of $(winlog_visible $L) mapped records at opacity > 0"'),
    ('"$(winlog_opaque render_visible) of $(winlog_records render_visible) records at opacity > 0"',
     '"$(winlog_opaque render_visible) of $(winlog_visible render_visible) mapped records at opacity > 0"'),
    ('[ "$(winlog_opaque render_plain)" -eq 0 ] && [ "$(winlog_records render_plain)" -ge 1 ]',
     '[ "$(winlog_opaque render_plain)" -eq 0 ] && [ "$(winlog_visible render_plain)" -ge 1 ]'),
    ('"$(winlog_opaque render_plain) opaque of $(winlog_records render_plain) records"',
     '"$(winlog_opaque render_plain) opaque of $(winlog_visible render_plain) mapped records"'),
    ('[ "$(winlog_opaque bake_oct)" -eq 0 ] && [ "$(winlog_records bake_oct)" -ge 1 ]',
     '[ "$(winlog_opaque bake_oct)" -eq 0 ] && [ "$(winlog_visible bake_oct)" -ge 1 ]'),
    ('"$(winlog_opaque bake_oct) opaque of $(winlog_records bake_oct) records"',
     '"$(winlog_opaque bake_oct) opaque of $(winlog_visible bake_oct) mapped records"'),
]

for old, new in subs:
    n = t.count(old)
    if n != 1:
        sys.stderr.write('anchor %d matches: %r\n' % (n, old[:60]))
        sys.exit(1)
    t = t.replace(old, new)

out = t.encode()
assert out.count(b'\r') == before_cr, 'CR count moved'
open(p, 'wb').write(out)
print('CR', out.count(b'\r'), 'LF', out.count(b'\n'), 'bytes', len(out))
