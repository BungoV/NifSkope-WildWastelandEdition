R = 'E:/Projects/NifskopeWWE-loadorder1/docs/'
edits = [
    ('LODGEN_LEDGER_FORMAT.md', b'--resource --plugins-txt\n', b'--resource --plugins-txt  --mo2-profile  --mo2-mods\n'),
    ('LODGEN_BAKE_RECORD.md',
     b'Given a plugin list \xe2\x80\x94 positionally, or via `--plugins-txt` / `--mo2` \xe2\x80\x94 it also\ndiffs:\n',
     b'Given a plugin list \xe2\x80\x94 positionally, or via `--plugins-txt` / `--mo2` /\n'
     b'`--mo2-profile` \xe2\x80\x94 it also diffs. (`--mo2-profile <profile folder>` reads MO2\'s\n'
     b'`modlist.txt` + `plugins.txt` off disk without usvfs: Fallout4.esm, the DLC and CC\n'
     b'masters present in Data, then each `*` plugin as the FULL PATH found in overwrite,\n'
     b'the enabled mod folders top-down, then Data -- so the record names every plugin\n'
     b'where it really sits. `--print-source` prints the list and the resource stack.)\n'),
]
for name, old, new in edits:
    p = R + name
    b = open(p, 'rb').read()
    cr = b.count(b'\r')
    if cr:
        old = old.replace(b'\n', b'\r\n')
        new = new.replace(b'\n', b'\r\n')
    assert b.count(old) == 1, name
    out = b.replace(old, new)
    assert out.count(b'\r') == cr + (new.count(b'\r') - old.count(b'\r')), name
    with open(p, 'wb') as f:
        f.write(out)
    print(name, 'ok, CR', cr, '->', out.count(b'\r'))
