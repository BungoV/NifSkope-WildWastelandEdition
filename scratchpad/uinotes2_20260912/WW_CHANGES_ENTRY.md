## 2026-09-12 — The two gizmo switches become icons, and Ctrl+A keeps the whole selection

**The transport row is all drawings now.** "pose" was the last word in a row of
glyphs, and it sat on the font's baseline — nine 2:1 pixels below the line every
drawing beside it is centred on, because a text button aligns text and an icon
button centres its icon. That, and not the auto-key dot, is what read as "the
round dot that's not centered". Both switches are glyphs on the same 16x16 grid
as the other ten, with the same 2 units of stroke and the same palette ink:

* **Auto-key** stays Blender's record dot, at radius 5 instead of 4, so it
  carries the ten-unit mass every other glyph in the set has instead of reading
  as a speck.
* **Pose with the gizmo** is a new glyph and it is OURS, not Blender's, said
  plainly because the house rule is to follow Blender and name every divergence:
  Blender has no single icon for this, its armature glyph is a bone and the gizmo
  is a ring drawn in the viewport, never on a button. So the glyph is the two put
  together — Blender's octahedral bone (head, two shoulders a third of the way
  down, a long tail) inside the two side arcs of the rotate gizmo's ring, the
  ring open top and bottom so the bone's tips read as tips.

**Both light up when the mode is on, in white.** The lit ink is `textBright`
(#f2f3f5), the palette's brightest ink, over the plain `text` (#e6e8eb) of an
unlit one; a toggle that is on but cannot be clicked draws in `text`, brighter
than the `textMuted` of a disabled off one, so it does not read as off. The
first cut lit them in the `accent` orange and bungo overruled it on the picture:
"Why do these turn yellow when selected? the buttons, keep them white". The step
between the two inks is deliberately small — 33 of a possible 765 in summed RGB
— because the signal the eye reads is the blue `:checked` plate under the glyph;
the ink only has to stop contradicting it. Play and Play backwards deliberately
do not light: they already say they are running by swapping to the pause bars.
Loop does not either, and that is a measurement, not a taste — the render
toolbar owns that shared action's icon and re-skins it on every refresh of its
popup.

**Ctrl+A in the Animations list now keeps every row.** It kept one. Two places
threw the selection away and both are fixed: `setSequenceByName`'s
`setCurrentItem`, whose one-argument form carries Qt's implicit `ClearAndSelect`,
now moves the current row without touching the selection when that row is
already part of it; and `rebuildList`, which had been restoring only the CURRENT
row, now remembers every selected row by kind and name and puts them all back.
The second one was quietly reducing every multi-selection to one row on every
refresh, so Delete, Copy and Cut acted on one clip however many the user had
picked.

A single click still drives the viewport exactly as before: the guard is on the
driven re-entry, not on the drive.

**Keys outside the play range are greyed out.** In the dope sheet every key
diamond whose frame falls outside the Start..End range is drawn dimmed, so the
picture says which keys the playback still takes into account. The dim is the
darkened band's own arithmetic applied to the ink instead of to the ground —
the `animOutOfRange` token composited over the key's colour at Blender's alpha
(155/255, `ANIM_draw_framerange`) — so a greyed diamond looks exactly as though
the out-of-range wash had been painted across it: `animKey` #bfbfbf becomes
#595a5c. A SELECTED key that falls out of the range keeps its own colour, dimmed
the same way (#ffbe33 becomes #725a25, #ff8c00 becomes #724611), so it still
reads as selected while it says "ignored". Nothing else about such a key changes
— same size, same shape, still clickable and draggable — and the diamonds follow
the ruler grips live, because dragging one already repaints the sheet. The
darkened band behind them is untouched.

This is a stated divergence from Blender: Blender darkens the out-of-range
background and leaves the keys as bright as the ones inside. bungo asked for the
keys themselves: "also, grey out diamond keyframes out of animations start and
end range, to indicate they're not being taking into consideration anymore".
