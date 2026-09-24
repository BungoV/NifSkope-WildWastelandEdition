# ESMFIX1 progress
## 21:56 setup
- Worktree objects copied from NifskopeWWE-loadorder1 (same commit 541bbe5, its make -n = 0 compiles); .qmake.stash from there; qmake rc 0 (125 esmfix1 paths, 0 loadorder1).
- Rung build 21:50:06 rc 0, release/NifSkope.before_esmfix1.exe sha1 b4f62763.
## measure
- TestWorldspace.esp: flags 0 (plain .esp, not ESM/ESL/localized), form version 131, HEDR 1.0 / 923 / next 10416, 1 master Fallout4.esm; 483 records, ALL top byte FF (WRLD, CELL 211, LAND 131, ACHR 121, REFR 13, ...).
- xEdit TwbFile.FileFileIDtoLoadOrderFileID: FullSlot < MasterCount -> that master, else the file's own slot. The game resolves FF here to TestWorldspace itself; nothing is ignored, so no skip/warning path.
- census.py over his 47 plugins: only TestWorldspace has record top bytes > master count (483); every one of them rung-refused.
## 21:56 fix built
- esmfile.cpp: form version < 0xC0 -> every index >= master count maps to the file itself, and the raw-ID refusal is off; FO76/Starfield unchanged. Build 21:55:01 rc 0 (esmfile.o only), sha1 ec5959c4.
