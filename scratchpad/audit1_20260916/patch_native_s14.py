"""AUDIT1 step 6: section 14 of tests/spells/lodgen_native.sh -- the two rules
of the C++ reader that nothing was asking.

  C1  src/lodifile.cpp:959, src/lodofile.cpp:1803
      `t.off + t.bytes > h.fileBytes` is two quint64 added before the compare.
      An offset 4,096 short of 2^64 wraps the sum below fileBytes, passes, and
      the pad walk three lines down then indexes p[i] from the previous payload
      up to that offset.
  C2  src/lodifile.cpp:888, :1160
      every aggregate rule -- two azimuths, a positive switch distance, a band
      above 1, the identity bit, the cell order, the coveredFirst partition, the
      index range, the double cover -- is behind `version == 4`, and a bake with
      placement AO (the default) writes version 5.

Section 14 doctors a COPY of the real pair, one thing at a time, re-signs the
checksums (tests/spells/lodgen_native_doctor.py), and requires
`lodgen --native-verify` to REFUSE BY NAME. It also carries its own floor: the
undoctored copy must be ACCEPTED, or a reader that refused everything would
read as a pass.
"""
import io
import os
import sys
import tempfile

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_native.sh'
T = chr(9)

ANCHOR = '''echo
echo "$checks checks, $fails failures"'''

SECTION = '''echo "== 14. the C++ reader's own rules: the payload bounds and the aggregates"
# WHY THIS IS NOT leg 3. Leg 3 runs lodgen_native_mutate.py, and that tool asks
# the INDEPENDENT PYTHON DECODER to refuse. These four cases are aimed at rules
# that only src/lodifile.cpp and src/lodofile.cpp enforce, so the thing that has
# to answer is `lodgen --native-verify` -- the product's own reader.
mkdir -p "$W/doctor"
cp "$(pairdir "$W/native/Native")/Commonwealth.lodo" "$W/doctor/clean.lodo"
cp "$(pairdir "$W/native/Native")/Commonwealth.lodi" "$W/doctor/clean.lodi"
# THE FLOOR, first: the untouched copy is ACCEPTED. Without it every line below
# is also passed by a reader that refuses its own output.
if run "$NS" -no-gui lodgen "$ESM" --worldspace 3C \\
''' + T + '''--native-verify "$WA/doctor/clean.lodo" "$WA/doctor/clean.lodi"; then
''' + T + '''note "(14 floor) the undoctored copy of the real pair is accepted"
else
''' + T + '''bad "(14 floor) the undoctored copy of the real pair is accepted"
''' + T + '''grep -i "REFUSED" "$W/last.log" | head -2
fi

# doctorCase <case> <expected substring of the refusal>
doctorCase () {
''' + T + '''local case="$1" want="$2"
''' + T + '''cp "$W/doctor/clean.lodo" "$W/doctor/$case.lodo"
''' + T + '''cp "$W/doctor/clean.lodi" "$W/doctor/$case.lodi"
''' + T + '''local said rc
''' + T + '''said="$("$PY" "$ROOT/tests/spells/lodgen_native_doctor.py" "$case" \\
''' + T + T + '''"$W/doctor/$case.lodo" "$W/doctor/$case.lodi" 2>&1)"
''' + T + '''rc=$?
''' + T + '''echo "    $said"
''' + T + '''if [ $rc -eq 3 ]; then
''' + T + T + '''echo "  SKIP ($case) the bake left nothing to doctor"
''' + T + T + '''return 0
''' + T + '''elif [ $rc -ne 0 ]; then
''' + T + T + '''bad "($case) the doctor could not write the case"
''' + T + T + '''return 0
''' + T + '''fi
''' + T + '''if run "$NS" -no-gui lodgen "$ESM" --worldspace 3C \\
''' + T + T + '''--native-verify "$WA/doctor/$case.lodo" "$WA/doctor/$case.lodi"; then
''' + T + T + '''bad "($case) the reader ACCEPTED it; it must refuse, naming \\"$want\\""
''' + T + '''elif grep -qai -- "$want" "$W/last.log"; then
''' + T + T + '''note "($case) refused by name: $(grep -ai "REFUSED" "$W/last.log" | head -1 | cut -c1-160)"
''' + T + '''else
''' + T + T + '''bad "($case) refused, but the message never says \\"$want\\""
''' + T + T + '''grep -ai "REFUSED" "$W/last.log" | head -2
''' + T + '''fi
}

doctorCase lodi-wrap "runs past the file"
doctorCase lodo-wrap "runs past the file"

# THE AGGREGATES need a file that HAS one, and `--aggregate` is off by default
# on the command line (src/nifcli.cpp:7352). Placement AO is ON by default, so
# this bake is a version-5 file carrying aggregates -- exactly the combination
# the version-4 gate stopped covering.
mkdir -p "$WA/agg"
# shellcheck disable=SC2086
if run "$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REGION --dim 4 \\
''' + T + '''--data-root "$DATA" --out-dir "$WA/agg" --tex-dir "$WA/agg/tex" --native "$WA/agg" \\
''' + T + '''--road-detail 1 --aggregate --aggregate-min 4; then
''' + T + '''AGGI="$(pairdir "$W/agg")/Commonwealth.lodi"
''' + T + '''AGGO="$(pairdir "$W/agg")/Commonwealth.lodo"
''' + T + '''AGGN="$("$PY" -c "import struct,sys;b=open(sys.argv[1],'rb').read();print(struct.unpack_from('<I',b,0xC0)[0],struct.unpack_from('<I',b,4)[0])" "$AGGI")"
''' + T + '''echo "    the --aggregate bake: aggregateCount and version = $AGGN"
''' + T + '''case "$AGGN" in
''' + T + '''0[[:space:]]*) bad "(14) the --aggregate bake carries no aggregate, so the v5 rules cannot be tested here" ;;
''' + T + '''*[[:space:]]5)
''' + T + T + '''note "(14) the --aggregate bake is a VERSION 5 file that carries aggregates ($AGGN)"
''' + T + T + '''cp "$AGGO" "$W/doctor/clean.lodo"
''' + T + T + '''cp "$AGGI" "$W/doctor/clean.lodi"
''' + T + T + '''doctorCase agg-views "aggregateViews"
''' + T + T + '''doctorCase agg-cover "twice" ;;
''' + T + '''*) bad "(14) the --aggregate bake is not version 5 ($AGGN); the v5 rules cannot be tested here" ;;
''' + T + '''esac
else
''' + T + '''bad "(14) the --aggregate bake did not run"
''' + T + '''tail -3 "$W/last.log"
fi

'''


def main():
    s = io.open(P, encoding='utf-8', newline='').read()
    if s.count(ANCHOR) != 1:
        print('ABORT: the summary anchor appears %d times' % s.count(ANCHOR))
        return 1
    if 'lodgen_native_doctor.py' in s:
        print('ABORT: section 14 is already in the gate')
        return 1
    out = s.replace(ANCHOR, SECTION + ANCHOR, 1)
    assert out.count('\r') == s.count('\r') == 0
    d = os.path.dirname(P)
    f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d,
                                    delete=False, suffix='.tmp')
    f.write(out)
    f.close()
    os.replace(f.name, P)
    print('lodgen_native.sh: %d -> %d bytes, CR %d' % (len(s), len(out), out.count('\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
