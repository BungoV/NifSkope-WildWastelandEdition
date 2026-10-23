#!/usr/bin/env python3
"""Lane TESTDRIVER1 (2026-10-22): where every WW_* switch is read, and what a live change of it needs.

Walks src/ for environment reads (qgetenv, qEnvironmentVariable*, getenv and the wwEnv* accessors of src/wwswitch.h),
finds the enclosing function, whether the read is cached in a function-static, and classifies the switch:
  live     (a) re-read per frame / per call, or (b) a static cache that follows the switch generation
  reopen   (c) read while the cell document is built (lights, GI, far LOD, stream seed): the live set re-opens the
               cell document in the same window (process, GL context, archives, model cache kept)
  relaunch     process-wide state fixed at startup (main(), constructors of the window/view/renderer, GL context)
  tool         read by a bake/CLI/test path a cell view session never runs (lodgen, impostor bake, *test.cpp)
Writes:  --tsv <file>  one row per read site;  --inc <file>  the C++ table src/wwswitch.cpp includes (name -> stage).
The hand table OVERRIDES below wins over the rules; each entry says why."""
import argparse, os, re, sys

READ = re.compile(r'\b(qgetenv|qEnvironmentVariable(?:IntValue|IsSet|IsEmpty)?|getenv|wwEnv(?:Str|Int|IsSet|IsEmpty)?)\s*\(\s*("([A-Za-z0-9_]+)")?')
LIT = re.compile(r'"(WW_[A-Z0-9_]+)"')
RANK = {'tool': 0, 'live': 1, 'shaders': 2, 'reopen': 3, 'relaunch': 4}

# name -> (stage, why). Names a rule gets wrong, or the harness depends on; checked by hand.
OVERRIDES = {
    'WW_SETTINGS_SCOPE': ('relaunch', 'QSettings scope chosen in main() before any window'),
    'WW_STDERR_BUFFER': ('relaunch', 'stderr buffering set once in main()'),
    'WW_RENDER_SHOT': ('live', 'the steps verb "shot <out>" names the file; the launch value only arms the handler'),
    'WW_RENDER_PATH': ('live', 'the steps verb "path <file> <out>" takes it as an argument'),
    'WW_RENDER_GPUMS': ('live', 'the steps verb "gpums <frames> <out>" takes it as an argument; path re-reads it per call'),
    'WW_LIVE_STEPS': ('relaunch', 'the driver itself'),
    'WW_TESTDRIVER_RED': ('relaunch', 'the driver red: read once when the driver starts'),
    'WW_CELL_OPEN': ('reopen', 'the cell spec: the reopen builds the new rectangle in the same window'),
    'WW_LOOKDEV_PLUGINS': ('relaunch', 'weather plugin list: LdState keeps the parsed climate for the process'),
    'WW_CELL_SPEED_REOPEN': ('relaunch', 'the speed harness own reopen list, taken once'),
    'WW_LIVE_SERVE': ('relaunch', 'the driver: keeps the window for port commands after the steps'),
    'WW_CELL_GI_VIS': ('shaders', 'march mode is a #define every shader gets at load (Shader::load)'),
    'WW_CELL_PV_RED': ('shaders', 'its march reds pick the #define every shader gets at load (Shader::load)'),
    'WW_SETUP_MEMO': ('live', 'setupMemo() re-reads it on any live set (and voids its stamps)'),
    'WW_SETUP_MEMO_CHECK': ('live', 'setupMemo() re-reads it on any live set'),
    'WW_CELL_HDR_RED': ('live', 'a member default; hst() re-reads it on a live set'),
}

# init-once state functions (read once under a loaded flag) that re-read their prefix on a live set: class (b)
SINGLETONS = {('celllights.cpp', 'st'), ('celllights.cpp', 'ao'), ('scenelighting.cpp', 'st'), ('lookdevstage.cpp', 'st'),
              ('shadowset.cpp', 'ss'), ('cellvolair.cpp', 'vs'), ('cellcull.cpp', 'st'), ('cellpost.cpp', 'ps'),
              ('sunshadow.cpp', 'cs'), ('cellssr.cpp', 'ssr'), ('cellbloom.cpp', 'bs'), ('gametaa.cpp', 'ts'),
              ('cellwater.cpp', 'st'), ('cellpasstime.cpp', 'st'), ('cellfxlit.cpp', 'st'), ('cellssgi.cpp', 'st'),
              ('cellaodecalgl.cpp', 'st'), ('cellgrass.cpp', 'gs')}

