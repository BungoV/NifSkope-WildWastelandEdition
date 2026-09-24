p = 'scratchpad/lodiv7_20260918/lane_lodiv7_report.md'
s = open(p, encoding='utf-8', newline='').read()

A = "**And it produced a finding that corrects something this lane wrote earlier today.**"
B = "**The lane stops here, as the director instructed**"
i = s.index(A)
j = s.index(B)

NEW = r"""**I then wrote a DIAGNOSIS OF THAT RUN THAT WAS WRONG, and it is retracted here.** I observed that the
fixed `shot()` carries `timeout 600`, that the process outlived it by seven minutes, and that no
`timeout.exe` was present -- and I concluded that GNU `timeout` had signalled the child, that a native
Windows GUI process started from an MSYS shell ignores that signal, and that the exe was "orphaned from its
own guard". I wrote that into `MISTAKES.md` and into the render-shot skill **as a measurement**. It was not
one. It was a mechanism fitted to two observations, and I never read the process's command line or its
parent chain, either of which refutes it in a single line.

**What was actually true, measured by the director at 18:3x:**

| | |
|---|---|
| bash **33296** | `bash tests/spells/lodi_v7.sh`, started **10:39:04** -- **this lane's own morning run**, still alive after the first wedged NifSkope was ended. bash reads a script **incrementally**, so that shell had parsed the OLD `shot()` at 10:39 and was still holding it; when the first exe died it went on to the next loop iteration with the old function |
| bash **40508** | a subshell of 33296 -- not an orphan anyone had lost, a child of my own morning run |
| NifSkope **24828** | launched by that old shell with the command line `NifSkope.exe --port 42947` and **no scene file**. The fixed `shot()` never ran for it, so its `timeout 600` never existed -- **which is exactly why no `timeout.exe` was found**, and I read that absence as a guard that had fired and given up |
| how it ended | **not killed.** The director sent it `NifSkope::open <the .lodl>` as a UTF-16LE UDP datagram to `127.0.0.1:42947` (`src/main.cpp`, `IPCsocket`). It loaded the scene, the `WW_RENDER_SHOT` hook fired, it wrote `scratchpad/lodi_v7_gate/v7_placement.png` and quit at **18:27:22**. The old shell then launched `v7_sky` (PID 36512, same scene-less form), which got the same treatment and was gone at **18:30:13**; the morning run ended there |

**The two real lessons, which replace the withdrawn one:**

1. **An edited gate script does not fix a run already in progress.** bash parses incrementally, so a live
   `bash <script>` keeps whatever it has already read. My 10:39 fix could not reach the 10:39 shell, and
   that shell kept launching scene-less windows from the old function all afternoon -- which looked exactly
   like the fix failing. Check for a stale `bash <script>` **by name**, not only for the exe it launched.
2. **A harness NifSkope wedged waiting for a scene can be unwedged by sending it the scene over its own
   `--port`**, with no kill and no permission prompt. `IPCsocket::sendCommand` is a bare UDP datagram of the
   command string's raw UTF-16LE bytes to `127.0.0.1:<port>`; `execCommand` acts on `NifSkope::open <path>`.
   That is the tool of choice whenever process-kill is denied, and I had it available all afternoon and did
   not reach for it.

Both files have been corrected: a new `MISTAKES.md` entry at the top dated 2026-09-18 18:3x carries the
retraction, the withdrawn block inside the 10:39 entry now says so and points at it, and the skill's guard
list keeps `timeout` **as written with nothing measured for or against it**, keeps the PID-record-and-verify
step explicitly as a PRECAUTION rather than a fix, and gains the stale-shell rule and the IPC unwedge recipe.

**The two PNGs that run left** -- `scratchpad/lodi_v7_gate/v7_placement.png` and `v7_sky.png` -- came from a
process started **without `WW_LODL_SHEETS` and without `WW_LODL_REGION`**, so they are pictures of the right
file under the wrong conditions and are **not gate evidence**. **Decision: they are deleted**, before the
real G4 runs into the same directory, precisely because a picture that looks plausible and was made under
unknown conditions is the thing most likely to be quoted later as though it had been.

"""

s = s[:i] + NEW + s[j:]
open(p, 'w', encoding='utf-8', newline='').write(s)
print('report 8a retraction written')
