#!/usr/bin/env python3
"""HARNESSWIN2 -- the two repairs, as a REFUSING anchored hook-up.

    python scratchpad/harnesswin2_20260919/hookup.py            # --check (writes nothing)
    python scratchpad/harnesswin2_20260919/hookup.py --apply     # writes, all or nothing

WHAT IT REPAIRS
---------------
(a) src/nifskope_ui.cpp, the WW_RENDER_SHOT grab lambda.

    The lambda resizes the window and THEN hides the docks:

        skope->showNormal();
        skope->resize( rw, rh );          <-- docks still up here
        for ( QDockWidget * dw : ... ) dw->hide();
        if ( skope->viewportHeader ) skope->viewportHeader->hide();
        qApp->processEvents();

    Qt will not make a window narrower than its layout's minimum, and with the
    docks visible that minimum is wider than most requests (1822 px on this
    machine with the LOD docks up).  So resize( 1024, 1024 ) lands at
    1822x1024, the docks come down afterwards, the window is never resized
    again, and the viewport ends at 1822x989 -- `upp` 8.99 where the spell
    asked for 16.0.  That is tests/spells/native_open.sh row (c) at 0.8978,
    measured by lane HARNESSWIN1 (scratchpad/harnesswin1_20260919/PENDING.md
    section 3).  The maximized persisted geometry that lane was briefed to fix
    was real, was fixed, and was NOT this.

    The repair: bring the docks and the viewport header down BEFORE the
    resize, and let the layout settle, so the minimum the resize is measured
    against is the minimum of the window the shot is actually taken from.
    Nothing is deleted: the existing hide loop below stays exactly where it is
    and runs a second time, which costs nothing and keeps the edit additive.

(b) src/nifskope.cpp -- the third settings writer, unguarded.

    `saveUi()` has refused to persist from a WW_* run since 2026-07-27 and
    lane HARNESSWIN1 gave `restoreUi()` the same refusal.  `setCurrentFile()`
    and `clearCurrentFile()` write `File/Recent File List` from OUTSIDE
    `saveUi()`, so neither guard covers them: every harness run that opens a
    file rewrites bungo's recent-file list, and has done for as long as the
    harnesses have existed.  It was found because it made
    tests/spells/harness_window.sh row (c) pass at 14:47 and fail at 14:58 on
    the same two binaries -- the export only differs when the list ORDER
    changes.

    The predicate is `NifSkope::wwHeadlessRun()` (src/nifskope.cpp:7594), the
    same one `restoreUi()` uses and the cached form of the environment walk
    `saveUi()` writes out by hand.  Both call sites are NifSkope members, so
    no include and no new symbol is needed.

NEITHER IS A TOGGLE.  There is no new environment key, no INI key and no menu
row: an ordinary session reaches (a) only when WW_RENDER_SHOT is set, and (b)
only when some WW_* variable is set, which is the definition of a harness run.

WHAT IT DELIBERATELY DOES NOT TOUCH
-----------------------------------
* src/nifskope_ui.cpp ~22386, the WW_IMPOSTOR_BAKE lambda, has the identical
  resize-before-hide order with resize( 560, 560 ).  It is floored today by
  the same mechanism.  It is NOT edited here: lane IMPOSTORFIX3 owns the
  impostor gates, and changing the bake framebuffer under it would move its
  numbers mid-lane.  Named in PENDING.md as CHANGE_NEEDED 5.
* src/nifskope.cpp:8273 writes `File/Recent Archive Files`, a DIFFERENT key,
  from setCurrentArchiveFile().  Same defect, same one-line repair; not in
  this table because the brief named `File/Recent File List`.  Named in
  PENDING.md as CHANGE_NEEDED 4.

HOW THE ANCHORS ARE BUILT
-------------------------
No tab, no line ending and no indent is typed in this file.  Each edit names a
LOCATOR: a short ASCII substring with no whitespace runs and no backslash.
The script finds it, asserts it occurs exactly the stated number of times,
takes the WHOLE PHYSICAL LINE it sits on out of the file's real bytes -- with
that line's own leading tabs and its own CRLF or LF -- and builds the new text
from those.  src/nifskope_ui.cpp is LF-only; src/nifskope.cpp is MIXED (CR
10899 / LF 10904) and the regions edited here are CRLF, which is why the
ending is read rather than chosen.  The CR and LF deltas are asserted against
the deltas of the inserted text, per CONSTITUTION rule 8.

THE ALREADY-APPLIED MARKER IS NOT AN ANCHOR.  `--check` reports APPLIED when
it finds the string HARNESSWIN2 in a target file -- a string that exists
nowhere in the tree today and that only these edits create.  It is never a
line the edits merely repeat, so a green anchor count can never be mistaken
for an applied edit (MISTAKES.md, lane BUILD5b).
"""

import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MARKER = "HARNESSWIN2"
TAB = chr(9)


