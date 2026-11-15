#!/usr/bin/env python3
"""lane VKOBJ6: the offline census of GL draw sites (every object, effect, particle, decal draw and every other draw in src).

Every draw call in src/ is a site keyed file | enclosing function | call token | ordinal (never a line number). sites.tsv
gives each its class:
  twin     a Vulkan route under WW_VKVIEW=1 / WW_VKOBJ_SHADE=1 is tried first; its guard token (the twin's entry, e.g.
           WwBl::vkCellDraw) must sit in the same function, or the row FAILS
  refusal  an object draw GL keeps by a named refusal (the reason is the note)
  glonly   an object / effect / particle / decal draw with no Vulkan route yet (Part B's port list)
  other    not an object draw (a screen pass, a shadow caster, an editor overlay...): the owner is the note
The gate FAILS on a site the table does not list (a new GL-only draw), a row whose site is gone, a twin row whose guard
is missing, and a malformed row. --self-red runs both reds on in-memory copies (an injected unlisted draw, a twin's guard
stripped) and requires each to FAIL by name, in the same run as the pass.

usage: python -I objcensus.py <repo> [--self-red] [--md]   (exit 0 pass, 1 fail)
"""
import os
import re
import sys

DRAW = re.compile(r'(?<![\w>.])(?:\w+(?:->|\.))*?(glDraw(?!Buffers?\b)\w+|glMultiDraw\w+|WwBl::drawElements|'
                  r'WwBl::multiDrawElements|GpuDriven::poolDraw)\s*\(|(?:\b\w+(?:->|\.))(mdi|drawShape)\s*\(')
FUNC = re.compile(r'^(?!(?:if|for|while|switch|return|else|do|case|namespace|struct|class|enum|union|static_assert|'
                  r'typedef|using|template|extern)\b)[A-Za-z_~][\w:<>,\*&~ \t]*?\b(~?\w+(?:::~?\w+)*)\s*\(')
EXTS = ('.cpp', '.h', '.hpp', '.inc', '.cc')
CLASSES = ('twin', 'refusal', 'glonly', 'other')


def strip_comments(lines):
    """the lines with // and /* */ comments blanked (strings are not parsed: no draw call sits in one)"""
    out, inblock = [], False
    for ln in lines:
        s, i, buf = ln, 0, []
        while i < len(s):
            if inblock:
                j = s.find('*/', i)
                if j < 0:
                    i = len(s)
                else:
                    inblock, i = False, j + 2
            else:
                j, k = s.find('/*', i), s.find('//', i)
                if k >= 0 and (j < 0 or k < j):
                    buf.append(s[i:k])
                    i = len(s)
                elif j >= 0:
                    buf.append(s[i:j])
                    inblock, i = True, j + 2
                else:
                    buf.append(s[i:])
                    i = len(s)
        out.append(''.join(buf))
    return out


def scan_text(rel, text):
    """[(key, line, function, token)] for one file, and {function: body text} for the guard check"""
    lines = strip_comments(text.splitlines())
    sites, bodies, func, seen = [], {}, '<file>', {}
    for n, ln in enumerate(lines, 1):
        m = FUNC.match(ln)
        if m and not ln.rstrip().endswith(';'):
            func = m.group(1)
        bodies[func] = bodies.get(func, '') + ln + '\n'
        for d in DRAW.finditer(ln):
            tok = d.group(1) or d.group(2)
            k = (func, tok)
            seen[k] = seen.get(k, 0) + 1
            sites.append(('%s|%s|%s|%d' % (rel, func, tok, seen[k]), n, func, tok))
    return sites, bodies


def scan_repo(repo, override=None):
    """every site under src/; override = {rel: text} replaces a file's text in memory (the reds)"""
    override = override or {}
    sites, bodies = [], {}
    root = os.path.join(repo, 'src')
    for dp, dn, fn in os.walk(root):
        dn.sort()
        for f in sorted(fn):
            if not f.endswith(EXTS):
                continue
            rel = os.path.relpath(os.path.join(dp, f), repo).replace('\\', '/')
            if rel in override:
                text = override[rel]
            else:
                with open(os.path.join(dp, f), encoding='utf-8', errors='replace') as fh:
                    text = fh.read()
            if 'Draw' not in text and 'mdi' not in text and 'drawShape' not in text:
                continue
            s, b = scan_text(rel, text)
            sites += s
            for fun, body in b.items():
                bodies[(rel, fun)] = body
    return sites, bodies


