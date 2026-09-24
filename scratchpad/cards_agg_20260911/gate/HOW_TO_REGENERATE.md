# Regenerating the gate trees (they are large and only two are kept)

The tile-1024 REFERENCE tree was 288 MB and is not kept; two cells' sheets are,
under `gate/ref_keep/`, which is what the pictures were drawn from. Everything
here is one command each, from the repo root, with **ABSOLUTE paths** (a
relative `--impostors` finds no card set — `nifskope-ww-lodgen`'s relative-path
trap).

```
R=E:/Projects/NifskopeWildWastelandEdition
G=$R/scratchpad/cards_agg_20260911/gate
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
CARDS=$R/scratchpad/cardwidth_20260910/cards

# the SUBJECT, the shipped defaults, 9-chunk Sanctuary          (kept: gate/agg)
./release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region -20 24 -9 35 --dim 4 --data-root "$DATA" \
  --out-dir $G/agg --native $G/agg/Terrain --impostors "$CARDS" --trees-only \
  --aggregate --aggregate-min 8 --aggregate-tile 64

# the same with the module OFF, for byte identity           (gate/noagg)
./release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region -20 24 -9 35 --dim 4 --data-root "$DATA" \
  --out-dir $G/noagg --native $G/noagg/Terrain --impostors "$CARDS" --trees-only

# the RUNG's own copy of that, for the same comparison        (gate/rung)
./release/NifSkope.before_cards_agg.exe -no-gui lodgen ... (the same line)

# the picture gate's SUBJECT and REFERENCE, one chunk         (gate/subj, gate/ref)
#   subject  --aggregate-tile 64     ~2 s
#   reference --aggregate-tile 1024  ~85 s, 288 MB
./release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region -20 24 -17 27 --dim 4 --data-root "$DATA" \
  --out-dir $G/subj --native $G/subj/Terrain --impostors "$CARDS" --trees-only \
  --aggregate --aggregate-min 8 --aggregate-tile 64
```

## The two standalone tools (deleted after use; ~30 s each to relink)

Both link against Qt6Core alone and cost no NifSkope build slot
(`ww-standalone-writer-gate`). Run them FROM THE MSYS2 SHELL — Qt6Core.dll is on
its PATH, not Git-Bash's.

```
# census.exe -- the forested-cell census, from the ESM alone
g++ -std=gnu++2a -O2 -fexceptions -mthreads -DUNICODE -D_UNICODE -DWIN32 \
  -DQT_NO_DEBUG -DQT_CORE_LIB -Isrc -Isrc/io -Ilib/libfo76utils/src \
  -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtCore \
  scratchpad/cards_agg_20260911/census_main.cpp src/esmdata.cpp \
  lib/libfo76utils/src/{esmfile,filebuf,common,zlib}.cpp \
  -LC:/msys64/ucrt64/lib -lQt6Core -o scratchpad/cards_agg_20260911/census.exe

# aggfixture.exe -- the .lodi v4 known-answer fixture and its mutator
g++ -std=gnu++2a -O1 -w -fexceptions -mthreads -DUNICODE -D_UNICODE -DWIN32 \
  -DQT_NO_DEBUG -DQT_CORE_LIB -Isrc -Isrc/io -Ilib -Ilib/meshoptimizer/src \
  -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtCore \
  scratchpad/cards_agg_20260911/aggfixture_main.cpp src/lodifile.cpp \
  src/lodofile.cpp src/io/lodvfile.cpp lib/meshoptimizer/src/*.cpp \
  -LC:/msys64/ucrt64/lib -lQt6Core -o scratchpad/cards_agg_20260911/aggfixture.exe
```

Only `gate/ref_keep/*_d.DDS` is kept from the tile-1024 reference: the picture
gate reads the colour sheet's alpha and nothing else of it.