# ---------------------------------------------------------------------------
# the table
# ---------------------------------------------------------------------------
# kind "before": the generated lines go IMMEDIATELY BEFORE the located line.
# kind "guard" : the located line is re-emitted one tab deeper under an
#                `if ( !NifSkope::wwHeadlessRun() )`, with the comment above.
#
# "count" is the number of times the locator must appear in the whole file.
# "after" (optional) names an earlier unique locator; the target is then the
# FIRST occurrence of "locator" at or after it, which is how two identical
# lines in two different functions are told apart.

EDITS = [
	dict(
		path="src/nifskope_ui.cpp",
		kind="before",
		locator="skope->resize( rw, rh );",
		count=1,
		lines=[
			(0, "/* THE DOCKS COME DOWN BEFORE THE RESIZE (lane HARNESSWIN2, 2026-09-19)."),
			(0, " *"),
			(0, " * Qt will not make a window narrower than its layout's minimum, and"),
			(0, " * the docks are part of that layout until they are hidden. Hiding"),
			(0, " * them AFTER the resize -- which is what this did -- meant the"),
			(0, " * resize below was measured against a minimum that the shot itself"),
			(0, " * then removed, and the window was never resized again: a request"),
			(0, " * for 1024x1024 came out 1822x1024 and the viewport 1822x989, so"),
			(0, " * `upp` read 8.99 where the spell asked for 16.0. That is"),
			(0, " * tests/spells/native_open.sh row (c) at covered 0.8978, and it is"),
			(0, " * NOT the maximized persisted geometry it was blamed on -- that was"),
			(0, " * a second, real defect, repaired separately in src/harnesswindow.cpp."),
			(0, " * The two were indistinguishable from outside because a maximized"),
			(0, " * window and the dock minimum both land on 1822 px on a 1920-wide"),
			(0, " * screen (lane HARNESSWIN1, 2026-09-19, measured row (a) against row"),
			(0, " * (d) on one binary: the same request obtained exactly with a small"),
			(0, " * fixture, floored with a .lodi scene whose docks raise the minimum)."),
			(0, " *"),
			(0, " * processEvents() is needed between the hide and the resize because"),
			(0, " * the layout minimum is recomputed lazily; without it the resize is"),
			(0, " * still measured against the docks that are already hidden. It is"),
			(0, " * safe here and only here: this lambda runs only when WW_RENDER_SHOT"),
			(0, " * is set, so it is always a harness run, never a restore path (see"),
			(0, " * the both-maximized crash table in NifSkope::restoreUi())."),
			(0, " *"),
			(0, " * The hide loop further down is left exactly where it is and runs a"),
			(0, " * second time. Hiding a hidden dock costs nothing, and leaving it"),
			(0, " * there keeps this edit purely additive."),
			(0, " */"),
			(0, "for ( QDockWidget * dw : skope->findChildren<QDockWidget *>() )"),
			(1, "dw->hide();"),
			(0, "if ( skope->viewportHeader )"),
			(1, "skope->viewportHeader->hide();"),
			(0, "qApp->processEvents();"),
		],
	),
	dict(
		path="src/nifskope.cpp",
		kind="guard",
		after="::updateRecentFiles( files, currentFile );",
		locator='settings.setValue( "File/Recent File List", files );',
		count=2,
		lines=[
			(0, "// A HARNESS RUN DOES NOT TOUCH HIS RECENT FILES (lane HARNESSWIN2,"),
			(0, "// 2026-09-19). saveUi() has refused to persist from a WW_* run since"),
			(0, "// 2026-07-27 and restoreUi() was given the same refusal on 2026-09-19,"),
			(0, "// but this writer sits outside both, so every gate that opens a .nif"),
			(0, "// rewrote the user's recent-file list. Found by tests/spells/"),
			(0, "// harness_window.sh row (c), which passed at 14:47 and failed at 14:58"),
			(0, "// on the same two binaries: the exported key only differs when the"),
			(0, "// list ORDER changes, so the damage was real and intermittent at once."),
			(0, "// The READ above is left alone -- the menu still shows his files."),
		],
	),
	dict(
		path="src/nifskope.cpp",
		kind="guard",
		after="files.removeAll( currentFile );",
		locator='settings.setValue( "File/Recent File List", files );',
		count=2,
		lines=[
			(0, "// The same writer in its other spelling (lane HARNESSWIN2, 2026-09-19):"),
			(0, "// clearCurrentFile() REMOVES an entry, and a harness run that closes a"),
			(0, "// document reaches it. Guarding only setCurrentFile() would have left"),
			(0, "// the list still being edited, in the other direction."),
		],
	),
]


# ---------------------------------------------------------------------------
def line_of(raw, at):
	"""The whole physical line containing byte offset `at`, with its own EOL."""
	start = raw.rfind(b"\n", 0, at) + 1
	nl = raw.find(b"\n", at)
	if nl < 0:
		return start, len(raw)
	return start, nl + 1