# file rules, first match wins: (regex on path, stage)
FILE_RULES = [
    (r'(test|tests?)\.cpp$|/spells/|nifcli\.cpp$|harnesswindow\.cpp$', 'tool'),
    (r'/lodgen[a-z]*\.cpp$|/lodgenao\.h$|impostorpreview|bake_|/impostorbake', 'tool'),
    (r'/gl/glcontext\.cpp$', 'shaders'),	# only Shader::load, Program::load, updateShaders (stage_of)
    (r'/main\.cpp$', 'relaunch'),
    (r'/cell(view|stream|farlod|mesh|texahead|splat|cable|aodecal|modsource|speed|census)\.cpp$|/probe[a-z]*\.cpp$|/esm[a-z]*\.cpp$|/btdterrain\.cpp$|/lodinative\.cpp$|/lodofile\.cpp$|/impostorchunk\.cpp$|/aovolume\.cpp$|/nativeemit\.cpp$', 'reopen'),
]
LOAD_FN = re.compile(r'(Publish|Load|load|Build|build|Open|open|Bake|bake|Pick|Import|import)')
CTOR = re.compile(r'\b(\w+)::\1$')
STARTUP_FN = re.compile(r'^(main|NifSkope::restoreUi|NifSkope::loadSettings|NifSkope::NifSkope|GLView::GLView|GLView::create|Renderer::Renderer|Scene::Scene|NifSkope::initActions|NifSkope::initDockWidgets|NifSkope::initMenu|NifSkope::initToolBars)$')


def strip_code(text):
    """Comments and string/char literals blanked (same length, newlines kept), so braces can be counted."""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = ' '
            i = j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        elif c == 'R' and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == '_')):
            m = re.match(r'R"([^(\s]*)\(', text[i:])
            if not m:
                i += 1
                continue
            end = ')' + m.group(1) + '"'
            j = text.find(end, i + m.end())
            j = n if j < 0 else j + len(end)
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        elif c == '"' or c == "'":
            j = i + 1
            while j < n and text[j] != c:
                if text[j] == '\\':
                    j += 1
                elif text[j] == '\n':
                    break
                j += 1
            for k in range(i + 1, min(j, n)):
                out[k] = ' '
            i = j + 1
        else:
            i += 1
    return ''.join(out)


def fn_name(head):
    """The qualified name before the first top-level '(' of a function head, or None."""
    h = ' '.join(head.split())
    if not h or re.match(r'^(namespace|class|struct|union|enum|extern|template\s*<[^>]*>\s*(class|struct))\b', h):
        return None
    if '(' not in h or re.search(r'(^|\s)(if|for|while|switch|catch|do|else)\s*\(', h[:h.find('(') + 1] + ' '):
        return None
    if '=' in h[:h.find('(')]:
        return None	# a lambda or an initializer
    m = re.search(r'([~\w:]+)\s*\($', h[:h.find('(') + 1])
    if not m:
        return None
    name = m.group(1)
    if name in ('if', 'for', 'while', 'switch', 'return', 'sizeof', 'catch'):
        return None
    return name


