"""Seed a WW_SETTINGS_SCOPE with the GAME MANAGER state of THIS MACHINE, never of the user's profile.

Usage: python settings_scope_game.py <scope> <out.reg>     (then: reg import <out.reg>)

Why (lane FIX1 fix 4, 2026-09-26): a GUI spell runs in its own wiped scope, and an empty
scope is a Game Manager FIRST INSTALL. That costs two things:

  * GameManager's constructor (src/gamemanager.cpp, reached from main.cpp before any WW window
    placement exists) shows "Initializing the Game Manager" as an OPAQUE QProgressDialog on the
    PRIMARY monitor -- render_shot.sh's outside sampler caught it at 842,446 on DISPLAY1, alpha 255;
  * init_settings() records the game's path but calls find_paths() before load() has filled
    gamePaths[], so "Game Folders" comes out EMPTY: no textures, no materials, no skeleton.hkx
    from the archives. refraction.sh's normal-map row and skeleton_overlay.sh's clip floor (d')
    went red on exactly that, while they had passed on the user's profile, which carries folders
    the Settings dialog once filled in.

So this writes what GameManager::find_paths() WOULD have found for Fallout 4 on this machine:
Game Paths / Game Folders / Game Status for "Fallout 4" and "Game Manager Version" 2. The path is
read from the same registry key the app reads (HKLM\\SOFTWARE\\Bethesda Softworks\\Fallout4,
32-bit view, "Installed Path" then "Path"). The folders are Data\\Textures and Data\\Materials
where they exist, then every Data\\*.ba2 / *.bsa in get_archive_list()'s order (base game, then
DLC/patch, then mods, each group by descending lower-cased name). find_paths() additionally drops
an archive holding no textures/ or materials/ entry; this keeps it (a superset, resolved lazily).

Values are QSettings "@Variant(...)" strings as the Windows backend writes them: a QDataStream at
version Qt_4_0 (no null flag), one byte per UTF-16 unit, stored as REG_BINARY.
Prints one line: what it seeded, or "no Fallout 4 install" (then it writes only the version).
"""
import os
import struct
import sys
import winreg


def qstr(s):
    b = s.encode('utf-16-be')
    return struct.pack('>I', len(b)) + b


def variant_map(d):
    # QVariant(QVariantMap): type 8, count, then (QString key, QVariant value) pairs
    out = struct.pack('>I', 8) + struct.pack('>I', len(d))
    for k, v in d.items():
        out += qstr(k) + v
    return out


def v_string(s):
    return struct.pack('>I', 10) + qstr(s)


def v_stringlist(l):
    return struct.pack('>I', 11) + struct.pack('>I', len(l)) + b''.join(qstr(s) for s in l)


def v_bool(b):
    return struct.pack('>I', 1) + (b'\x01' if b else b'\x00')


def reg_variant(name, payload):
    s = '@Variant(' + ''.join(chr(x) for x in payload) + ')'
    raw = s.encode('utf-16-le')
    return '"%s"=hex:%s' % (name, ','.join('%02x' % x for x in raw))


def fo4_path():
    for val in ('Installed Path', 'Path'):
        try:
            k = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\Bethesda Softworks\Fallout4', 0,
                               winreg.KEY_READ | winreg.KEY_WOW64_32KEY)
            p = str(winreg.QueryValueEx(k, val)[0]).replace('"', '')
        except OSError:
            continue
        if p and os.path.isdir(p):
            return os.path.normpath(p).replace('\\', '/').rstrip('/')
    return ''


def archive_order(names):
    keyed = []
    for n in names:
        low = n.lower()
        c = '2'
        if low.startswith('fallout'):
            c = '1' if ('update' in low or low.endswith('patch.ba2')) else '0'
        elif low.startswith('dlc'):
            c = '1'
        keyed.append((c + low, n))
    return [n for _, n in sorted(keyed, reverse=True)]


def main():
    scope, out = sys.argv[1], sys.argv[2]
    if not scope or len(scope) > 40 or any(not (c.isalnum() or c in '-_') for c in scope):
        sys.exit('REFUSED: scope %r' % scope)
    key = 'HKEY_CURRENT_USER\\Software\\NifTools\\NifSkope 2.0 ' + scope
    lines = ['Windows Registry Editor Version 5.00', '', '[' + key + ']',
             '"Game Manager Version"=dword:00000002']
    game = fo4_path()
    if game:
        data = game + '/Data'
        folders = [data + '/' + f for f in ('Textures', 'Materials') if os.path.isdir(os.path.join(data, f))]
        arch = [n for n in os.listdir(data) if n.lower().endswith(('.ba2', '.bsa'))
                and os.path.isfile(os.path.join(data, n))]
        folders += [data + '/' + n for n in archive_order(arch)]
        lines.append(reg_variant('Game Paths', variant_map({'Fallout 4': v_string(game)})))
        lines.append(reg_variant('Game Folders', variant_map({'Fallout 4': v_stringlist(folders)})))
        lines.append(reg_variant('Game Status', variant_map({'Fallout 4': v_bool(True)})))
        print('scope %s: Fallout 4 at %s, %d folder(s)/archive(s)' % (scope, game, len(folders)))
    else:
        print('scope %s: no Fallout 4 install in the registry; Game Manager Version only' % scope)
    for l in lines:
        if l.startswith('[') and l != '[' + key + ']':
            sys.exit('REFUSED: header outside the scope')
    with open(out, 'wb') as f:
        f.write(('﻿' + '\r\n'.join(lines) + '\r\n').encode('utf-16-le'))


if __name__ == '__main__':
    main()