def load_table(path):
    rows, bad = {}, []
    with open(path, encoding='utf-8') as fh:
        for n, ln in enumerate(fh, 1):
            ln = ln.rstrip('\n')
            if not ln.strip() or ln.startswith('#'):
                continue
            p = ln.split('\t')
            if len(p) != 4 or p[1] not in CLASSES or not p[3].strip() or (p[1] == 'twin' and not p[2].strip()):
                bad.append('sites.tsv:%d malformed (key, class, guard, note; a twin names its guard; no blank note)' % n)
                continue
            if p[0] in rows:
                bad.append('sites.tsv:%d duplicate key %s' % (n, p[0]))
            rows[p[0]] = (p[1], p[2].strip(), p[3].strip())
    return rows, bad


def check(sites, bodies, rows, bad):
    """the verdict lines: FAIL ... per defect"""
    fails = list('FAIL ' + b for b in bad)
    keys = set()
    for key, line, func, tok in sites:
        keys.add(key)
        if key not in rows:
            fails.append('FAIL unlisted %s (line %d)' % (key, line))
            continue
        cls, guard, _ = rows[key]
        if cls == 'twin':
            rel = key.split('|')[0]
            body = bodies.get((rel, func), '')
            for g in guard.split('+'):
                if g not in body:
                    fails.append('FAIL guard %s missing from %s (%s)' % (g, func, key))
    for key in rows:
        if key not in keys:
            fails.append('FAIL stale %s (no such site)' % key)
    return fails


def counts(sites, rows):
    c = dict((k, 0) for k in CLASSES)
    for key, *_ in sites:
        if key in rows:
            c[rows[key][0]] += 1
    return c


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not args:
        print(__doc__)
        return 2
    repo = args[0]
    table = os.path.join(repo, 'tests', 'vkframe', 'objcensus', 'sites.tsv')
    rows, bad = load_table(table)
    sites, bodies = scan_repo(repo)
    if '--md' in sys.argv:
        for key, line, func, tok in sites:
            cls, guard, note = rows.get(key, ('UNLISTED', '', ''))
            rel = key.split('|')[0]
            print('| %s:%d | %s | %s | %s | %s | %s |' % (rel, line, func, tok, cls, guard or '-', note or '-'))
        return 0
    if '--hints' in sys.argv:
        # a row writer's aid: the Vulkan-route tokens each draw function's body names (candidate guards)
        vk = re.compile(r'\b(?:WwBl::vk\w+|WwVkPresent::\w+|ww\w*Vk\w*|vk\w*(?:Draw|On|Shade)\w*)\b')
        done = set()
        for key, line, func, tok in sites:
            rel = key.split('|')[0]
            if (rel, func) in done:
                continue
            done.add((rel, func))
            hit = sorted(set(vk.findall(bodies.get((rel, func), ''))))
            print('%s|%s: %s' % (rel, func, ' '.join(hit) if hit else '-'))
        return 0
    fails = check(sites, bodies, rows, bad)
    c = counts(sites, rows)
    for f in fails:
        print(f)
    print('OBJCENSUS sites %d: twin %d, refusal %d, glonly %d, other %d; rows %d -> %s'
          % (len(sites), c['twin'], c['refusal'], c['glonly'], c['other'], len(rows), 'FAIL' if fails else 'OK'))
    ok = not fails
    if '--self-red' in sys.argv:
        # red 1: an unlisted GL draw injected into a copy of the first twin row's file, in memory
        twin_keys = [k for k, v in rows.items() if v[0] == 'twin']
        if not twin_keys:
            print('OBJCENSUS red inject: no twin row to inject beside -> TESTS NOTHING')
            return 1
        rel = twin_keys[0].split('|')[0]
        with open(os.path.join(repo, rel), encoding='utf-8', errors='replace') as fh:
            text = fh.read()
        inj = text + '\nvoid objcensusRedInject( QOpenGLFunctions * f )\n{\n\tf->glDrawArrays( GL_TRIANGLES, 0, 3 );\n}\n'
        s1, b1 = scan_repo(repo, {rel: inj})
        f1 = [f for f in check(s1, b1, rows, bad) if 'unlisted' in f and 'objcensusRedInject' in f]
        print('OBJCENSUS red inject (%s): %s' % (rel, 'FAIL as it must' if f1 else 'PASSED -> the gate is blind'))
        # red 2: a twin row's guard token stripped from its function, in memory
        key = twin_keys[0]
        guard = rows[key][1].split('+')[0]
        s2, b2 = scan_repo(repo, {rel: text.replace(guard, 'objcensusRedNoGuard')})
        f2 = [f for f in check(s2, b2, rows, bad) if f.startswith('FAIL guard ' + guard)]
        print('OBJCENSUS red guard (%s, %s): %s' % (key, guard, 'FAIL as it must' if f2 else 'PASSED -> the gate is blind'))
        ok = ok and bool(f1) and bool(f2)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
