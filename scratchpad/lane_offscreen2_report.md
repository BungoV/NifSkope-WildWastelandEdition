# lane OFFSCREEN2 -- a headless run is invisible, and never on his main monitor

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, **no commits**.

bungo's two requirements, verbatim, and both have to hold for every headless run:

* *"Agent is launching nifskope on my main monitor, which is a no no"*
* *"the screen is flashing white and black, that's a view hazard for epileptics"*

**Both hold, measured.** `tests/spells/render_shot.sh`: **55 checks, 0 failures**
on `release/NifSkope.exe` 19:35:14. Restart-free interactive use is unchanged:
with no `WW_*` variable in the environment `NifSkope::wwHeadlessRun()` is false
and the maximise-and-raise branch runs exactly as it always did.

## 0. What was inherited, and why it is dead

Lane OFFSCREEN moved the headless window OFF EVERY SCREEN. Lane BUILD2 built it
and measured two failures, both in `MISTAKES.md`:

1. **It was inert.** `restoreUi()` restores a MAXIMISED window and on Windows
   `move()` on a maximised window changes nothing except which monitor it is
   maximised onto. An origin on no monitor changed nothing at all.
2. **And when the maximised bit was cleared, it worked and NOTHING RENDERED.**
   `GLView` is a `QOpenGLWindow`; a surface that is never exposed never gets a
   context, so `grabFramebuffer()` returned a null image. `WW_RENDER_SHOT` wrote
   no PNG; `WW_IMPOSTOR_BAKE` wrote its 110-byte sidecar and no card image; both
   exited 0. A bake that writes EMPTY cards and reports success is worse than
   the hazard.

So "off every screen" is not the property to aim at. The property is
**INVISIBLE OR ABSENT, and NEVER ON THE PRIMARY** -- which is what bungo asked
for, in two separate sentences.

---

## 1. Variant A: on a non-primary screen, at window opacity 0

**It passes, and no Variant B was needed.** `GLView` is a `QOpenGLWindow`
(`src/glview.h:58`), so exposure is the whole question: a layered window at
alpha 0 is still mapped, still gets paint events, still has a context. An
off-screen one is not.

### What is in the code

`src/nifskope_ui.cpp`

* `wwHeadlessWindowOrigin( QString * arm )` -- serves `WW_WINDOW_AT` if that
  point is **not on the primary screen**, else the first non-primary screen's
  top-left `+8,+8`, else `1920,0` when no screen is reported at all, else the
  primary with opacity 0 as the only guarantee. It NAMES the arm it served, and
  a refused `WW_WINDOW_AT` carries `refused-WW_WINDOW_AT-on-primary/` in front
  of the arm that took over (CONSTITUTION 10: a fallback is never silent).
* `NifSkope::wwPlaceHeadlessWindow( QWidget * )` -- the one implementation:
  clear `WindowMaximized|WindowFullScreen`, `move()` to that origin,
  `WA_ShowWithoutActivating`, and `setWindowOpacity( 0.0 )` unless
  `WW_WINDOW_VISIBLE=1`.
* `wwLogTopLevelWindows()` -- the record now splits the one number it used to
  carry into the two questions bungo actually asked, plus the state that made
  the last attempt inert:
  `<when> <class> geom=... visible=0|1 onscreen=0|1 onprimary=0|1 opacity=0.00 maximised=0|1 arm=<...>`.
* `createWindow` -- calls the helper after `restoreUi()`, then `show()`, then
  `move()` once more; the non-headless branch is byte-for-byte the old one.

`src/nifskope.cpp` / `src/nifskope.h`

* the `NifSkope` **constructor** calls the helper before `ui->setupUi()`;
* `NifSkope::eventFilter` calls it on `QEvent::Show` for **every other
  top-level window** the process shows -- a dialog, a floating dock, a tool
  window -- as a backstop.

### Why three call sites, and not one

Because one was measured to be too late. With the placement only in
`createWindow`, an `EnumWindows` probe at 25 ms found a **426x306 OPAQUE window
of the process at 632,249 on the PRIMARY monitor at t=371 ms** -- Qt's default
geometry, class `Qt6111QWindowIcon`, the application title with no filename --
gone by t=1403 ms and replaced by a different HWND, class
`Qt6111QWindowOwnDCIcon`, at the asked-for place with layered alpha 0. The class
change is the mechanism: Qt's Windows plugin picks a window class by whether the
surface needs its own DC, so realising the GL container
(`ogl->createWindowContainer( this )`, `nifskope.cpp:2729`) destroys the first
native window and creates a second -- and the first was created and shown while
the widget still had its default geometry, because `createWindow` does not touch
it until the constructor has returned.

