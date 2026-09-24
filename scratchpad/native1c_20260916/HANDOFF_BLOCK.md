# For bungo -- 2026-09-16

**Your open NifSkope window needs a restart.** The exe on disk was rebuilt today
at 15:53 and the window you have open is the older one.

## What changed

**1. The far-away object models are now built from the real models, not from
Bethesda's LOD meshes.**

Until today, the "full detail" the far-field system simplified from was already
a simplified mesh -- on average 47.7 triangles for a whole building. So the first
step down the detail ladder was a simplification of a simplification, and the
numbers said it: that first step only became worth taking at about 90,000 units
away, which is further than anything is drawn. It was correct and it did nothing.

It now starts from the real near model. The same first step is now worth taking
at **4,872 units** -- **18.5 times closer** -- which is inside the first ring.
There is a picture of this: `images/library_near_vs_mnam.png`.

The cost is size. That one nine-chunk test region's object library goes from
**9.7 MB to 225 MB** and its bake from 13 seconds to 83. A full Commonwealth bake
will be much bigger and much slower than the one you have been doing. If that is
not a trade you want, `--library mnam` on the command line puts it back exactly
as it was, and that way back is tested against the previous build.

**2. Trees no longer become stumps.** You said it over one of the ladder pictures
on the 11th. A tree's shape is in its texture cutout, not in its two triangles,
so a simplifier that scores it by geometry throws away the tree and keeps the
stick. Anything using a see-through tree or bush material is now left alone --
2,849 of them in the test region.

**3. A new rule stops any level that loses its outline.** Each step down the
ladder now has to keep at least 70 percent of the original's silhouette, checked
from eight directions around the horizon. If a step drops below that, the whole
step is thrown away and that shape stops simplifying. On the test region it threw
away 5,683 steps across 3,266 models.

The 70 percent is not a guess. A second, separate program measures the same thing
from the finished file: a real kept step holds 73 percent at its worst, and a
deliberately broken one holds 26 percent. 70 sits in the gap. Picture:
`images/silhouette_floor.png`. If you want it stricter or looser it is one
number on the command line, and 0 turns it off.

**4. Your impostor question from the 11th is answered, and the answer was no.**
You asked whether vertex ambient occlusion was baked into impostors on top of the
texture AO they already carry. It was not -- worse, every card-drawn placement was
being written down as *fully lit*, because the number came from averaging the
mesh vertices, and a card has no mesh vertices. A tree under a bridge read exactly
the same as a tree in an open field.

Each placement now gets its own measured value, from a ray cast straight up
against the assembled scene and the landscape, the same way the ground vertices
get theirs. Picture: `images/placement_ao.png`.

## The one thing you have to know before you bake

**Every object library file on disk from before today is refused and has to be
re-baked.** The file says so by name when it is opened -- it names the four bytes
that changed meaning. This is deliberate: those four bytes used to be zeros and
are now a triangle count, and a reader that guessed would silently read zero
triangles for every building. There was no way to make that one safe.

The instance files (the ones that say where things are placed) are not affected
that way.

## What was checked

The three existing test suites all still pass and two of them got bigger: the
format suite went from 108 checks to 120 with no failures, the "nothing else
moved" comparison still reads 25 files and 0 differences, and the defaults check
still reads 28 and 0. A new suite of 22 checks covers the four changes above and
their four ways back.

One test suite has a known red that is not from this work: the far-field picture
matches the chunk file to 82 percent where the bar is 95. It reads exactly the
same on today's build and on yesterday's, so nothing here moved it.

That same suite also keeps a library file baked on the 12th sitting on disk, and
that file is now refused for the reason above. Pointed at a freshly baked file
instead, the suite reads 1 failure -- the 82-percent one -- and everything else
green, so the refusal is the only thing the old file was causing.

Nothing was committed. Nothing was sent to anyone.
