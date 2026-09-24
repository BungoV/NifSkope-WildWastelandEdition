#!/usr/bin/env python
"""Lane SKELFIX, part 2: the rule text exists the moment the overlay goes on.

`refreshSkeletonOverlay()` is lazy -- it runs inside the next draw -- so
`skeletonOverlayRule()` would be empty at the instant the Overlays entry is
ticked, which is exactly when its tooltip wants it. Build the list (and with it
the armature and the sentence) on the way in, and leave the dirty flag set so
anything that renumbers blocks still forces a rebuild.

src/glview.cpp is CRLF: spliced in binary.
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
PATH = os.path.join(ROOT, "src", "glview.cpp")

OLD = """		skelOverlayDirty = true;
		skelOverlayCensus = SkeletonOverlayCensus();
	} else {
"""
NEW = """		skelOverlayDirty = true;
		skelOverlayCensus = SkeletonOverlayCensus();
		/* ...and build it NOW as well (lane SKELFIX). The rebuild is otherwise
		 * lazy, inside the next draw, so `skeletonOverlayRule()` -- the sentence
		 * the Overlays entry puts in its tooltip -- would be empty at the exact
		 * moment the entry is ticked. The dirty flag stays set, so anything that
		 * renumbers blocks still forces the rebuild it always did.
		 */
		if ( model && scene )
			refreshSkeletonOverlay();
		skelOverlayDirty = true;
	} else {
"""


def main(argv):
    blob = open(PATH, "rb").read()
    o = OLD.replace("\n", "\r\n").encode()
    n = NEW.replace("\n", "\r\n").encode()
    if n in blob:
        print("already applied")
        return 0
    if blob.count(o) != 1:
        print("anchor matches %d times, refusing" % blob.count(o))
        return 2
    out = blob.replace(o, n)
    print("src/glview.cpp %+d bytes, CR %d -> %d, LF %d -> %d"
          % (len(out) - len(blob), blob.count(b"\r"), out.count(b"\r"),
             blob.count(b"\n"), out.count(b"\n")))
    if "--apply" in argv:
        open(PATH, "wb").write(out)
        print("WRITTEN")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
