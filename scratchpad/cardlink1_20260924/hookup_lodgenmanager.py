"""CARDLINK1 hook-up for the GUI front end (src/lodgenmanager.cpp is not this lane's file).

Run from the tree it should patch:  python hookup_lodgenmanager.py <repo root>
Refuses (exit 1, nothing written) unless the anchor is there exactly once and
the file's CR count is unchanged by the edit.

What it does: right after the card arrays succeed, hand them to the emitter
(`lodgenNativeLinkCards`). It must be HERE and not beside `lodgenNativeWrite`,
because the GUI's scratch teardown clears `writtenBto` and deletes the manifests
between the two. A link refusal disarms the emitter, so no pair is written
rather than a pair without its cards, and the reason is in the summary tail.
"""
import sys
root = sys.argv[1] if len(sys.argv) > 1 else '.'
path = root.rstrip('/\\') + '/src/lodgenmanager.cpp'
with open(path, 'rb') as f:
    b = f.read()
cr0 = b.count(b'\r')
anchor = (b'\t\t\t\t\ttail += tr( ", card arrays: %1" ).arg( rep );\n'
          b'\t\t\t\t\tif ( wantNative() )\n'
          b'\t\t\t\t\t\tlodgenNoteLayoutDir( arrDir );\n')
if b'lodgenNativeLinkCards' in b:
    sys.exit('refused: %s already calls lodgenNativeLinkCards' % path)
if b.count(anchor) != 1:
    sys.exit('refused: anchor found %d times in %s' % (b.count(anchor), path))
add = (b'\t\t\t\t\t/* CARDLINK1: the emitter links the arrays just written, while the\n'
       b'\t\t\t\t\t * manifests still sit beside the chunks (the teardown below\n'
       b'\t\t\t\t\t * removes them before the pair is written). */\n'
       b'\t\t\t\t\tif ( lodgenNativeActive() ) {\n'
       b'\t\t\t\t\t\tQString lerr;\n'
       b'\t\t\t\t\t\tif ( !lodgenNativeLinkCards( writtenBto,\n'
       b'\t\t\t\t\t\t\tarrDir + "/" + ws + QStringLiteral( ".LodgenCards" ), &lerr ) ) {\n'
       b'\t\t\t\t\t\t\ttail += tr( ", native: not written, %1" ).arg( lerr );\n'
       b'\t\t\t\t\t\t\tlodgenNativeEnd();\n'
       b'\t\t\t\t\t\t}\n'
       b'\t\t\t\t\t}\n')
b = b.replace(anchor, anchor + add)
if b.count(b'\r') != cr0:
    sys.exit('refused: CR count would move')
with open(path, 'wb') as f:
    f.write(b)
print('hooked: lodgenNativeLinkCards after the GUI card arrays in', path)
