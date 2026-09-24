
## 2026-09-10, lane HKXEDIT1 (the raw .hkx block layer)

**1. A Bash heredoc halved the backslashes, again -- the fifth time this ledger
records it.** A patch to `tests/spells/hkxfile_oracle.py` was typed as a
`python - <<'EOF'` heredoc; the `"\\n"` inside a print became a real newline
in the file and the oracle stopped parsing. Found by the next run's
`SyntaxError`; repaired with the Edit tool. The rule already stands (every
script and every multi-line text through the Write tool, no exception); the
same lane's later patches went through Edit/Write and cost nothing. Repeating
an entry that is already in this file is its own entry: this is one.

**2. `cat -A` through the tool was trusted about line endings.** `cat -A` on
`src/nifskope.cpp`'s load() region printed `$` without `^M`, so the hook-up's
four anchors for that file were written LF. `hookup.py --check` counted 0 for
all four; Python byte counts (`b.count(b"\r")` = 9,520, and `b[i+len(key):]`
starting `\r\n`) showed every region is CRLF. The rule (CONSTITUTION 8:
endings by Python byte counts ONLY) already existed; the tool's `cat -A`
output is one more thing that lies. Fixed by a per-file EOL table in the
script; the check then counted 1 for every anchor.

**3. A gate wrapper passed `#2.numFrames=24` to bash unquoted.** Bash read
`#...` as a comment, the C++ `edit` command got no `--set` arguments, wrote an
unedited file and printed `wrote ... 12288 bytes` -- and the gate read that
line as success. Found because the per-field diff (the gate's own floor)
reported ZERO changed fields for the C++ file. The rule: quote every argument
you hand to a shell, and a "wrote" line is intent, not proof -- the reload is.

**4. The initializer emulator declared 30 classes enum-less and their
signatures wrong, and only the independent hash said so.** The first version
tracked `[rsp + X]` stores only; 61 initializers copy `rsp` to `rax` BEFORE
`sub rsp, 0x78` and store through `[rax - 0x10]`, so their enum pointer and
count read as zero. Every other field was plausible and the self-check was
green. The signature cross-check against HKXPACK (a hash over every field,
computed from the exe's own `writeSignature` disassembly) went 878/908, and
the 30 misses named the shape. Rule: an extractor's plausibility checks are
not a gate; a hash that an INDEPENDENT source also stores is.

**5. The prior lanes' skills were loaded but one procedure was still typed by
hand.** The syntax-check flag list was copied out of HKX1's `build_dump.sh`
into a new `sx.sh` and a new `build_gate.sh` rather than pointed at; the third
copy of the same 900-character flag string in the tree. Not wrong, but the
kind of re-typing rule 1a exists for; noted in the skill review rather than
fixed, because the fix (one shared `scratchpad/flags.sh`) belongs to whoever
next touches all three.
