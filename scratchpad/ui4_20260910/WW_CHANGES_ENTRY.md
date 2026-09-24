<!-- Lane UI4, 2026-09-10. TEXT ONLY for WW_CHANGES.md; the director splices it
     (CONSTITUTION 8). WW_CHANGES.md is mixed CRLF/LF and stays so -- splice by
     Python, match the neighbours, and check the CR count did not move. -->

### The Header | Blocks | Files strip gets four pixels of air (lane UI4)

bungo, over a screenshot of the 18:25:20 window: *"just do what is on my
screenshot, 4 pixels from each nearby element of separation for the header /
blocks / files"*. The segmented strip filled its 35 px row edge to edge --
0 px above, 0 below, 0 from the window's left edge, 0 between the segments and
3 to the toolbar beside it, and those 3 were the painted dock separator, not
air.

**The row does not move.** `wwAlignBarRow` still pins the strip to the shared
35 px row, the menu bar, the toolbars, the viewport header, their buttons and
the dock's search row are exactly where lane UI3 left them, and the segments
get 8 px shorter inside the same row instead: **35 / 0 / 0 / 0 / 3 / 0 ->
35 / 4 / 4 / 4 / 4 / 4** (row height, then top, bottom, left, right-to-toolbar,
between).

The air is stated once, in the shared skin, as MARGIN:
`wwSegmentedStripAir()` (`UI/SegmentedStripAir`, default 4) is the one reader;
`wwSegmentedQss` puts `margin` on the row-path segments and drops `min-height`
to `rowHeight - 8 - 2*air`; a separated segment closes its own box, because the
joined strip's shared seam (`border-left: 0`) leaves an open-sided rectangle
once there is a gap beside it. `wwSegmentedTabBarQss` now takes the window,
because the right-hand air is the full air LESS the dock separator, and that
style metric answers 3 asked with a widget and 6 asked with nullptr. **The way
back is exact**: `UI/SegmentedStripAir = 0` emits the 18:25:20 sheet, flush
seam and square inner corners included.

Measured outside the application first (`scratchpad/ui4_20260910/probe.cpp`,
skill `ww-qss-geometry-probe`), which is what kept it to one build: the probe
reproduced the shipped strip exactly (383x35 bar, segments 128/127/128, header
at x 386) and then found that **`QTabBar::tabRect()` returns the rect INCLUDING
the margin** -- identical before and after -- so the gate reads the PAINTED box
out of a grab instead. It also found that `release/style.qss` is an
unsubstituted byte copy of `res/style.qss`, which had cost the first probe run
a 6 px separator the application does not have.

Gate: `tests/spells/water_ui.sh` group **S** -- the five distances plus "the row
did not move", each within 1 px, with two floors (every segment found as a real
painted box; the same scan finds nothing for a colour the strip does not carry)
and a live sabotage floor that appends the 18:25:20 flush strip over the shipped
sheet, watches all five collapse, and takes it away again.
