"""Make the whole-region refusal REACHABLE, which it was not.

`--atlas` is not on the switch-digest skip list -- correctly, because it changes
the output. So running `--incremental` with `--atlas` against a ledger written
WITHOUT `--atlas` refuses with `the switches differ`, and the whole-region
refusal is never reached. The arm was measuring the switch digest twice and the
whole-region check not at all.

The reachable path is the one a person would actually walk: bake the region once
WITH `--atlas` (which writes a ledger carrying that digest), then run the same
command again with `--incremental`. Now the digests agree and the only thing
standing between the run and a quarter-sized atlas is the refusal under test.

This is the second time in one gate that an arm passed or failed for a reason it
had not asked about. Both were caught by the same thing: printing the refusal
sentence beside every verdict instead of only the pass/fail.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/b2_refusals.sh'

OLD = """# 4. a whole-region product is asked for.
arm whole-region "build ONE region-wide product" "$GOOD" "$REG" --atlas"""

NEW = """# 4. a whole-region product is asked for.
#
#    THE SETUP IS THE POINT. --atlas is not on the switch-digest skip list, so
#    against $GOOD (baked without it) this arm would refuse with "the switches
#    differ" and never reach the check it exists to test. So bake the region
#    ONCE with --atlas first -- that writes a ledger carrying the --atlas digest
#    -- and only then ask for an incremental run of the same command.
ATL="$W/_atlasledger"
if [ ! -f "$ATL/obj/Commonwealth.lodb" ]; then
	rm -rf "$ATL"; mkdir -p "$ATL/obj" "$ATL/tex"
	echo "   (baking a ledger WITH --atlas so the whole-region arm is reachable)" | tee -a "$LOG"
	# shellcheck disable=SC2086
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REG --dim 4 \\
		--out-dir "$ATL/obj" --tex-dir "$ATL/tex" --data-root "$DATA" \\
		--cover --roads --road-detail 1 --atlas \\
		> "$ATL/bake.log" 2>&1
	[ -f "$ATL/obj/Commonwealth.lodb" ] || {
		echo "whole-region    SKIPPED -- the --atlas base bake failed, see $ATL/bake.log" \\
			| tee -a "$LOG"; }
fi
arm whole-region "build ONE region-wide product" "$ATL/obj" "$REG" --atlas"""


def main():
    b = open(P, 'rb').read()
    s = b.decode('utf-8')
    n = s.count(OLD)
    assert n == 1, 'arm 4 matched %d times' % n
    s = s.replace(OLD, NEW)
    out = s.encode('utf-8')
    open(P, 'wb').write(out)
    print('b2_refusals.sh %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
