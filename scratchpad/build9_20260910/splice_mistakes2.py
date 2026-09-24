#!/usr/bin/env python3
"""Lane BUILD9, 2026-09-10 -- the two lanes whose Mistakes sections were never
spliced. HKX3's are already in the file (entry of 2026-09-10, "lane HKX3
(animation workspace), spliced from its report"); FILESTAB's and SKELOVERLAY's
were not. Their own words, condensed only where they repeated themselves.
Default --check, --apply writes.
"""
import sys

PATH = "MISTAKES.md"
AFTER = "Newest at the top.\n"
MARK = "## 2026-09-10 -- lane FILESTAB (the Files tab), spliced from its report"

BLOCK = """
## 2026-09-10 -- lane FILESTAB (the Files tab), spliced from its report

1. **A Bash heredoc halved the backslashes in the first inventory script**, so
   `[^"\\\\]` arrived as `[^"\\]` and Python refused with "unterminated character
   set". This trap has been paid for repeatedly in this repo in two days --
   `nifskope-ww-build-verify`, `nifskope-ww-resume-pending` and
   `ww-anchored-hookup` all name it -- and it was walked into anyway on a script
   that "was only a grep". Then it was walked into a SECOND time in the same
   lane, appending the report through `cat >> file << EOF`, which died with
   "unexpected EOF while looking for matching quote" and wrote nothing. The rule,
   restated so it has no exception: **every script and every multi-line text goes
   through the Write tool, whatever it is for.**
2. **Two anchors were declared unique that were not.**
   `const int source = nameIndex.data( NifBrowserSourceRole ).toInt();` occurs in
   three functions, and `"Use as Skeleton for Loaded NIFs"` occurs three times,
   not two, because a comment quotes the menu item by name. Both were caught by
   `--check` printing the COUNT rather than "ok", which is why
   `ww-anchored-hookup` says to print counts -- so the cost was two minutes, not
   a build. The comment was renamed with the label it quotes: a comment naming a
   menu item that can no longer be found by that name is worse than no comment.

## 2026-09-10 -- lane SKELOVERLAY (Overlays > Show Skeleton), spliced from its report

1. **`NifSkope::ogl` copied as if it were public.** The harness was written by
   following `WW_SKELETON_TEST`, which reads `skope->ogl` -- legally, because it
   is code INSIDE `NifSkope`. Lifting the same line into a separate translation
   unit does not lift the access. Found by the `-fsyntax-only` pass, which is
   exactly the failure class that pass is for. The rule: when copying a pattern
   out of a class's own file into a new one, check every member it touches is
   public first. (Lane BUILD9 hit the same class of error the same day writing
   `src/uialigntest.cpp`: `setLeftColumnMode()` and its `LeftColumnMode` enum are
   private too. It drives the tab bar instead, which is also the better gate.)
2. **`Scene::getNode()` used at first for a read-only overlay.** It CONSTRUCTS a
   Node for any block handed to it, and this overlay is handed every
   `NiAVObject` block in the file. On a file with a block the scene graph does
   not reach, the "read-only" overlay would have grown `nodes` and moved
   `Scene::bounds()`. Caught by reading `getNode`'s body before trusting its
   name; fixed by adding `Scene::findNode()`. The rule: an accessor named `get`
   is not evidence that it only gets.
3. **`sx_tmp.sh` at the repo root was already there**, owned by another live
   lane. Writing to it would have destroyed that lane's working file mid-run.
   The fix is in `nifskope-ww-build-verify` in both trees: the throwaway syntax
   script is `sx_$LANE.sh`, never a fixed name.

"""


def main():
    apply = "--apply" in sys.argv
    raw = open(PATH, "rb").read()
    cr0, lf0, n0 = raw.count(b"\r"), raw.count(b"\n"), len(raw)
    text = raw.decode("utf-8")
    if cr0 != 0:
        print("REFUSED: expected LF-only; %d CR." % cr0)
        return 1
    if text.count(AFTER) != 1:
        print("REFUSED: header line not found exactly once.")
        return 1
    if MARK in text:
        print("REFUSED: already present.")
        return 1
    at = text.index(AFTER) + len(AFTER)
    out = (text[:at] + BLOCK + text[at:]).encode("utf-8")
    print("%s bytes %d -> %d   CR %d -> %d   LF %d -> %d"
          % (PATH, n0, len(out), cr0, out.count(b"\r"), lf0, out.count(b"\n")))
    if out.count(b"\r") != 0:
        print("REFUSED: a CR appeared.")
        return 1
    if not apply:
        print("--check: clean. Nothing written.")
        return 0
    open(PATH, "wb").write(out)
    print("--apply: written.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
