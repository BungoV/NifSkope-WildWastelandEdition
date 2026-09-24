# Section 9 — CLOSING THE ROUND TRIP (added by lane BUILD8, 2026-09-10)

Sections 1–8 gate a WRITER against an independent reader. This section is for
the day the pair exists — writer AND importer — and someone asks for the trip:
*"export and import, see if they're 1:1"*. It was earned on FO4 `.hkx` ↔ glTF
2.0 in one lane; nothing in it is specific to those two formats.

## 9.1 The trip runs through the SHIPPING BINARY, or it is not the trip

A standalone gate driver proves the translation units. It does not prove that
the application can do the thing. Lane HKX5b's importer and writer were linked
into `NifSkope.exe` for a whole build and **no route reached either of them** —
no menu item, no CLI command, nothing. "Linked" is not "reachable", and a lane
that reports the former while the brief asked for the latter has not delivered.

So: before measuring anything, make each half addressable from the shipping
binary (`ww-anchored-hookup` writes the hook-up), and prove it with the
binary's OWN help output, not with the source.

## 9.2 One text format is the currency, and BOTH ends of every leg get one

Give the binary a `--tsv` / `dump` command that writes the in-memory object as
rows. Then every leg of the trip is a file diff and every comparison is the
same script:

```
original.bin --(independent decoder A)--> orig.tsv
original.bin --(the app's reader)-------> orig_app.tsv     leg 0: the reader itself
orig --(app export)--> interchange --(app import, --tsv)--> imported.tsv
                                    --(app write)--> written.bin
written.bin --(independent decoder B)---> written.tsv
written.bin --(the app's reader)--------> written_app.tsv
```

Six comparisons fall out, and each one accuses a different component:

| comparison | what a failure means |
|---|---|
| `orig` vs `orig_app` | the app's READER disagrees with the oracle |
| `orig` vs `imported` | **the round trip**, before the writer is involved |
| `orig` vs `written` | the round trip end to end |
| `imported` vs `written` | the WRITER alone (must be 0.0, it is a memcpy of floats) |
| `written` vs `written_app` | the app's reader on its own output (must be 0.0) |
| `orig_app` vs `orig` on a MUTATED file | the floor |

Reporting only the end-to-end number tells you nothing about which half is
lossy. The lane that measured all six could say "the writer contributes exactly
zero" as a fact rather than a belief.

## 9.3 A lossy leg is restricted BY NAME, never by loosening the bar

An interchange format that cannot carry everything drops rows. Do not compare
"what both files happen to have" silently: key the rows by the semantic
identity (bone name, not track index — the index is re-numbered by the trip),
intersect, and **print the count dropped from each side**. Then check that the
dropped set is exactly the set the exporter already NAMED as unable to travel.
On the FO4 rig that was 17 `Weapon*` tracks × the frame count = 391 and 1,581
rows, and the exporter lists all 17 by name in its own report — so the
restriction is a re-statement of a documented loss, not a hiding place.

## 9.4 Two defaults that each look right can fail to compose

The two contract pages are written by two lanes and each is internally correct.
Hold them against each other for the things that only exist BETWEEN them:

* **the same quantity named from two ends.** Our exporter composed root motion
  onto the *root bone's* node; our importer defaulted to *the scene root*. Both
  sentences are true and they are different nodes, and the import refused by
  name until the option existed. Neither page said it.
* **a default that is a value vs a default that is "whatever the file says".**
  The exporter wrote the clip's own frame rate; the importer defaulted to 30
  fps. A 60 fps clip round-tripped at defaults comes back at half its frames.

Both classes are invisible to either lane alone and both show up in the first
end-to-end run. When you find one, write it into BOTH pages, with the refusal
sentence quoted verbatim.

## 9.5 THE PICTURE NEEDS A NOISE FLOOR BEFORE IT NEEDS A DIFFERENCE

The visual proof is the same frames rendered from the original and from the
round-tripped data, one pinned camera. Before believing any difference:

1. **render the SAME input twice and diff that.** If it is not zero, the
   renderer is contributing and no per-tile number below it means anything. Ours
   was 0 pixels on all six tiles, which is what let 170 differing pixels be
   attributed to float error rather than to the renderer.
2. **include a tile that CANNOT differ** — a bind pose with no clip loaded, the
   same on both sides. A run where that tile differs is measuring the wrong
   thing and says so in the report.

## 9.6 Count colour differences PER CHANNEL

`ImageChops.difference(a, b).convert("L")` weights blue at 0.114, so a pixel
that differs by (0, 0, 1) **rounds to zero** and the tile reports IDENTICAL.
Ours printed a bounding box and a count of 0 on the same line before anyone
noticed. Split the bands, count a pixel as differing if any channel does, and
take the worst step as the max over bands.

And the difference PICTURE is not a fixed-factor amplification of the whole
frame: ×16 on a step of 1 is 16/255, i.e. black, and reads as "no difference".
Crop to the bounding box with a margin, amplify by `255 // worst`, and put the
factor in the caption (`ww-texel-picture` is the full procedure).

## 9.7 The third-party leg measures the third party, and say so

Driving the reference application (Blender, headless, `--factory-startup`)
through import→export→import is the trip the user will actually do. Report what
IT changed, separately from what we changed:

* **the frame rate is the big one.** Blender lays an imported animation onto the
  SCENE's frame grid, and the factory scene is 24 fps. A 30 fps clip through
  Blender at defaults came back re-timed — 22 frames instead of 23, and
  0.29 units / 4.83° of pure resampling. With the scene rate set to the clip's
  own rate: 23 frames and 1.006e-04 units / 4.68e-04°.
* **concepts the format has no field for** get invented on the way back: Blender
  gave 107 of 110 bones a non-zero *roll*. Count them and name them; they are
  not a defect and they are not nothing.
* run BOTH — the factory default and the corrected setting — because the
  difference between them IS the instruction to give the user.

Never quote the third-party leg's number as "the round trip error". Quote our
own leg, then theirs, then the setting that closes the gap.

## 9.8 The application must be able to open what it just wrote

The last thing to check, and the easiest to skip because every gate is green
without it: feed the written file back to the SHIPPING binary. Ours refused —
the reader dispatched on a class name and the writer emitted a different class,
so NifSkope could not open its own export, and the picture proof (which goes
through that reader) was impossible. Every independent decoder in the world
passing does not make a pair usable.
