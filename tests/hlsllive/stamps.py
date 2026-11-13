"""Content keys for the incremental SPIR-V step (HLSLBULK1 R7).
A stage .spv is rebuilt only when its key changes: sha1 of the .hlsl, every file it #includes (recursively), the define set, the DXC
flags (DXC_OPT, the exe's size and time) and the tools that shape the bytes (spvgl.py, build_spv.py). The key sits beside the .spv
as `<name>.spv.key`. A program's key is the sha1 of its stage keys; the link result (programs.cache) and the A4 verdict
(a4cache.txt) are cached against it, the A4 one also against the GLSL sources it compares with and the harness files."""
import hashlib
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
DXC = 'E:/Tools/dxc/bin/x64/dxc.exe'
INC = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def sha(*parts):
    h = hashlib.sha1()
    for p in parts:
        h.update(p if isinstance(p, bytes) else str(p).encode('utf-8'))
        h.update(b'\x00')
    return h.hexdigest()


def _file(path):
    try:
        return open(path, 'rb').read()
    except OSError:
        return b'<missing>'


def tools_key(names):
    return sha(*[_file(os.path.join(HERE, n)) for n in names])


def _code(path):
    """HLSLFIX2: a .py file's code only: the parsed tree with every docstring dropped (comments, blank lines, layout and
    docstrings do not reach it; any change to a statement, name or constant does). A file that does not parse: its bytes."""
    import ast
    import sys
    raw = _file(path)
    try:
        t = ast.parse(raw)
    except (SyntaxError, ValueError):
        return raw
    for n in ast.walk(t):
        b = getattr(n, 'body', None)
        if isinstance(n, (ast.Module, ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)) and b and isinstance(b[0], ast.Expr) \
                and isinstance(b[0].value, ast.Constant) and isinstance(b[0].value.value, str):
            n.body = b[1:] or [ast.Pass()]
    return ('py%d.%d|' % sys.version_info[:2] + ast.dump(t)).encode('utf-8')


def code_key(names):
    """tools_key over the code of each file (_code): a comment or docstring edit keeps the key, so cached verdicts stay"""
    return sha(*[_code(os.path.join(HERE, n)) for n in names])


def dxc_key():
    try:
        st = os.stat(DXC)
        return '%d:%d' % (st.st_size, int(st.st_mtime))
    except OSError:
        return 'nodxc'


def include_closure(path, find=None, seen=None):
    """-> sorted list of the files `path` pulls in by #include "x" (relative to the including file, else find(name) -> path)"""
    seen = set() if seen is None else seen
    if path in seen:
        return seen
    seen.add(path)
    txt = _file(path).decode('utf-8', 'replace')
    for m in INC.finditer(txt):
        cand = os.path.normpath(os.path.join(os.path.dirname(path), m.group(1)))
        if not os.path.exists(cand) and find:
            cand = find(os.path.basename(m.group(1))) or cand
        include_closure(cand, find, seen)
    return seen


def source_key(path, find=None):
    files = sorted(include_closure(os.path.normpath(path), find))
    return sha(*[os.path.basename(f) + ':' + sha(_file(f)) for f in files])


DEFAULT_OPT = '-O3'
_LEVELS = {}


def levels_of(hlsl):
    """R8: res/shaders/hlsl/optlevels.txt = `<stage file name>\\t<DXC level>` for the stages that stay below the default level
    (the ones whose optimised build failed A4); every other stage is built at DEFAULT_OPT. -> {file name: level}"""
    p = os.path.abspath(hlsl).replace('\\', '/')
    root = p.split('/res/shaders/hlsl/')[0] + '/res/shaders/hlsl/optlevels.txt' if '/res/shaders/hlsl/' in p else ''
    if root not in _LEVELS:
        _LEVELS[root] = {k: v[0] for k, v in load_table(root, 2).items()} if root else {}
    return _LEVELS[root]


def opt_of(hlsl):
    """the DXC optimisation flag of one stage file; env DXC_OPT overrides every stage (a diagnosis run)"""
    return os.environ.get('DXC_OPT') or levels_of(hlsl).get(os.path.basename(hlsl), DEFAULT_OPT)


def stage_key(hlsl, defines, toolkey):
    return sha(source_key(hlsl), ','.join(defines), opt_of(hlsl), dxc_key(), toolkey)


def read_key(spv):
    try:
        return open(spv + '.key').read().strip()
    except OSError:
        return ''


def write_key(spv, key):
    open(spv + '.key', 'w', newline='\n').write(key + '\n')


def load_table(path, ncol):
    """name -> list of the other columns, from a tab separated file (missing file: empty)"""
    out = {}
    if os.path.exists(path):
        for ln in open(path, encoding='utf-8'):
            c = ln.rstrip('\n').split('\t', ncol - 1)
            if len(c) == ncol:
                out[c[0]] = c[1:]
    return out
