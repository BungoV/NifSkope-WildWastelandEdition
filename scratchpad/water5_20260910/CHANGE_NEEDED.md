# Lane WATER5 -- changes the SOLVER owes the window (not made by this lane)

The brief: *"SOLVE runs WATER4's solver (the existing solve() entry point --
do not reimplement it; if its signature needs a change, write it here)"*.
`WaterMarkDoc::solve( WaterMarkSolve *, QString * )` is called AS IS by
`WaterWindow::solve()` after `WaterCurveDoc::writeTo()` mirrors the curves
into the stroke store.  Its signature does NOT need to change.  What it does
not yet CONSUME is below; each is a `src/watermark.cpp` change for the lane
that owns that file, with the mechanism named and the gate that would prove it.

## C1. Per-point speed weight (WaterStroke::extra, one float a point on kind 0 / 1)

As written, `solveBody` (watermark.cpp ~1413-1429) takes ONE speed a stroke
(`s.speed`, averaged into `sumSpeed` over the strokes) and the stroke's path
enters as a conductance bump.  The window stores a weight `w` a point
(1 = the curve's speed, 2 twice, 0.5 half).

Proposed consumption, smallest first:
* the speed nibble: where the written speed is `round( 8 |u| / mean|u| )`,
  multiply `|u|` at a texel by the weight interpolated along the nearest
  curve segment (within the curve's half-width; 1 outside) BEFORE the mean is
  taken -- a weighted reach reads faster or slower without moving the
  direction field; 
* the conductance bump: scale the x4 bump by `w` along the segment, so a
  heavily weighted reach also attracts more flux.  This one moves the
  direction and needs the F5 numbers re-read.

Gate: a straight synthetic channel with a curve whose weights go 1 -> 2 over
its length; the speed nibble at the end is twice the nibble at the start
(+- one step), direction unchanged (p99 adjacent difference unchanged).

## C2. A one-point curve = a pin (kind 1 with ONE point)

`solveBody` skips any kind 0 / 1 stroke with fewer than two points
(`if ( s.pts.size() < 2 ) continue;`, watermark.cpp ~1414).  bungo's ruling:
*"a one-point curve = a pin"*.  A one-point pin carries no direction, so what
it can mean is either
* (a) a SPEED pin: the speed nibble within its width is held at the pin's
  `speed` x the point's weight, direction from the solve; or
* (b) a WAYPOINT: a source-of-flux disc on the way, equivalent to the
  stroke's first/last-point behaviour (the solve already makes those a
  source and a sink).

Recommendation: (a), because it is the only reading that does not ask the
solver for a direction the user did not give.  Gate: the F1 channel with a
speed pin of speed 1.0 at x = 128, weight 1: the nibble within the pin's
width is 15 (saturated) and the direction at those texels is unchanged.

## C3. Raster layer authority (kind 10, `WaterStroke::extra` = the payload)

The window imports a flow-map PNG as a `WaterRasterLayer` (watercurves.h)
and, after hook-up H2, stores it as a kind-10 record.  bungo's ruling:
*"import = a raster source layer in the stroke store (authority where
painted)"*.  Nothing in `solve()` or `flowWordOf()` reads it yet; the window
paints it (Show = Imported flow) and the file carries it.

Proposed consumption: in `WaterMarkDoc::flowWordOf( px, py, id )`
(watermark.cpp ~2509), before the solved field and the automatic word are
consulted, ask the raster layers (last painted wins) and return the raster's
word where painted.  That is the whole of "authority where painted": the
solve is untouched, the plane the file writes is the raster's where it says
so.  `WaterCurveDoc::rasterWordAt()` is the reference implementation of the
lookup and `WaterRasterLayer::fromPayload()` decodes the record.

Gate: gate W5 (export -> import -> byte-identical flow plane) extended by one
step: after the import, `save()` and reopen; the flow plane's words equal the
raster's over the painted texels -- the words that would be read by FO4CS.

## Not a change: what `solve()` already does that the window relies on

* `addStroke` names the body from the first WET point and stores dry points
  as drawn -- the window's curves may cross land and the sentence says so.
* `setBodyLockZero`, `setBodyDyeMouth`, `setDyeHalfDistance` write marks of
  kinds 6 / 9 / 8, which `writeTo()` leaves alone (it replaces only kinds
  0 / 1 / 4 / 5 / 7 / 10).
* `sweep()` is what the export and the W4 / W5 gates read; it walks the body
  plane once and hands back the word this document WOULD write.
