"""MERGE1 copy of INCR2 install.py (only BACKUP and LOG changed). INCR2 install: make mods/FO4CSLOD/FO4CSLOD/<EDID> equal the staged tree, file by file.

usage: python install.py <staged FO4CSLOD dir> <EDID> [<EDID>...] [--dry]
- Writes ONLY under E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/<EDID>.
- A file whose sha1 already equals the staged one is left alone (the quick-rebake path copies only what moved).
- Every replaced or removed installed file is MOVED (same drive, no copy) to
  scratchpad/incr2_20260926/replaced/<EDID>/<rel> first.
- sha1 before (installed + staged) and after (installed) for every copied file, in install_log.tsv.
- The caller runs the game gate immediately before this script.
"""
import hashlib, os, shutil, sys

DST_ROOT = 'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD'
BACKUP = 'E:/Projects/Fallout 4 Mods/backups/FO4CSLOD_replaced_MERGE1_20260929'
LOG = 'E:/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/install_log.tsv'


def sha1(p):
    h = hashlib.sha1()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 22), b''):
            h.update(b)
    return h.hexdigest()


def files(root):
    out = {}
    for r, _, fs in os.walk(root):
        for x in fs:
            full = os.path.join(r, x)
            out[os.path.relpath(full, root).replace('\\', '/')] = full
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    dry = '--dry' in sys.argv
    stage, edids = args[0], args[1:]
    log = open(LOG, 'a', encoding='utf-8')
    for ed in edids:
        src = os.path.join(stage, ed)
        dst = os.path.join(DST_ROOT, ed)
        assert os.path.isdir(src), src
        s, d = files(src), files(dst) if os.path.isdir(dst) else {}
        same = copied = removed = 0
        for rel in sorted(set(s) | set(d)):
            if rel in d and rel not in s:           # stale installed file: back it up, take it out
                if not dry:
                    b = os.path.join(BACKUP, ed, rel)
                    os.makedirs(os.path.dirname(b), exist_ok=True)
                    before = sha1(d[rel])
                    shutil.move(d[rel], b)
                    log.write(f'{ed}\t{rel}\tremoved\t{before}\t-\t-\n')
                removed += 1
                continue
            hs = sha1(s[rel])
            hd = sha1(d[rel]) if rel in d else '-'
            if hs == hd:
                same += 1
                continue
            copied += 1
            if dry:
                print(f'  would copy {ed}/{rel}')
                continue
            if rel in d:
                b = os.path.join(BACKUP, ed, rel)
                os.makedirs(os.path.dirname(b), exist_ok=True)
                shutil.move(d[rel], b)
            t = os.path.join(dst, rel)
            os.makedirs(os.path.dirname(t), exist_ok=True)
            shutil.copyfile(s[rel], t)
            after = sha1(t)
            log.write(f'{ed}\t{rel}\tcopied\t{hd}\t{hs}\t{after}\n')
            if after != hs:
                print(f'VERIFY FAIL {ed}/{rel}: staged {hs} installed {after}')
                sys.exit(1)
        log.flush()
        print(f'{ed}: {len(s)} staged, {same} already equal, {copied} copied{" (dry)" if dry else ""}, '
              f'{removed} stale removed')


main()
