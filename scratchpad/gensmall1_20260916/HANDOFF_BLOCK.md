NifSkope was rebuilt today at 13:52:56, so if you have it open, close and reopen it — the window you are looking at is the older build.

Five small things landed, all in the far-terrain generator.

A loose `Data` folder can now serve `.pbrm` and `.lodm` files. Before today those two only worked from inside a packed `.ba2`, so materials sitting loose beside a model were skipped without a word. Nothing else about which files get served changed, and a full bake of Sanctuary comes out byte for byte the same as it did before.

The far-object files now carry a count of how many things cast a shadow, split by what they are: trees, cards, meshes, or nothing at all. On the test region that reads 3,446 trees and 80 meshes out of 3,526 placements. The two shadow timings the engine owns — the terrain march and the far shadow map — are written down as engine numbers rather than guessed at, because a bake cannot time a frame.

The height layer for the far terrain is now listed in the command-line help. It was already in the program and already had a box in the LOD panel, it just was not in the list anyone reads. It stays off unless you ask for it, because it more than doubles the size of the terrain pyramid.

The far-object file reader now checks that each object's stored cell agrees with its stored position. That caught nothing wrong — every file tested is consistent — but it is now checked instead of assumed.

One thing is measured and not fixed, and it is your call. The old-style object files still silently lose about 6% of the objects on a dense chunk — 2,628 of 42,560 on the downtown chunk — because they run out of room and stop, without saying so. The new-style files lose none of them on the same chunk, in the same run. The fix is a counter and one line of output so a person is told, but it changes the old path, so it is not something to do without you.

Three older test problems were also cleared: a terrain test that was reading a texture the wrong way round, a test that needed an old copy of the program that no longer exists, and a missing reference file for the ground-cover test. The ground-cover test still has four real complaints left in it about the grass feature itself; those are separate and still open.
