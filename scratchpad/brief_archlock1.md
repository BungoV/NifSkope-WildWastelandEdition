# Lane ARCHLOCK1 -- hotfix: the archive-index lock deadlocks the GUI thread on itself

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch: read `release/NifSkope.exe`'s mtime and size
  yourself and write them in the report's first line (queue: ... -> LAYOUT1 -> BAKEREC1 -> ARCHLOCK1 (you) -> INCR1 -> PERF1
  -> AUDIT1, one lane at a time). Rung ONCE before your first build: `release/NifSkope.before_archlock1.exe` (never delete
  any `release/NifSkope.before_*.exe`). Markers `scratchpad/archlock1_20260917/BUILDING` (touch FIRST) / `DONE` (first word
  `archlock`). Report `scratchpad/archlock1_20260917/lane_archlock1_report.md`, INCREMENTAL; `PENDING.md` past half
  context. Never commit, never `git stash`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command, run and read before every build and every
  exe launch; Fallout4 up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with no `--port` is bungo's own window: rename the
  exe aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it; never rename or delete a `NifSkope_inuse_*.exe` you did
  not create. A GUI harness is `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time, second monitor only. Every path in
  argv and every WW_* path ABSOLUTE `E:/...`.
- You are the ONLY lane in the tree. No mutex needed; leave none behind. ONE background waiter at a time.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (the RED 00:53 2026-09-17 line = your ruling); `MISTAKES.md` (root,
  top entries); `src/gamemanager.h` lines 90-110 (the NIFPARSE1 comment on `archiveLock()`); `src/gamemanager.cpp`
  `init_archives` / `close_archives` / `find_file` / `get_file` / `list_files` / `addNIFResourcePath`.
- Skills (repo `.claude/skills`): `nifskope-ww-build-verify`, `ww-test-harness-add`, `ww-panel-run-harness` (section 9:
  run a GUI harness from Git Bash, an MSYS2 bash drops the parent shell's variables), `ww-spec-gate-audit`.

## bungo's words, verbatim (2026-09-17 00:4x)
- "When I click on anything from 'files', it freezes nifskope" (his own window: 0.3.3 build 720762a, Files tab filtered
  "armor", `Loaded files . 0`, title `m_arm_heavy_l.nif`, "(Not Responding)").

## What the director measured (00:43-00:50 2026-09-17, no build, gdb attached to his frozen window pid 15984 and detached)
- CPU 0.0 s over 5 s, 9 threads, none of them a Qt worker: a wait, not work. Main thread, resolved against a symbol
  re-link of the SAME 250 objects (code bytes identical at every frame; scratch only, nothing in the tree):
  `GLView::paintGL` -> `Scene::make` -> ... -> `BSShaderLightingProperty::setMaterial` -> `ShaderMaterial::ShaderMaterial`
  -> `Material::openFile` -> `NifModel::getResourceFile` -> `GameResources::get_file`+168 -> `GameResources::get_file`+200
  -> `GameResources::init_archives`+777 -> Qt6core -> libwinpthread wait.
- Mechanism (`src/gamemanager.cpp`): `get_file` takes `QReadLocker archiveReadLock( &archiveLock() )` (line ~309), finds
  nothing in the document-local index, and while STILL HOLDING that read lock recurses `return parent->get_file( ... )`
  (line ~315). The parent (`archives[game]`, the shared game index) had no `ba2File` yet, so its `get_file` calls
  `init_archives()`, whose first line is `QWriteLocker archiveWriteLock( &archiveLock() )`. `archiveLock()` is ONE static
  `QReadWriteLock( Recursive )`; recursive mode allows read-after-read and write-after-write by the same thread, never a
  read -> write UPGRADE: `lockForWrite` waits for the reader count to reach zero, and the reader is the waiting thread
  itself. The GUI thread sleeps forever; nothing can wake it.
- `find_file` (line ~285-296) has the same shape: read lock held across `return parent->find_file( fullPath )`.
- When it bites: a document-local `GameResources` with an EMPTY data path (`addNIFResourcePath` with `dataPath` empty:
  `dataPaths` stays empty, so its own `init_archives()` never runs and its `ba2File` stays null) whose first material /
  texture lookup happens before anything else built the parent's index (a fresh window, `Loaded files . 0`). A document
  that has its own data path builds the parent from its own `init_archives()` (line ~184) with no read lock held, which is
  why most opens survive. The retry in `src/gl/glproperty.cpp:1128-1135` (closes the child AND the parent, then looks up
  again) walks into the same trap on purpose.
- Introduced by NIFPARSE1 (2026-09-11, uncommitted, `git status` shows `src/gamemanager.cpp` modified against 5388eda);
  every exe since carries it, including every `release/NifSkope.before_*.exe` from that date. NOT LAYOUT1's.

## Landed since this brief was written (read before "The work")
- 01:02 2026-09-17 the director APPLIED step 1's two-line fix in the tree (`src/gamemanager.cpp`, two sites tagged
  ARCHLOCK1, in `find_file` and `get_file`), bungo: "I need a fix for the freeze in nifskope now" / "Replace my exe now".
  BAKEREC1 then landed 01:17: `release/NifSkope.exe` 22,459,904 B 01:17:01 CARRIES that fix. So the rung you take
  (`release/NifSkope.before_archlock1.exe` ALREADY EXISTS: BAKEREC1's 01:00:05 exe WITHOUT the fix, 22,459,904 B; do not
  overwrite it, it is your refuter exe) and the exe on disk differ only by the fix. Your step 1 becomes: keep the two
  sites, do the audit of every OTHER parent recursion / lazy init under the read lock, and add the header comment.
- The one existing GUI probe (a loose `.nif` given as argv) does NOT reproduce the hang on the rung: that route opens.
  bungo's route is the Files tab's configured-resource row: `openConfiguredNif` -> `loadConfiguredNifIntoDocument`
  (`src/nifskope.cpp` ~8620-8660) loads the bytes through a QBuffer, so `NifModel::load` sees NO file name,
  `getNIFDataPath` gives an empty data path and the document-local `GameResources` never builds its own index. Your
  refuter (gate (a)) must go through THAT route: extend the WW_FILESTAB_TEST harness (`src/filestabtest.cpp`,
  `tests/spells/files_tab.sh`, seam `wwFilesTabOpenRow`) or add a sibling seam that puts a CONFIGURED-RESOURCE row for a
  `.nif` whose `.bgsm` lives only in the game archives and opens it exactly as a double-click does, in a FRESH process
  whose shared index nothing built first. It must hang the rung (watchdog kill = the passing refuter row) and open on the
  new exe.
- BAKEREC1's gates are PENDING on the 01:17 exe (Fallout4 was up 01:14-01:17; it is down now). Since the exe you gate
  is that one, run `OUT=<dir under your folder> bash tests/spells/lodgen_bakerec.sh` legs (a)-(h) in full and
  `tests/spells/lodgen_layout.sh`, `lodgen_defaults.sh`, `lodgen_native.sh`, `lodgen_btofree.sh` on it BEFORE your own
  build, and again on your exe after; table both runs (name, checks, failures, seconds). Its leg (e) picture
  (`--native-verify` naming a plugin) is owed: put it in `scratchpad/bakerec1_20260916/images/`.
- One more row, BAKEREC1's finding: in batch mode NifSkope resolves relative paths against its own folder
  (`src/nifcli.cpp:179`, deliberate, how `nif.xml` is found), so `lodgen --bake-record scratchpad/.../x.lodb` is refused
  while the file exists. Add the directory it looked in to that refusal's wording (one clause, no behaviour change) and a
  gate row that shows the message names the directory.
- Ordering: gates on the 01:17 exe FIRST (before any build of yours), then the fix audit + header comment + the refusal
  clause, then `BUILDING`, build, gates again. Game check before every build and every exe launch, as the header says.

## The work
1. Fix: in `get_file` and `find_file`, release the read lock BEFORE recursing to the parent (`archiveReadLock.unlock()`
   right before `return parent->...`; with `fd == nullptr` no `string_view` into the index is alive, so nothing is lost).
   Do the same audit for every other path that recurses to `parent` or calls `init_archives()` / `close_archives()` with a
   read lock held (`list_files`, `init_materials`, `close_materials`, the glproperty retry) and list each site in the report
   with the verdict "held" / "not held". Keep the lock; NIFPARSE1's reason for it (sixteen chunk workers under one index)
   stands. Comment the rule at the lock's declaration in `src/gamemanager.h`: "never recurse to parent under the read lock;
   the parent's lazy init takes the write lock".
2. **Gates** (`tests/spells/gamemanager_archlock.sh`, new, with `ww-test-harness-add`): (a) REFUTER on the rung: a
   GUI harness (`--port <unused>`, `WW_WINDOW_AT=1960,40`, a WW_* switch of yours that opens ONE loose `.nif` given by
   absolute path whose material `.bgsm` / textures live only in the game archives, as the FIRST open of the process, with
   the FO4 data path configured through the harness's own isolated QSettings, `feedback_harness_isolate_settings`) run
   against `release/NifSkope.before_archlock1.exe` must HANG: the harness's own watchdog (a `timeout 60` or a PowerShell
   `Wait-Process` with limit) kills it and the gate records "hung, killed after N s" as the PASSING refuter row; the same
   command against the new exe loads the file, paints one frame and exits 0 within the limit; (b) the CLI path: any
   `-no-gui` verb that reaches `get_file` through a document-local resource set with an empty data path (find one, e.g. a
   `lodgen` verb over the fixture, or add a tiny `--ww-archlock-probe <nif>` verb that does exactly `getResourceFile` for
   the nif's first material and prints found/not-found), same refuter shape: rung hangs (killed), new exe answers;
   (c) `find_file` covered the same way (a texture lookup); (d) the chunk-worker case NIFPARSE1 built the lock for still
   holds: `tests/spells/lodgen_native.sh` and `lodgen_defaults.sh` PASS on the new exe, byte-identical fixture output
   against the rung on (-20,24); (e) the panel self-test PASS. Record every count and every wall time.
3. Pictures: one screenshot of the harness window with the loose `.nif` rendered on the new exe (`scratchpad/
   archlock1_20260917/images/`), and the terminal capture of the rung hanging and being killed (the refuter).
4. Docs: `docs/MISTAKES.md` lodgen/resource section: one entry, what let it through (no gate opened a document with an
   empty data path in a fresh process before the parent index existed). `MISTAKES.md` root: your own mistakes only.
5. Report + changelog text for the director to splice (WW_CHANGES entry + HANDOFF LANDED block; you edit neither). Then
   `DONE` with first word `archlock` and the one-line verdict: refuter hung N s on the rung / passed on the new exe, every
   gate's count, the exe's mtime/size.
