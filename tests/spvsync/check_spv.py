# Lane RESTIRSPV1: every committed SPIR-V header (src/**/*.spv.h) must be what its generator script makes of its
# source TODAY. A .comp / .glsl edit without `bash tools/rtddgi_spirv.sh` (or tools/bakert_spirv.sh) leaves the exe
# running the OLD kernel against a CPU twin with the new rule (skill ww-rtddgi-volume, "a stale header").
#   python -I tests/spvsync/check_spv.py <tree> <scratch dir> [--glslang <exe>] [--committed <name>=<file>]...
# Rebuilds each header from its source into <scratch> with the same tool + flags the generator scripts use, and
# compares the bytes with the committed header (--committed swaps the file compared, for the red).
# rc 0 PASS (every header identical), 1 FAIL (a header differs, a header nobody generates, a script that drifted
# from this table), 77 SKIP (no glslangValidator, or a different glslang version than the headers were made with:
# the bytes then cannot be compared, which is not a pass).
import os, re, shutil, subprocess, sys

# (source dir, kernel, --target-env, --vn) == tools/bakert_spirv.sh + tools/rtddgi_spirv.sh
SPECS = [
	('src/bakert', 'rq', 'vulkan1.2', 'bakert_rq_spv'),
	('src/bakert', 'bvh', 'vulkan1.1', 'bakert_bvh_spv'),
	('src/bakert', 'rqx', 'vulkan1.2', 'bakert_rqx_spv'),
] + [('src/rtddgi', k, 'vulkan1.2', 'rtddgi_%s_spv' % k) for k in ('trace', 'blend', 'border', 'reloc', 'restir')]
DEFAULT_G = 'E:/Tools/RenderDoc/RenderDoc_1.45_64/plugins/spirv/glslangValidator.exe'


def script_ok(tree):
	"""The generator scripts still say what SPECS says (one table, checked against the scripts it mirrors)."""
	bad = []
	b = open(os.path.join(tree, 'tools/bakert_spirv.sh'), encoding='utf-8', errors='replace').read()
	r = open(os.path.join(tree, 'tools/rtddgi_spirv.sh'), encoding='utf-8', errors='replace').read()
	m = re.search(r'^for k in ([a-z ]+); do$', r, re.M)
	loop = m.group(1).split() if m else []
	if '"$G" --target-env vulkan1.2 -V --vn "rtddgi_${k}_spv" -o "${k}.spv.h" "${k}.comp"' not in r:
		bad.append('tools/rtddgi_spirv.sh: its compile line changed')
	for d, k, env, vn in SPECS:
		if d == 'src/bakert':
			if '"$G" --target-env %s -V --vn %s -o %s.spv.h %s.comp' % (env, vn, k, k) not in b:
				bad.append('tools/bakert_spirv.sh: no line for %s (%s)' % (k, env))
		elif k not in loop:
			bad.append('tools/rtddgi_spirv.sh: %s not in its kernel loop' % k)
	known = set(k for d, k, e, v in SPECS if d == 'src/rtddgi')
	for k in loop:
		if k not in known:
			bad.append('tools/rtddgi_spirv.sh: kernel %s is not in this checker' % k)
	return bad


def tool_stamp(g):
	out = subprocess.run([g, '--version'], capture_output=True, text=True, timeout=60).stdout
	m = re.search(r'Glslang Version:\s*(\d+):(\S+)', out)
	return (m.group(1) + m.group(2)) if m else None


def first_diff(a, b):
	wa = re.findall(rb'0x[0-9a-f]{8}', a)
	wb = re.findall(rb'0x[0-9a-f]{8}', b)
	for i in range(min(len(wa), len(wb))):
		if wa[i] != wb[i]:
			return 'words %d vs %d, first differing word %d' % (len(wa), len(wb), i)
	return 'words %d vs %d, common prefix equal' % (len(wa), len(wb))


