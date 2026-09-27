#!/bin/bash
# GPU1 comparator: sha1 of every file under <a>/{mod,scr} vs <b>/{mod,scr} (same relative paths).
# usage: cmp_trees.sh <bake A> <bake B> [label]
# Prints one verdict line: SAME n files | DIFF k of n (+ the first differing paths) | MISSING.
# Skips the files that carry wall times or absolute run paths by design (lodgen_timing*, *.log);
# the list of skipped names is printed so it can never hide a product file.
A="$1"; B="$2"; L="${3:-}"
tmp=$(mktemp -d)
for s in A B; do
	d=${!s}
	( cd "$d" && find mod scr -type f | LC_ALL=C sort | while read -r f; do
		case "$f" in
		*.log|*lodgen_timing*) echo "SKIP $f";;
		# the bake record carries the clock, the run paths and the census wall times: compare its
		# product / input / digest lines only
		# product / input / digest lines only. Its `chunk` lines and the chunk cache `.key` files carry the
		# chunk INPUT digest, which hashes the running exe's bytes by design (lodgenGeneratorIdentity), so
		# two different exes always differ there: their digest field is masked. The flat-objects report
		# names the override file, which lives beside the run's exe: that one line is masked.
		*.lodb) echo "$(grep -P '^(product|out|resource|plugin|chunk|hash|shape|loadorder)\t' "$f" | sed -E -e 's/^(chunk\t.*\t)[0-9a-f]{40}$/\1<exe-digest>/' -e 's/^(product\t[^\t]*flat_objects_report\.txt\t)[0-9a-f]{40}$/\1<path-digest>/' | sha1sum | cut -c1-40) $f(record lines)";;
		*.key) echo "$(sed -E 's/^inputs [0-9a-f]{40}$/inputs <exe-digest>/' "$f" | sha1sum | cut -c1-40) $f(inputs masked)";;
		*flat_objects_report.txt) echo "$(grep -v '^# Override file: ' "$f" | sha1sum | cut -c1-40) $f(override path masked)";;
		*) echo "$(sha1sum "$f" | cut -c1-40) $f";;
		esac
	done ) > "$tmp/$s.txt"
done
skipped=$(grep -c '^SKIP' "$tmp/A.txt")
grep -v '^SKIP' "$tmp/A.txt" > "$tmp/a"; grep -v '^SKIP' "$tmp/B.txt" > "$tmp/b"
n=$(wc -l < "$tmp/a")
if ! diff -q <(cut -d' ' -f2- "$tmp/a") <(cut -d' ' -f2- "$tmp/b") > /dev/null; then
	echo "MISSING $L: file lists differ"; diff <(cut -d' ' -f2- "$tmp/a") <(cut -d' ' -f2- "$tmp/b") | head -5
fi
k=$(diff "$tmp/a" "$tmp/b" | grep -c '^<')
if [ "$k" = "0" ]; then echo "SAME $L: $n files (skipped $skipped: $(grep '^SKIP' "$tmp/A.txt" | cut -c6- | tr '\n' ' '))"
else echo "DIFF $L: $k of $n files"; diff "$tmp/a" "$tmp/b" | grep '^<' | head -8 | cut -c44-; fi
rm -rf "$tmp"