That was a real flash on his main monitor, present in all four hidden runs of
gate run 3, and it is what the constructor call removes. Gate run 4:
`0 of 6-26 sample(s) on the primary` in every run.

### The numbers

`release/NifSkope.exe` **19:35:14**, `release/style.qss` 19:35:19, byte-equal to
`res/style.qss`; both newer than every changed source (swept file by file over
`git status`). `make -j2` RC=0 after `qmake`-free incremental (only two TUs and
one header changed; `nifcli.o` is older than `nifskope.h` but does not include
it -- checked, not assumed).

| | hidden runs | visible control | strobe control |
|---|---|---|---|
| windows on the primary, process's own log | 0 of 4-6 records | 0 of 4 | 0 of 6 |
| windows on the primary, EnumWindows outside | 0 of 6-26 samples | 0 | 0 |
| mapped windows at opacity > 0 | 0 | 2 of 2 | -- |
| layered alpha > 0, Windows' own answer | 0 samples | 11 of 11 | -- |
| **the second monitor's own pixels, luminance range** | **0.233** | 27.1 | **246.8** |

The last row is the one that answers him. A 200x200 region of the second monitor
where the window sits is sampled every ~50 ms; a hidden 4x4 octahedral bake --
148 full repaints, two in every nine alternating a black clear and a white one
-- moved it by **0.233**, which is exactly what that region does with nothing
running at all. The same bake made visible on purpose moved it by **246.8**.

**The pixels do not change.** Same build, same scene, opacity 0 against opacity
1: the render PNG is byte-identical (`fedab869884174fb...`), and so are the six
card sheets of a bake (`3f2db029bde9d94d...`).

**On the real model.** `TreeMapleForest02.nif` from the unpacked FO4 corpus,
`WW_IMPOSTOR_OCT=4 WW_IMPOSTOR_TILE=64`, hidden and visible:

| | hidden | visible |
|---|---|---|
| sheets written | 6 | 6 |
| card-set sha256 | `6db4e81468e37b85...` | `6db4e81468e37b85...` **identical** |
| mapped windows on the primary | 0 | 0 |
| window opacity | 0.00 | 1.00 |
| second monitor luminance range | **0.111** | **251.581** |

Script: `scratchpad/offscreen2_20260909/treebake.sh`.

**Interactive and harness use still work.** `tests/spells/lod_generation.sh`
**97 checks, 0 failures** -- a GUI harness that takes focus, drives widgets and
scrolls a wheel over a number field, on an invisible window.

## 2. Variant B (FBO on a QOffscreenSurface): NOT NEEDED, NOT WRITTEN

Variant A's gate is green on both of its conditions -- byte-identical pixels and
byte-identical cards -- so the brief's condition for Variant B was never met.
`GLView::grabSupersampled` (`src/glview.cpp:20817`) remains the path a future
lane would extend if a driver ever disagrees; nothing about it was touched.

## 3. Gates