def scan(path, rel):
    text = open(path, encoding='utf-8', errors='replace').read()
    code = strip_code(text)
    lines = text.split('\n')
    # the WW_RENDER_SHOT handler: from its connect to the next harness block (IMPOSTOR CARD BAKER)
    first = next((k + 1 for k, l in enumerate(lines) if 'fname.isEmpty()' in l and 'wwEnvIsSet( "WW_RENDER_SHOT" )' in l), 0)
    if first:
        last = next((k + 1 for k, l in enumerate(lines) if k + 1 > first and 'IMPOSTOR CARD BAKER' in l), 0)
        SHOT_RANGE[rel] = (first, last)
    # per position: the enclosing function and whether a static initializer encloses it
    sites = []
    stack = []	# entries: (type, name, static)
    head_start = 0
    line_of = []
    ln = 1
    for ch in code:
        line_of.append(ln)
        if ch == '\n':
            ln += 1
    reads = []
    for m in READ.finditer(text):
        if code[m.start()] == ' ':
            continue	# inside a comment or string
        reads.append(m)
    ri = 0
    i = 0
    n = len(code)
    events = []
    for k, ch in enumerate(code):
        if ch in '{};':
            events.append(k)
    ev = 0
    def current():
        fn = None
        st = False
        fs = -1
        for t, name, s, k0 in stack:
            if t == 'fn':
                fn, fs = name, k0
            if s:
                st = True
        return fn, st, fs
    last_break = 0
    for m in reads:
        pos = m.start()
        while ev < len(events) and events[ev] < pos:
            k = events[ev]
            ch = code[k]
            head = code[last_break:k]
            if ch == '{':
                in_fn = any(t == 'fn' for t, _, _, _ in stack)
                name = None if in_fn else fn_name(head)
                hs = ' '.join(head.split())
                is_static = in_fn and bool(re.match(r'^static\b', hs))
                kind = 'fn' if name else ('rec' if re.match(r'^(template\s*<.*>\s*)?(struct|class|union)\b', hs) and not in_fn else 'blk')
                if kind == 'rec':
                    rm = re.match(r'^(?:template\s*<.*>\s*)?(?:struct|class|union)\s+(\w+)', hs)
                    name = rm.group(1) if rm else None
                if kind == 'blk':
                    name = hs
                stack.append((kind, name, is_static, k))
            elif ch == '}':
                if stack:
                    stack.pop()
            last_break = k + 1
            ev += 1
        fn, st, fs = current()
        if fn is None and stack and stack[-1][0] == 'rec':
            fn = '(member default)'
        # TDFIX2: an inline constructor of a struct (`struct Switches { Switches() { ...reads... } }`): its name
        recs = [nm for t, nm, _, _ in stack if t == 'rec']
        heads = [nm for t, nm, _, _ in stack if t == 'blk']
        once = fs >= 0 and once_guarded(code, fs, fn, recs, heads)
        stmt = ' '.join(code[last_break:pos].split())
        if re.match(r'^static\b', stmt):
            st = True
        name = m.group(3) or ''
        sites.append((rel, line_of[pos], fn or '(file scope)', st, m.group(1), name, lines[line_of[pos] - 1].strip()[:160]))
        if once:
            ONCE.add((rel, line_of[pos]))
    return sites, set(LIT.findall(re.sub(r'wwSwitchMovedFor\s*\([^)]*\)', '', text)))	# prefixes are not names


SHOT_RANGE = {}	# rel -> (first, last) line of the WW_RENDER_SHOT handler in createWindow
ONCE = set()	# (rel, line) of reads a once-guard caches for the process (TDFIX2)
ONCE_GUARD = re.compile(r'^if\s*\(\s*!\s*\w+\s*\.\s*(loaded|init)\s*\)$')
SENTINEL = re.compile(r'^if\s*\(\s*(\w+)\s*<\s*0\s*\)$')
FOLLOWS = re.compile(r'wwSwitchMovedFor|wwSwitchMoved\b|WW_SWITCH_STATIC|wwSwitchGen')


def body_of(code, start):
    """The function body from its '{' at `start` to the matching '}' (comments and strings are blanked in `code`)."""
    d = 0
    for k in range(start, len(code)):
        if code[k] == '{':
            d += 1
        elif code[k] == '}':
            d -= 1
            if d == 0:
                return code[start:k + 1]
    return code[start:]


def once_guarded(code, fs, fn, recs, heads):
    """True when the read's function caches it once for the process: an enclosing block is exactly
    `if ( !s.loaded )` / `if ( !s.init )` (no generation check in it), or `if ( on < 0 )` on a `static int on = -1`,
    or the read is in the constructor of a struct some function keeps as `static const`."""
    short = (fn or '').split('::')[-1]
    if recs and short == recs[-1]:
        return bool(re.search(r'static\s+const\s+' + re.escape(short) + r'\s+\w+\s*;', code))
    for h in heads:
        if ONCE_GUARD.match(h):
            return True
        m = SENTINEL.match(h)
        if m and re.search(r'static\s+int\s+' + m.group(1) + r'\s*=\s*-1\s*;', body_of(code, fs)):
            return True
    return False