def split_line(blob):
	"""-> (indent bytes, body bytes, eol bytes) of one physical line."""
	eol = b""
	body = blob
	if body.endswith(b"\n"):
		eol = b"\n"
		body = body[:-1]
		if body.endswith(b"\r"):
			eol = b"\r\n"
			body = body[:-1]
	indent = body[: len(body) - len(body.lstrip(b"\t"))]
	return indent, body, eol


def build(edit, raw):
	"""-> (start, end, replacement bytes, report string) or refuses."""
	loc = edit["locator"].encode("utf-8")
	n = raw.count(loc)
	if n != edit["count"]:
		return None, "REFUSED %s: locator %r occurs %d times, the table says %d" % (
			edit["path"], edit["locator"], n, edit["count"])

	at = -1
	if edit.get("after"):
		aft = edit["after"].encode("utf-8")
		if raw.count(aft) != 1:
			return None, "REFUSED %s: the 'after' locator %r occurs %d times, not once" % (
				edit["path"], edit["after"], raw.count(aft))
		at = raw.find(loc, raw.find(aft))
	else:
		at = raw.find(loc)
	if at < 0:
		return None, "REFUSED %s: locator not found after its 'after'" % edit["path"]

	start, end = line_of(raw, at)
	indent, body, eol = split_line(raw[start:end])
	if not eol:
		return None, "REFUSED %s: the located line has no line ending" % edit["path"]

	made = []
	for extra, text in edit["lines"]:
		made.append(indent + TAB.encode("utf-8") * extra + text.encode("utf-8") + eol)

	if edit["kind"] == "before":
		new = b"".join(made) + raw[start:end]
	elif edit["kind"] == "guard":
		gate = indent + b"if ( !NifSkope::wwHeadlessRun() )" + eol
		new = b"".join(made) + gate + indent + TAB.encode("utf-8") + body.lstrip(b"\t") + eol
	else:
		return None, "REFUSED: unknown kind %r" % edit["kind"]

	report = "  %-22s %-7s line %d  eol=%s indent=%d tabs  %r" % (
		edit["path"], edit["kind"], raw[:start].count(b"\n") + 1,
		"CRLF" if eol == b"\r\n" else "LF", len(indent), body.strip()[:56].decode("utf-8"))
	return (start, end, new), report


def main():
	apply = "--apply" in sys.argv
	paths = []
	for e in EDITS:
		if e["path"] not in paths:
			paths.append(e["path"])

	applied = []
	for p in paths:
		raw = io.open(os.path.join(ROOT, p), "rb").read()
		if MARKER.encode("utf-8") in raw:
			applied.append(p)
	if applied:
		print("ALREADY APPLIED: %s carries the marker %s. Nothing written."
			% (", ".join(applied), MARKER))
		if len(applied) != len(paths):
			print("REFUSED: it is in %d of %d files -- a half-applied hook-up. "
				"Look before re-running." % (len(applied), len(paths)))
			return 1
		return 0

	planned = {}
	ok = True
	for e in EDITS:
		full = os.path.join(ROOT, e["path"])
		raw = io.open(full, "rb").read()
		got, report = build(e, raw)
		print(report)
		if got is None:
			ok = False
			continue
		planned.setdefault(e["path"], []).append(got)
	if not ok:
		print("REFUSED: at least one anchor did not match. Nothing written.")
		return 1

	print()
	for p, edits in planned.items():
		full = os.path.join(ROOT, p)
		raw = io.open(full, "rb").read()
		cr0, lf0, n0 = raw.count(b"\r"), raw.count(b"\n"), len(raw)
		new = raw
		added_cr = added_lf = 0
		for start, end, text in sorted(edits, key=lambda t: -t[0]):
			added_cr += text.count(b"\r") - raw[start:end].count(b"\r")
			added_lf += text.count(b"\n") - raw[start:end].count(b"\n")
			new = new[:start] + text + new[end:]
		cr1, lf1, n1 = new.count(b"\r"), new.count(b"\n"), len(new)
		print("%-22s %8d -> %-8d bytes   CR %+4d   LF %+4d   (expected CR %+d LF %+d)"
			% (p, n0, n1, cr1 - cr0, lf1 - lf0, added_cr, added_lf))
		if (cr1 - cr0) != added_cr or (lf1 - lf0) != added_lf:
			print("REFUSED: the line-ending delta is not the inserted text's. Nothing written.")
			return 1
		if MARKER.encode("utf-8") not in new:
			print("REFUSED: the result does not carry the marker -- nothing to detect later.")
			return 1
		planned[p] = new

	if not apply:
		print("\n--check only. Nothing written. Re-run with --apply.")
		return 0
	for p, new in planned.items():
		io.open(os.path.join(ROOT, p), "wb").write(new)
		print("wrote %s" % p)
	return 0


if __name__ == "__main__":
	sys.exit(main())
