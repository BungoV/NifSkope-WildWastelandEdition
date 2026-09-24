---
name: ww-material-folder-classify
description: Classify a shipped Bethesda asset by the FOLDER its material lives in, never by the material's file name, and keep the C++ rule and the offline Python rule the same rule. Use whenever a NifSkope Wild Wasteland pass has to say what KIND of thing a shape is -- road surface vs terrain vs sidewalk, one family vs another -- from a corpus you did not author, and whenever a census counts things by class.
---

# Classify by the material's folder, not by its name

Written from lane ROADS4 (2026-09-12), where a name-stem list hid the entire
finding for the first hour and the folder rule found it in one run.

## The situation

A pass has to treat some shapes differently from others, and the only thing it
knows about a shape is its material path. The temptation is a list of name
stems -- `asphalt`, `concrete`, `sidewalk`, `road` -- because you can read a
handful of NIFs and write it in two minutes.

**Bethesda did not name their materials for your classifier.** The two biggest
contributors to the thing lane ROADS4 was measuring were:

| material | texels won | what the stem list said |
|---|---|---|
| `CommonwealthDefault01.bgsm` | 7,558 | unclassed |
| `SancSW01.BGSM` | 5,317 | unclassed |

One is the Commonwealth's default landscape material and the other is a
Sanctuary sidewalk, and nothing in either NAME says so. They were 12,875 texels
in an "unclassed" bucket that read like noise. The FOLDERS say it outright:
`materials\Landscape\Ground\CommonwealthDefault01.bgsm` and
`materials\Landscape\Roads\SancSW01.BGSM`. With the folder rule the same corpus
split 36.1 % / 63.9 % and the finding was one line of output.

The folder is a data-driven discriminator the ARTISTS maintained, because that
is how they organised their own build. A name is a label; a folder is a
decision someone made about what the thing IS.

## The rule

```cpp
bool lodgenRoadMaterialIsGround( const QString & matName )
{
	if ( matName.isEmpty() )
		return false;
	return lodgenRoadMaterialPath( matName ).toLower()
		.contains( QStringLiteral( "materials/landscape/ground/" ) );
}
```

Three things in four lines, and all three matter:

1. **Normalise first.** Shipped NIFs carry material paths in at least four
   shapes: bare (`materials\Landscape\Roads\Asphalt01.bgsm`), `Data\`-prefixed,
   the absolute Bethesda build path
   (`C:\Projects\Fallout4\Build\PC\Data\materials\...`), and forward-slashed.
   `lodgenRoadMaterialPath()` keys on the **last** `materials/` in the string,
   which collapses all four. Write that helper before the classifier, not after
   the first surprise.
2. **Case-fold.** `SancSW01.BGSM` and `asphalt01.bgsm` ship in the same folder.
3. **Match the folder WITH its separators** -- `materials/landscape/ground/`,
   not `ground` -- or `Landscape\Ground\DirtGravel01` and
   `Landscape\Roads\GroundCrackDecal01` land in the same bucket.

An empty material is its own class (`no-material`), never silently one of the
real ones. Lane ROADS4's corpus had 80 shapes with no material at all.

## Keep ONE rule, in two languages, and prove they agree

The C++ pass classifies at bake time and the offline Python classifies the same
shapes to measure what the bake did. **If the two rules differ, the measurement
is not about the bake.** Make them textually the same predicate:

```python
GROUND_FOLDER = 'landscape/ground/'
def classify(mat):
    p = (mat or '').replace(chr(92), '/').lower()
    if not p:              return 'no-material'
    if GROUND_FOLDER in p: return 'terrain'
    ...
```

and then gate on a number the binary prints: lane ROADS4 added
`ground_shapes` / `ground_texels` to the road census so the exe's own count can
be put beside the Python one. **A classifier whose only witness is the script
that measures it has no witness.**

## The three checks before the classification is believed

1. **Print the full table, every class, with an `unclassed` row.** Not the count
   of the class you care about. A large `unclassed` row is the finding, and it
   is the row a stem list makes look small and boring -- 12,875 texels across
   two entries reads as noise until you see it is 55 % of the class you were
   hunting.
2. **Sort by size and read the top ten names out loud.** The two that broke
   lane ROADS4's stem list were rows 1 and 2.
3. **Count the path shapes in the corpus.** One line: how many distinct
   prefixes appear before the last `materials/`. If it is more than one, the
   normalisation is load-bearing and every downstream number depends on it.

## Where this generalises

Anywhere the tree asks "what kind of thing is this shape" about assets someone
else authored: road vs terrain vs sidewalk, LOD family (`materials/LOD/`),
decal families, the impostor candidate rules. The failure mode is always the
same -- a plausible name list that is silently incomplete, producing a number
that is wrong in one direction only, which is exactly what
`ww-spec-gate-audit` says to look for.

**And the corollary: a folder rule can be wrong too.** It is a claim about how
the corpus is organised, so measure it. Lane ROADS4's claim was "shapes carried
by road NIFs whose material is under `Landscape/Ground/` are the verge", and
the picture that checks it is `road_ground_where.png`: those texels drawn in
red on the sheet, and they trace the verge and the junction fill along the
Sanctuary loop. **If you cannot draw your classification on the artefact and
recognise what it selected, you have not verified it.**