def stage_of(rel, fn, static, line=0):
    if (rel.split('/')[-1], (fn or '').split('::')[-1]) in SINGLETONS:
        return 'live', 'init-once state, re-read when a live set moves its prefix (b)'
    if (rel, line) in ONCE and not any(s == 'tool' and re.search(rx, '/' + rel) for rx, s in FILE_RULES):
        return 'relaunch', 'read once for the process (a loaded/init flag or static const struct, no generation check)'
    for rx, s in FILE_RULES:
        if s == 'shaders' and not re.search(r'(Shader::load|Program::load|updateShaders)$', fn or ''):
            continue
        if re.search(rx, '/' + rel):
            if s == 'reopen' and static:
                return 'relaunch', 'cell build, function static: read once for the process (a reopen keeps it)'
            return s, {'tool': 'bake/CLI/test path', 'relaunch': 'startup (main)', 'shaders': 'read when the shader programs load (in place: reload shaders)', 'reopen': 'read while the cell document is built'}[s]
    if fn == '(member default)':
        return 'relaunch', 'member default of a long-lived object'
    if fn and (STARTUP_FN.match(fn) or CTOR.search(fn)):
        return 'relaunch', 'constructor/startup of a window-lifetime object'
    if fn and re.search(r'/(nifskope_ui)\.cpp$', '/' + rel) and fn == 'NifSkope::createWindow':
        a, b = SHOT_RANGE.get(rel, (0, -1))
        if a <= line <= b:
            return 'reopen', 'shot handler option: the live reopen runs the handler again (window, camera, options)'
        return 'relaunch', 'createWindow: picks which harness the window runs'
    if fn and LOAD_FN.search(fn.split('::')[-1]) and re.search(r'/gl/', '/' + rel):
        return 'reopen', 'read when the renderer is handed the cell (publish/load/build)'
    if static:	# TDFIX2: a C++ function static is read once for the process (WW_SWITCH_STATIC sites are not 'static')
        return 'relaunch', 'function static: read once for the process (not WW_SWITCH_STATIC)'
    return 'live', 're-read per frame or per call (a)'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.join(os.path.dirname(__file__), '..'))
    ap.add_argument('--tsv')
    ap.add_argument('--inc')
    a = ap.parse_args()
    src = os.path.join(a.root, 'src')
    rows, lits = [], set()
    for dp, _, fs in os.walk(src):
        for f in sorted(fs):
            if not f.endswith(('.cpp', '.h')) or f.startswith('wwswitch'):
                continue
            p = os.path.join(dp, f)
            rel = os.path.relpath(p, a.root).replace('\\', '/')
            s, l = scan(p, rel)
            rows += s
            lits |= l
    names = {}
    for rel, line, fn, st, how, name, txt in rows:
        if not name.startswith('WW_'):
            continue
        stg, why = stage_of(rel, fn, st, line)
        cur = names.get(name)
        if cur is None or RANK[stg] > RANK[cur[0]]:
            names[name] = (stg, why, f'{rel}:{line} {fn}')
    for name in sorted(lits - set(names)):
        names[name] = ('reopen', 'name passed through a variable: conservative', '-')
    for name, (stg, why) in OVERRIDES.items():
        names[name] = (stg, 'hand: ' + why, names.get(name, ('', '', '-'))[2])
    if a.tsv:
        with open(a.tsv, 'w', encoding='utf-8', newline='\n') as o:
            o.write('file\tline\tfunction\tstatic\tread\tname\tsite_stage\tname_stage\twhy\n')
            for rel, line, fn, st, how, name, txt in rows:
                stg, why = stage_of(rel, fn, st, line)
                ns = names.get(name, ('-',))[0]
                o.write(f'{rel}\t{line}\t{fn}\t{int(st)}\t{how}\t{name or "(variable)"}\t{stg}\t{ns}\t{why}\n')
    if a.inc:
        with open(a.inc, 'w', encoding='utf-8', newline='\n') as o:
            o.write('// GENERATED by tools/wwswitch_survey.py --inc (lane TESTDRIVER1). Do not edit: change the script.\n')
            o.write('// name, stage (0 live, 1 reopen, 2 relaunch, 3 tool, 4 shaders), where (the read that decided it)\n')
            code = {'live': 0, 'reopen': 1, 'relaunch': 2, 'tool': 3, 'shaders': 4}
            for name in sorted(names):
                stg, why, where = names[name]
                o.write('{ "%s", %d, "%s" },\n' % (name, code[stg], (where + ' | ' + why).replace('\\', '/').replace('"', "'")))
    cnt = {}
    for v in names.values():
        cnt[v[0]] = cnt.get(v[0], 0) + 1
    print(f'sites {len(rows)} names {len(names)} ' + ' '.join(f'{k} {v}' for k, v in sorted(cnt.items())))


if __name__ == '__main__':
    main()
