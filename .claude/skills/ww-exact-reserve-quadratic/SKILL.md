---
name: ww-exact-reserve-quadratic
description: Find and fix the "exact reserve in a loop" slowdown (vector.reserve(size()+n) or QVector::reserve per item) that turns an append loop quadratic, and prove the fix changes no byte. Use when one single-thread bake stage takes minutes for work that should take seconds, or before parallelising a stage that appends into one growing array.
---

# The exact reserve that makes an append loop quadratic (lane GPU1, 2026-09-26)

## The pattern
```cpp
for ( const Placement & p : placements ) {
    samples.reserve( samples.size() + p.count );   // exact size: capacity == size afterwards
    for ( ... ) samples.push_back( ... );
}
```
`reserve` to an exact size replaces the vector's doubling: every call reallocates and copies the WHOLE array,
so N appends cost O(N^2). GPU1's identity join (src/nativeemit.cpp) did this per placement: 42306 placements
into 1.9 M samples took 109 s at the Boston box and ~1330 s whole map. Measured offline on the bake's own join input, the
reserve was ~100 s of Boston's 103 s; with it gone (and the walk grouped by cell and placement, see the comment
in src/nativeemit.cpp) the join takes 0.7 s.

## Find it
* The profile shows the time in `memcpy` (ucrtbase) and the allocator (ntdll) under ONE function, on one thread.
* Search ONE source file at a time (search-lean) for `reserve(` whose argument contains `size()` or a running
  total, inside a loop. `QVector`/`QList::reserve` behave the same way.

## Fix it
* Delete the in-loop reserve, or reserve ONCE before the loop with the total (count it in a first pass).
* Measure it offline first: dump the stage's input once from the bake (GPU1: a temporary dump behind an env
  var, removed before commit), replay the stage in a small exe, time old vs new, then build the product.
* If the loop must also regroup data (GPU1 grouped the walk by cell), keep the output order identical.

## Prove it
* Same counts in the stage's own log line (placements, samples, pairs).
* Whole-bake byte identity on the Boston box against the exe before the fix: the lane's `cmp_trees.sh`
  (sha1 of every output file; masks only the exe digest in chunk keys and the run path in the flat-objects
  report). GPU1: SAME, 253 files.
