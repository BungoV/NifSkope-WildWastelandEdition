# 2026-09-18 18:3x. RETRACTION. The 18:2x "correction" was itself wrong: it
# diagnosed a `timeout` failure that was never measured, because the parent chain
# of the wedged process was never read. Removing the claim from both files and
# replacing it with what the director measured.

RETRACTION_ENTRY = r"""## 2026-09-18 18:3x -- a diagnosis written before the parent chain was read, and it was wrong

- 2026-09-18 18:2x (lane LODIV7) -- a NifSkope launched at 18:04:59 was still
  resident 17 minutes later with no `timeout.exe` process beside it. I wrote into
  this ledger and into `.claude/skills/nifskope-ww-render-shot/SKILL.md` that
  **GNU `timeout` had signalled the child, that a native Windows GUI process
  started from an MSYS/Git-Bash shell does not act on that signal, and that the
  exe was therefore "orphaned from its own guard"**, and I called that a
  measurement.
- **It was not a measurement. It was a story fitted to two observations**, and it
  is retracted in full. I never read the process's parent chain and I never read
  its command line; both were available with one `Get-CimInstance` call.
- What was actually true, measured by the director:
  1. bash **33296** was `bash tests/spells/lodi_v7.sh` started **10:39:04** --
     the lane's ORIGINAL morning run, still alive after the first wedged NifSkope
     was ended. **bash reads a script incrementally**, so that shell had parsed
     the OLD `shot()` at 10:39 and was still holding it. When the first exe died
     it simply went on to the next loop iteration, still calling the old,
     scene-less function.
  2. PID **24828** was launched by that old shell (through subshell 40508) with
     the command line `NifSkope.exe --port 42947` and **no scene file**. The
     fixed `shot()` never ran for it. Its `timeout 600` never existed for this
     process -- which is the entire reason no `timeout.exe` was found, and I read
     that absence as a guard that had fired and given up.
  3. It was ended **without killing anything**, by sending it the scene it was
     waiting for over its own port (below). It loaded, the render hook fired, it
     wrote its picture and quit at 18:27:22.
- How the error was made, which is the part worth keeping: **the exe's own
  command line refutes the whole story in one line, and I never looked at it.**
  I had two facts, a plausible mechanism joined them, and I wrote the mechanism
  down as measured -- in a ledger whose entire purpose is to separate those two
  things. The house rule was already on the wall: an invariant that fails on
  broken code, and proof that it fails.
- The rules, and both are cheap:
  * **Before diagnosing any wedged or unexpected process, read its command line
    and its parent chain.**
    `Get-CimInstance Win32_Process -Filter "ProcessId=<pid>" | Select-Object CommandLine,ParentProcessId`
    walks it. A process's own arguments say which code path launched it; an
    inference about which code path launched it is a guess with a confident
    voice.
  * **Editing a gate script does not fix a run already in progress.** bash parses
    incrementally, so a long-running `bash foo.sh` keeps whatever it has already
    read. A fix applied to the file while the old shell is alive changes nothing
    for that shell, and its output will look like the fix failing. **End the old
    run before believing anything about the new script**, and check for a stale
    `bash <script>` by name, not just for the exe it launched.
- The `timeout` guidance that stood on that claim is withdrawn from both files.
  What replaces it is in the skill: the unwedge-by-IPC recipe below, which is a
  thing that was actually done and worked.

"""

p2 = 'MISTAKES.md'
s = open(p2, encoding='utf-8', newline='').read()
HEAD = "Newest at the top.\n\n"
assert s.count(HEAD) == 1
s = s.replace(HEAD, HEAD + RETRACTION_ENTRY)

