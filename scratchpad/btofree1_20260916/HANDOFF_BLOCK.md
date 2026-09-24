The LOD bake for FO4 Community Shaders no longer leaves the old `.BTO` chunk files in your mod folder.

Your NifSkope window is running the older program. Close it and open it again to get this.

What you get now when you bake with the FO4 Community Shaders target picked: the mod folder holds
our own five file types, the texture and card sheets, the heightmap, and the small text file that
sits beside each chunk. The chunk files themselves are gone from it.

They still get made. Five steps of the bake -- the texture sheets, the atlas, the shape merge, the
far-ring trim and the card sheets -- read a chunk back after it is written, so the bake still needs
one per chunk. It now makes them in a folder called `lodgen_bto_scratch` inside your mod folder,
uses them, and deletes them and the folder at the end. If a bake is interrupted you will see that
folder sitting there; the next bake clears it out by itself.

If you want the old behaviour back, tick **Keep legacy .BTO chunks** in the Object modules section of
the LOD Generation panel (it only appears under the FO4 Community Shaders target, and it starts off).
On the command line it is `--keep-bto`. Either one puts the chunks back in the mod folder exactly as
before, down to the byte.

Picking the Stock engine target changes nothing at all. That bake writes exactly the same files it
always did, and we compared every one of them against the previous program to be sure.

The line under the progress bar now tells you what happened: how many chunks were built, how many
were removed, and how many bytes that saved.

On the one test chunk that number was 860,743 bytes for a single chunk; a whole worldspace is
thousands of chunks, so a full bake stops writing a few gigabytes into your mod folder.

There is a before-and-after picture of the mod folder at
scratchpad/btofree1_20260916/pictures/mod_folder_before_after.png.
