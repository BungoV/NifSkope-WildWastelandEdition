# Lane VKFLIGHT6 (2026-11-16): the obscurance pass's g-buffer is the Vulkan view's own (shared colour beside the shared
# depth), so its draws reach the object twins (objBlFrameRead takes shared targets only). A site gate on ao.cpp: the share
# is made before aoAlloc, follows the view (red key), drops on a size change or a lost device, and attaches the shared name.
# Usage: python -I aogbufsite.py <tree> [red]   red = the gbufShare call stripped in memory: must FAIL
import re, sys
tree = sys.argv[1]
red = len(sys.argv) > 2 and sys.argv[2] == 'red'
src = open(tree + '/src/features/ao/ao.cpp', encoding='utf-8', errors='replace').read().replace('\r\n', '\n')
if red:
	src = src.replace('\tgbufShare( fn, g, W, H, depthTex );', '', 1)
rows = 0
fails = 0
def check(ok, what):
	global rows, fails
	rows += 1
	if not ok:
		fails += 1
	print(('ok   ' if ok else 'FAIL ') + what)
m = re.search(r'\nbool gbufShare\( [^)]*\)\n\{\n(.*?)\n\}\n', src, re.S)
body = m.group(1) if m else ''
check(m is not None, 'gbufShare is defined (%d lines)' % body.count('\n'))
check('shareWanted( "WW_VKVIEW_AOGBUF_RED" )' in body, 'it follows the view, red key WW_VKVIEW_AOGBUF_RED')
check(re.search(r'want = depthTex &&', body) is not None, 'it shares the colour only while the depth is shared (one frame of shared targets)')
check(re.search(r'sharedTarget\( key, 0, W, H, 1, GL_RGBA32F, t, why \)', body) is not None, 'the shared texture is RGBA32F, 2D, one sample (as aoAlloc\'s)')
check(re.search(r'!WwVkPresent::sharedTargetIs\( key, g\.gbuf\.tex \) \|\| g\.gbuf\.w != W \|\| g\.gbuf\.h != H', body) is not None,
	'it drops on a lost device or a size change')
check(body.count('sharedTargetDrop( key )') == 2, 'sharedTargetDrop on the drop and on a refusal (%d)' % body.count('sharedTargetDrop( key )'))
check(re.search(r'glFramebufferTexture2D\( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0 \)', body) is not None,
	'the shared name is attached to the g-buffer FBO')
check('g.gbufNoW = W;' in body, 'a refused size is remembered (no retry every frame)')
check('vk aogbuf:' in body, 'a refusal is named in the log (vk aogbuf:)')
call = src.find('\tgbufShare( fn, g, W, H, depthTex );')
alloc = src.find('\taoAlloc( fn, g.gbuf, W, H, GL_RGBA32F, GL_RGBA, false );')
dshare = src.find('const bool depthTex = depthShare( g, W, H, gbufFmt );')
check(call > 0 and dshare > 0 and alloc > 0 and dshare < call < alloc, 'called after depthShare and before aoAlloc of the g-buffer (%d < %d < %d)' % (dshare, call, alloc))
check(src.count('gbufShare(') == 2, 'one definition, one call (%d)' % src.count('gbufShare('))
print('AOGBUFSITE %s %d rows, %d failed' % ('OK' if fails == 0 else 'FAIL', rows, fails))
sys.exit(0 if fails == 0 else 1)