`tests/spells/render_shot.sh`, sections 5-6 rewritten, section 0 added
(the desktop's own noise floor), sections 1-4 kept and given one new check each
where the old ones could not fail.

* **The "off every screen" checks are gone**, replaced by *invisible or absent*
  and *never on the primary*, as the brief directs. `onscreen=1` is now
  EXPECTED in a headless run.
* **The refuter for the dead design is kept and strengthened**: section 2 now
  requires the bake to write a card IMAGE, not only its sidecar. That is the
  check that goes red the moment a headless window stops being exposed -- the
  exact failure BUILD2 measured, which the old gate was green on.
* **Anti-vacuity everywhere.** Each run must have recorded a MAPPED window
  before its zeros count; each identity check asserts that the hidden run really
  was hidden before comparing hashes (the 2026-09-09 entry where that
  comparison passed by comparing a picture with itself).
* **Both control runs are placed by the same code as the hidden ones**, on a
  non-primary screen. The gate's own control is how a NifSkope reached his main
  monitor in the first place, and it is no longer exempt.
* **Three instruments, and the pixel one exists because the other two lie by
  construction**: the log and the layered alpha both echo what Windows was TOLD.
  A GL child window presenting over its layered parent would satisfy both and
  still strobe. Only the desktop's pixels can refuse that.
* **Every floor fired in the same run** -- `render_visible` seen by all three,
  and `bake_oct_visible` (a visible `WW_IMPOSTOR_OCT=4` bake, run once on the
  second monitor on purpose) seen alternating at range 246.8 against a bar of 30.

Full logs: `scratchpad/offscreen2_20260909/render_shot_run{1..4}.log`. Run 1 was
the first build, run 4 the final one; runs 2 and 3 are the instrument repairs
below, kept because they are what makes run 4 believable.

`tools/bake_impostor_cards.sh` -- header note rewritten: the strobe still
happens, and why nothing of it is on a monitor.

### What was NOT measured

* An OCT=8 run of a full tree library through `tools/bake_impostor_cards.sh`
  end to end. The driver's candidate machinery belongs to another lane and the
  window behaviour is per-process, identical for one model or fifty.
* Whether another machine's driver honours a layered alpha the way this one
  does. The gate's pixel instrument is what would catch it, and it now ships.
* `release/NifSkope.before.exe` (17:22:05, sha256 `5ca38e28...`) is still in
  `release/`. This lane's comparison is same-build, so the before-exe was never
  needed; it is left for the director to dispose of.

## 4. Mistakes

Five, all written into `MISTAKES.md` the moment they were recognised:

1. **An instrument that picks ONE window was used to prove a claim about ALL of
   them, and I wrote the wrong conclusion into the gate.**
   `Get-Process().MainWindowHandle` reported an opaque window on the primary; a
   probe that had not been running at the time saw nothing; I concluded
   "heuristic error" and put it in `render_shot.sh`'s header as fact. The window
   is REAL -- enumerating found it in every hidden run and named it. Rule:
   enumerate, carry the identity in the record, and do not write a conclusion
   into a comment before running the measurement that would refute it. The
   comment has been taken back out and replaced with what was measured.
2. **A visibility check counted a window that was never mapped.** Qt's internal
   0x0 helper `QWindow` reports `opacity=1.00` because nobody ever set one on
   it; counting it failed eight checks about a window nobody can see.
3. **PowerShell case-insensitivity emptied an instrument.** `$b` (the screen's
   `Bounds`) and `$B` (the window's bottom edge) are one variable; every
   comparison threw and the sampler wrote nothing, which reads exactly like a
   pass. **Only the floor caught it** -- which is the argument for floors, made
   for me rather than by me.
4. **A "the screen did not change" bar was set from a desktop nobody controls.**
   One console print into the sampled region moved it by 5.4 against a bar of
   3.0. Measured amplitudes now quoted in the gate: 0.2 idle, 5.4 a console
   print, 27.4 an opaque window, 250 the strobe; bar 15.
5. **Two probe runs hung with no output** because a relative POSIX path was
   handed to a native exe from Git-Bash. Not the code under test; my typing.

## 5. Finished-work skill review

**Loaded and used.** `nifskope-ww-render-shot` (the hook, the switches, and its
BUILD2 section, which is the reason no time was spent re-discovering that an
off-screen window renders nothing); `nifskope-ww-build-verify` (the gated chain,
make's own exit code, the exe-newer sweep over every changed file, the
stylesheet `cmp`, the stale-object rule after a header change -- which is how
`nifcli.o` was checked rather than assumed, and it is not a real dependency).

**Amended, in BOTH trees** (`E:\Projects\Claude\.claude\skills\...` and the repo
`.claude\skills\...`, byte-identical, `cmp` clean): `nifskope-ww-render-shot`.
Its first section said, as fact, that the fix was inert and the strobe was live.
That is now false, and it is the file every lane in this repo reads before a
bake. It now opens with what is true as built, keeps BOTH dead ends so nobody
pays for them again (off-every-screen renders nothing; placing in `createWindow`
alone is too late, with the 371 ms measurement and the window-class mechanism),
and adds a section on measuring "nothing was on the screen" without fooling
yourself: enumerate rather than take one handle, read the layered alpha back,
sample the desktop's own pixels because the first two only echo what Windows was
told, and put a floor beside every zero. The repo tree had no copy of this skill
at all; it does now.

**Declined, with the reason.** A separate skill for "prove a Windows GUI process
shows nothing on a monitor". It is a genuinely repeatable procedure and it cost
this lane two gate runs to get right -- but every step of it is now written in
`nifskope-ww-render-shot`'s measuring section, which is where a lane doing this
work will actually meet it, and a second copy is the thing that drifts (the
same reasoning lane OFFSCREEN used for its line-ending splice). If a
non-NifSkope project ever needs it, that section is what to lift.
