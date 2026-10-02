#!/usr/bin/env python3
"""Join the part files of a sliced cell census into one census (lane PRTP5, tests/spells/cell_census.sh).

With WW_CELL_CENSUS_SLICE=i/N each NifSkope window writes `<census>.part<i>of<N>.tsv`. This joins the N
parts into `<census>.tsv`: the first part's two header lines, then every part's rows in slice order.

It joins and nothing else. It does NOT drop a row that is there twice and does not invent one that is
missing: whether every cell has exactly one row is the checker's question (cell_census_check.py), and a
merge that tidied the parts would hide the very thing the checker is there to find.

It refuses (exit 2, nothing written) when a part is missing, when a part's rows say they were written by
another slice count, or when the parts do not have the same columns.

  cell_census_merge.py <census.tsv> <N>
"""

import os
import sys


def part_path(census, i, n):
    stem = census[:-4] if census.lower().endswith(".tsv") else census
    tail = ".tsv" if census.lower().endswith(".tsv") else ""
    return "%s.part%dof%d%s" % (stem, i, n, tail)


def main(argv):
    if len(argv) != 3 or not argv[2].isdigit() or int(argv[2]) < 1:
        print(__doc__)
        return 2
    census, n = argv[1], int(argv[2])
    comment, columns, rows, counts = None, None, [], []
    for i in range(1, n + 1):
        path = part_path(census, i, n)
        if not os.path.isfile(path):
            print("REFUSED: slice %d of %d has no part file (%s)" % (i, n, path))
            return 2
        mine = 0
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                line = line.rstrip("\r\n")
                if not line:
                    continue
                if line.startswith("#"):
                    comment = comment or line
                    continue
                if line.startswith("key\t"):
                    if columns is None:
                        columns = line
                    elif line != columns:
                        print("REFUSED: %s does not have the columns of the first part" % path)
                        return 2
                    continue
                rows.append(line)
                mine += 1
        counts.append(mine)
    if columns is None:
        print("REFUSED: no part has a column line")
        return 2
    at = columns.split("\t").index("slice")
    for line in rows:
        s = line.split("\t")[at]
        if not s.endswith("/%d" % n):
            print("REFUSED: a row of %s says slice %s, not one of %d" % (line.split("\t")[0], s, n))
            return 2
    with open(census, "w", encoding="utf-8", newline="\n") as f:
        f.write((comment or "# WW cell census v2") + "   [joined from %d parts]\n" % n)
        f.write(columns + "\n")
        for line in rows:
            f.write(line + "\n")
    print("joined %d parts into %s: %s rows (%d in all)" % (n, census, " + ".join(str(c) for c in counts), len(rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