def main(argv):
	if len(argv) < 3:
		print('usage: check_spv.py <tree> <scratch> [--glslang exe] [--committed name=file]...')
		return 2
	tree, scratch = argv[1], argv[2]
	g = None
	swap = {}
	i = 3
	while i < len(argv):
		if argv[i] == '--glslang':
			g = argv[i + 1]; i += 2
		elif argv[i] == '--committed':
			n, f = argv[i + 1].split('=', 1); swap[n] = f; i += 2
		else:
			print('unknown arg', argv[i]); return 2
	os.makedirs(scratch, exist_ok=True)
	fails, skips = [], []
	for s in script_ok(tree):
		fails.append(s)
	# every committed SPIR-V header has a generator line
	listed = set('%s/%s.spv.h' % (d, k) for d, k, e, v in SPECS)
	for root, dirs, files in os.walk(os.path.join(tree, 'src')):
		for f in files:
			if f.endswith('.spv.h'):
				rel = os.path.relpath(os.path.join(root, f), tree).replace('\\', '/')
				if rel not in listed:
					fails.append('%s: no generator in this checker (add it to SPECS and its tools/*_spirv.sh)' % rel)
	for n in listed:
		if not os.path.isfile(os.path.join(tree, n)):
			fails.append('%s: listed but not on disk' % n)
	for n in swap:
		if not any(n == '%s.spv.h' % k for d, k, e, v in SPECS):
			fails.append('--committed %s: no such header' % n)
	g = g or os.environ.get('GLSLANG') or (DEFAULT_G if os.path.isfile(DEFAULT_G) else None) \
		or shutil.which('glslangValidator')
	if not g or not os.path.isfile(g):
		skips.append('no glslangValidator (set GLSLANG=<exe>; RenderDoc ships one under plugins/spirv)')
		stamp = None
	else:
		stamp = tool_stamp(g)
		print('glslang %s stamp %s' % (g, stamp))
		if not stamp:
			skips.append('cannot read the glslang version of %s' % g)
	if stamp:
		for d, k, env, vn in SPECS:
			name = '%s.spv.h' % k
			committed = swap.get(name, os.path.join(tree, d, name))
			if not os.path.isfile(committed):
				continue
			c = open(committed, 'rb').read()
			cs = c.split(b'\n', 1)[0].strip().lstrip(b'/').strip().decode('ascii', 'replace')
			if cs != stamp:
				skips.append('%s: made by glslang %s, this machine has %s (bytes not comparable)' % (name, cs, stamp))
				continue
			out = os.path.join(scratch, d.replace('/', '_') + '_' + name)
			if os.path.exists(out):
				os.remove(out)
			r = subprocess.run([g, '--target-env', env, '-V', '--vn', vn, '-o', out, k + '.comp'],
			                   cwd=os.path.join(tree, d), capture_output=True, text=True, timeout=300)
			if r.returncode != 0 or not os.path.isfile(out):
				fails.append('%s: glslang rc %d: %s' % (name, r.returncode, (r.stdout + r.stderr).strip()[-300:]))
				continue
			a = open(out, 'rb').read()
			if a == c:
				print('OK    %s/%s %d B' % (d, name, len(c)))
			else:
				why = 'line endings only' if a.replace(b'\r\n', b'\n') == c.replace(b'\r\n', b'\n') else first_diff(a, c)
				fails.append('%s/%s: STALE, rebuilt %d B vs committed %d B (%s); run tools/%s_spirv.sh'
				             % (d, name, len(a), len(c), why, 'bakert' if d == 'src/bakert' else 'rtddgi'))
	for f in fails:
		print('FAIL  ' + f)
	for s in skips:
		print('SKIP  ' + s)
	if fails:
		print('SPVSYNC FAIL %d' % len(fails)); return 1
	if skips:
		print('SPVSYNC SKIP %d' % len(skips)); return 77
	print('SPVSYNC PASS %d' % len(SPECS)); return 0


if __name__ == '__main__':
	sys.exit(main(sys.argv))
