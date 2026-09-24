#!/bin/bash
# Lane GROUND1 gate A2g: the strength sweep.
#
# The first distribution of the AO byte was taken with a decoder that read the
# cover tiles' BC3 mask sheet as BC1 (see MISTAKES_ENTRIES.md).  Re-measured
# correctly, strength 1.0 moves the region's AO mean from 211.55 to 93.33 and
# puts 276,234 of 1,048,576 texels in the bottom sixteenth.  That is a number
# worth a sweep rather than a shrug, so this bakes the same region at four
# strengths and reports what each one does to the channel.
set -u
W=/e/Projects/NifskopeWildWastelandEdition/scratchpad/ground1_20260912/work
. "$W/bake.sh"
for s in 0.15 0.25 0.50; do
	n="sw$(echo $s | tr -d '.')"
	bake NifSkope.exe "$n" -24 24 -17 31 --terrain-object-ao \
		--terrain-object-ao-strength $s
done