# ---- withdraw the wrong block from the 10:39 entry ---------------------------
BAD_START = "\n\n**CORRECTION, 2026-09-18 18:2x, and it corrects part (2) of the rule above rather"
BAD_END = "the second time in one day."
i = s.index(BAD_START)
j = s.index(BAD_END) + len(BAD_END)
GOOD = r"""

**RETRACTED AND REPLACED, 2026-09-18 18:3x -- see the entry at the top of this
file.** A block stood here claiming `timeout` had fired and been ignored by a
Windows GUI process. It was never measured and it was wrong: the process it
described was launched by the lane's OWN morning shell, which had parsed the old
scene-less `shot()` before the fix was written, so it ran with no scene file and
no `timeout` at all. Part (2) above stands as written -- wrap the launch in
`timeout` -- with nothing measured for or against it by this lane.

**And the fix above did not take effect when it was written, for a reason that is
its own lesson:** the morning's `bash tests/spells/lodi_v7.sh` was still alive,
and bash reads a script incrementally, so that shell kept the old function it had
already parsed and went on launching scene-less windows from it. **Editing a gate
script does not fix a run already in progress.** End the old run first, and look
for a stale `bash <script>` by name rather than only for the exe it launched.

**What to do with a harness NifSkope that is already wedged waiting for a scene,
and it does not involve killing it:** send it the scene over its own `--port`.
`src/main.cpp`'s `IPCsocket::sendCommand` is a bare UDP datagram of the command
string's raw UTF-16LE bytes to `127.0.0.1:<port>`, and `execCommand` acts on
`NifSkope::open <path>`. The process loads the file, `completeLoading` fires the
`WW_RENDER_SHOT` hook, it writes its picture and quits. That is how PID 24828 was
ended at 18:27:22 and PID 36512 at 18:30:13, with no kill and no permission
prompt -- and it is the tool of choice whenever process-kill is denied."""
s = s[:i] + GOOD + s[j:]
open(p2, 'w', encoding='utf-8', newline='').write(s)
print('MISTAKES retraction written')

# ---- the skill ---------------------------------------------------------------
p = '.claude/skills/nifskope-ww-render-shot/SKILL.md'
k = open(p, encoding='utf-8', newline='').read()
O1_START = """2. **Wrap it in `timeout` -- and know that `timeout` is NOT enough on its own.**"""
O1_END = """   and closed on `PENDING.md` twice."""
a = k.index(O1_START)
b = k.index(O1_END) + len(O1_END)
N1 = r"""2. **Wrap it in `timeout`.** `timeout 600 "$EXE" --port "$PORT" "$(winpath "$LODL")"`.
   A stuck window is not a failed test, it is a failed TREE.

   As a precaution beside it -- not as a fix for anything measured -- **record the
   PID at launch and check after the wait that it is gone**: launch with `&`, keep
   `$!`, and afterwards look for the process by the `--port` you gave it, which is
   the only thing that tells your harness's window from bungo's. A driver that
   finds it still alive and cannot end it prints the PID and the port and stops,
   because the one-instance rule makes every later gate refuse anyway.

   **(An earlier version of this section claimed `timeout` had been measured to
   fire and be ignored by a Windows GUI process. That was wrong and is withdrawn;
   the process in question was launched by a stale shell with no `timeout` at all.
   See `MISTAKES.md` 2026-09-18 18:3x.)**

3. **A fix to this script does not reach a run already in progress.** bash reads a
   script incrementally, so a long-running `bash tests/spells/<gate>.sh` keeps
   whatever it has already parsed. Edit the file while the old shell is alive and
   that shell keeps launching the OLD, broken form -- and its output looks exactly
   like your fix failing. **Before trusting a fixed gate, check for a stale
   `bash <script>` by name**, not only for the exe it launched, and end it.

4. **Unwedging a harness NifSkope WITHOUT killing it: send it the scene over its
   own `--port`.** This is the tool of choice when process-kill is denied, and it
   is how two wedged windows were ended on 2026-09-18 (PID 24828 at 18:27:22, PID
   36512 at 18:30:13) with no kill and no permission prompt.

   `src/main.cpp`'s `IPCsocket` binds `127.0.0.1:<port>` when the exe is started
   with `--port`, and `IPCsocket::sendCommand` is nothing more than a UDP datagram
   carrying the command string's **raw UTF-16LE bytes** -- no length prefix, no
   terminator:

   ```cpp
   udp.writeDatagram( (const char *)cmd.data(), cmd.length() * sizeof( QChar ),
                      QHostAddress( QHostAddress::LocalHost ), port );
   ```

   `execCommand` acts on anything starting `NifSkope::open`, so the datagram to
   send is the string `NifSkope::open <absolute windows path>` encoded UTF-16LE.
   The process loads the file, `completeLoading` fires the `WW_RENDER_SHOT` hook,
   it writes its picture and quits on its own.

   **The picture it writes is NOT gate evidence** unless that process was started
   with the whole environment the gate specifies. A window unwedged this way had
   no `WW_LODL_SHEETS` and no `WW_LODL_REGION`, so what lands is a picture of the
   right file under the wrong conditions. Unwedge to free the tree, then re-run
   the gate properly."""
k = k[:a] + N1 + k[b:]

# the old part (3) is now part 5
OLD3 = "3. **Assert the artefact after the launch**, in the driver:"
assert k.count(OLD3) == 1
k = k.replace(OLD3, "5. **Assert the artefact after the launch**, in the driver:")
open(p, 'w', encoding='utf-8', newline='').write(k)
print('skill retraction written')
