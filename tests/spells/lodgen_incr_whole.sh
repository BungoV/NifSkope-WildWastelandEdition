#!/bin/bash
# lodgen_incr_whole.sh -- lane INCR2 (2026-09-26): whole-map --incremental on the FO4CS target (--dim all + region
# products), gated against a full bake byte for byte (every file of the mod tree but the .lodb record).
# usage: bash tests/spells/lodgen_incr_whole.sh <exe abs> <ws> <x0 y0 x1 y1> <cards dir> <work dir abs> <legs...>
#   env: PROFILE (MO2 profile dir), VANILLA (vanilla terrain LOD input cache).
#   SCRBASE (optional): each arm's out-dir goes to SCRBASE/<arm> instead of <work>/<arm>/scr. Put it on
#   ANOTHER DRIVE than <work> to test absolute out rows (a cross-drive out-dir once made every chunk "output lost").
#   A touch leg needs <work>/touch.nif and <work>/touch.rel (the LOD model's Data-relative path) first.
# Measured 2026-09-26 on Commonwealth -32 -32 -1 -1 (85 jobs): full 979 s, null 128 s, forced replay 488 s;
# refute red (.lodo/.lodi differ), heal/staleraw/touch-vs-full IDENTICAL.
# legs: full  null  lodo  refute  staleraw  touch  touchref
#   full     full bake into W/A (resource dir W/res empty); snapshot A.sha
#   null     --incremental on A, nothing touched -> expect "nothing moved"; compare to A.sha
#   lodo     delete A's .lodo -> every chunk replayed from cache, all region passes -> compare to A.sha
#   refute   as lodo with WW_LODGEN_INCR_NO_RESTORE=1 -> MUST differ from A.sha (red)
#   staleraw edit one cached raw manifest -> that chunk rebakes; compare to A.sha
#   touch    put an edited LOD nif (W/touch.nif -> W/res/<rel>) then --incremental on A
#   touchref full bake with the touched res into W/B; compare A (after touch) to B
set -u
NS="$1"; WS="$2"; X0=$3; Y0=$4; X1=$5; Y1=$6; CARDS="$7"; W="$8"; shift 8
P="${PROFILE:-E:/Projects/Fallout 4 Mods/profiles/Default}"
VR=( --vanilla-lod-root "${VANILLA:-E:/Tools/Fallout 4/DataUnpacked/Data}" )
mkdir -p "$W/logs" "$W/res"
stamp() { date +%H:%M:%S; }
scr() { if [ -n "${SCRBASE:-}" ]; then echo "$SCRBASE/$1"; else echo "$W/$1/scr"; fi; }
gamegate() { if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "$(stamp) GAME UP" | tee -a "$W/logs/stages.txt"; exit 1; fi; }
bake() { # <arm> <logname> [extra args...]
	local arm="$1" name="$2"; shift 2; gamegate
	local t0=$(date +%s)
	"$NS" -no-gui lodgen --mo2-profile "$P" --resource "$W/res" --worldspace "$WS" \
		--terrain-region $X0 $Y0 $X1 $Y1 --dim all --out-dir "$(scr $arm)" --tex-dir "$(scr $arm)/textures" \
		--native "$W/$arm/mod" --vt "$W/$arm/mod" --vt-height --vt-density 16 --cover --vt-fill-vanilla "${VR[@]}" \
		--land-fill-vanilla --impostors "$CARDS" --arrays --fo4cs-one-root "$@" > "$W/logs/$name.log" 2>&1
	local rc=$?
	echo "$(stamp) $name rc=$rc $(( $(date +%s) - t0 )) s" | tee -a "$W/logs/stages.txt"
	grep -E '^incremental|^raw chunk|^vt: kept|nothing moved|^card sets|^native cache|^rings' "$W/logs/$name.log" | sed 's/^/    /' | tee -a "$W/logs/stages.txt"
	return $rc
}
snap() { # <arm> -> sha list of the mod tree, record excluded
	( cd "$W/$1/mod" && find . -type f ! -name '*.lodb' -print0 | sort -z | xargs -0 sha1sum )
}
cmpto() { # <arm> <ref.sha> <label>
	snap "$1" > "$W/logs/$3.sha"
	if diff -q "$2" "$W/logs/$3.sha" >/dev/null; then echo "$(stamp) GATE $3: IDENTICAL ($(wc -l < "$2") files)" | tee -a "$W/logs/stages.txt"; return 0; fi
	local n=$(diff "$2" "$W/logs/$3.sha" | grep -c '^[<>]')
	echo "$(stamp) GATE $3: DIFFERS ($n diff lines)" | tee -a "$W/logs/stages.txt"
	diff "$2" "$W/logs/$3.sha" | grep '^[<>]' | awk '{print $1, $3}' | sort -k2 | uniq -c -f1 | head -12 | sed 's/^/    /' | tee -a "$W/logs/stages.txt"
	return 1
}
for leg in "$@"; do
	case $leg in
	full)  rm -rf "$W/A" "$(scr A)"; mkdir -p "$W/A/mod" "$(scr A)"; bake A full || exit 1; snap A > "$W/logs/A.sha"
	       du -sh "$W/A/mod" "$(scr A)" "$(scr A)/lodgen_chunk_cache" | sed 's/^/    /' | tee -a "$W/logs/stages.txt" ;;
	null)  bake A null --incremental "$W/A/mod"; cmpto A "$W/logs/A.sha" null
	       # bytes alone cannot fail here: a run that rebakes everything is identical too; the census must say it
	       if grep -q 'nothing moved' "$W/logs/null.log"; then echo "$(stamp) GATE null census: nothing moved" | tee -a "$W/logs/stages.txt"
	       else echo "$(stamp) GATE null census: FAIL -- the null run rebaked (read its incremental line)" | tee -a "$W/logs/stages.txt"; fi ;;
	lodo)  rm -f "$W/A/mod/FO4CSLOD/"*/*.lodo; bake A lodo --incremental "$W/A/mod"; cmpto A "$W/logs/A.sha" lodo ;;
	refute) rm -f "$W/A/mod/FO4CSLOD/"*/*.lodo; WW_LODGEN_INCR_NO_RESTORE=1 bake A refute --incremental "$W/A/mod"
	       if cmpto A "$W/logs/A.sha" refute; then echo "$(stamp) REFUTER DID NOT GO RED" | tee -a "$W/logs/stages.txt"; else echo "$(stamp) refuter red as required" | tee -a "$W/logs/stages.txt"; fi
	       # heal: a normal incremental run must bring A back
	       rm -f "$W/A/mod/FO4CSLOD/"*/*.lodo; bake A heal --incremental "$W/A/mod"; cmpto A "$W/logs/A.sha" heal ;;
	staleraw) m=$(ls "$(scr A)/lodgen_chunk_cache/"*.manifest.txt | head -40 | tail -1); echo "    edited $(basename "$m")" | tee -a "$W/logs/stages.txt"
	       printf '#\n' >> "$m"; bake A staleraw --incremental "$W/A/mod"; cmpto A "$W/logs/A.sha" staleraw ;;
	touch) [ -f "$W/touch.rel" ] || { echo "no touch.rel"; exit 1; }; rel=$(cat "$W/touch.rel")
	       mkdir -p "$W/res/$(dirname "$rel")"; cp "$W/touch.nif" "$W/res/$rel"
	       bake A touch --incremental "$W/A/mod" ;;
	touchref) rm -rf "$W/B" "$(scr B)"; mkdir -p "$W/B/mod" "$(scr B)"; bake B touchref || exit 1; snap B > "$W/logs/B.sha"
	       cmpto A "$W/logs/B.sha" touch_vs_full ;;
	esac
done
