"""Lane UI4 -- the two edits src/nifskope_ui.cpp needs, as a REFUSING script.

The lane owns res/style.qss, src/wateruitest.cpp, tests/spells/water_ui.sh and
src/wwskin.h's declarations; the QSS helpers themselves live in
src/nifskope_ui.cpp, which is another lane's file, so every byte that goes into
it goes through here (skill ww-anchored-hookup).

  E1  replace the segmented-strip sheet block (wwSegmentedQss +
      wwSegmentedToolButtonQss + wwSegmentedTabBarQss) with the version that
      gives the strip bungo's 4 px of air, and add wwSegmentedStripAir().
  E2  replace the ONE call site that restyles the strip for the bar row, so it
      passes the window the separator metric has to be asked with.

Both anchors and both replacements are FILES, read as bytes:
  anchor_seg_old.txt / anchor_seg_new.txt
  anchor_call_old.txt / anchor_call_new.txt
The old ones were cut out of the file itself by extract_anchors.py, so an
anchor is the file's bytes and not a transcription of them.

  python scratchpad/ui4_20260910/hookup.py            # --check, writes nothing
  python scratchpad/ui4_20260910/hookup.py --apply

APPLIED OR NOT is decided by the MARKER, never by the anchor: --check on an
applied file reports the anchor missing AND the marker present, which is the
BUILD5b trap this script's contract exists for.
"""
import sys
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HERE = os.path.join(ROOT, "scratchpad", "ui4_20260910")
TARGET = os.path.join(ROOT, "src", "nifskope_ui.cpp")

# a string the replacement carries and the original cannot
MARKER = b"wwSegmentedStripAir"

EDITS = [
    ("E1 the segmented strip sheet", "anchor_seg_old.txt", "anchor_seg_new.txt"),
    ("E2 the bar-row call site", "anchor_call_old.txt", "anchor_call_new.txt"),
]


def load(name):
    with open(os.path.join(HERE, name), "rb") as f:
        return f.read()


def main():
    apply_it = "--apply" in sys.argv
    with open(TARGET, "rb") as f:
        data = f.read()
    cr_before = data.count(b"\r")
    print("src/nifskope_ui.cpp: %d bytes, CR %d, marker present %d"
          % (len(data), cr_before, data.count(MARKER)))

    ok = True
    new = data
    delta = 0
    for label, oldf, newf in EDITS:
        old = load(oldf)
        rep = load(newf)
        n = new.count(old)
        print("  %-30s anchor %5d bytes, CR %d, occurrences %d -> replacement "
              "%5d bytes, CR %d" % (label, len(old), old.count(b"\r"), n,
                                    len(rep), rep.count(b"\r")))
        if n != 1:
            ok = False
            continue
        if old.count(b"\r") != rep.count(b"\r"):
            print("    REFUSED: the replacement's line endings differ from the "
                  "anchor's (%d CR vs %d)" % (rep.count(b"\r"), old.count(b"\r")))
            ok = False
            continue
        new = new.replace(old, rep)
        delta += len(rep) - len(old)

    if not ok:
        print("REFUSED: %d of %d anchors did not match exactly once (or the CR "
              "counts differ). Nothing written." % (
                  sum(1 for l, o, r in EDITS if new.count(load(o)) != 1), len(EDITS)))
        return 1

    print("all %d anchors match once; CR before %d, after %d; size %d -> %d "
          "(delta %+d)" % (len(EDITS), cr_before, new.count(b"\r"), len(data),
                           len(new), delta))
    if new.count(b"\r") != cr_before:
        print("REFUSED: the CR count moved. Nothing written.")
        return 1
    # counted from the two texts, never guessed: anchor_seg_new.txt names it
    # once as a definition and once as the call inside wwSegmentedQss.
    want = load("anchor_seg_new.txt").count(MARKER)
    if want < 2 or new.count(MARKER) != want:
        print("REFUSED: the result carries the marker %r %d times, the "
              "replacement text says %d." % (MARKER, new.count(MARKER), want))
        return 1

    if not apply_it:
        print("--check only: nothing written. Re-run with --apply.")
        return 0

    with open(TARGET, "wb") as f:
        f.write(new)
    print("APPLIED: src/nifskope_ui.cpp is now %d bytes, CR %d, marker %d"
          % (len(new), new.count(b"\r"), new.count(MARKER)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
