## 2026-09-16 17:0x -- lane BTOFREE1 (the FO4CS target stops writing `.BTO`)

1. **Hit the backslash trap AGAIN, on the third day it is in this ledger, by
   reaching for `python -c` instead of a file.**
   Reading a `.lodo` header meant turning a Git-Bash path into a Windows one, so
   I typed `python -c "p=r'...'.replace('/','\\\\')"`. The shell collapsed the
   escape, Python received a string ending in a lone backslash, and the error was
   `EOL while scanning string literal` -- which points at the quoting, not at the
   path. The ledger's own top entry (NATIVE1c, 16:2x) is the same mistake, and it
   says in so many words that the rule covers `python -c` as well as heredocs.
   **How it was found:** the interpreter refused to start, so it cost seconds
   rather than a build -- but only by luck, since the same collapse inside a
   patch anchor is silent.
   **The rule, restated so the next lane cannot read past it:** if a command
   contains a backslash or an apostrophe, it is written to a file with the Write
   tool and run from there. No heredoc, no `python -c`, no exceptions, and the
   patch script asserts `count == 1` on every anchor so a silent collapse becomes
   a refusal instead of a wrong edit.

2. **Wrote the same teardown loop twice -- once in `nifcli.cpp`, once headed for
   `lodgenmanager.cpp` -- before noticing that a gate in this tree exists
   precisely to catch the two drifting apart.**
   The `.BTO` scratch folder has to be emptied at the end of a bake, and the
   command line got that loop inline in patch 2. It was only while writing the
   panel half that I re-read `tests/spells/lodgen_byte_gate.sh` phase (c), which
   compares the panel's output tree with the command line's file by file. Two
   copies of a loop that decides which files survive a bake is a byte difference
   waiting to happen, and phase (c) would have found it a week later in a lane
   that had not touched either file.
   **How it was found:** re-reading the gate that covers the code I was about to
   write, before writing it rather than after.
   **The rule:** when a change lands in BOTH front ends, the shared part goes in
   the shared file first and the two front ends call it. `lodgenDropBtoScratch()`
   in `src/lodgenchunkpass.cpp` is that function now. Ask "what compares these
   two?" before writing the second copy, not after.

3. **Counted tab stops by eye when writing a patch anchor into a 1.6 MB file,
   and every anchor missed.**
   `wwNewRows[]` sits seven tabs deep in `src/nifskope_ui.cpp` and the checks
   around it eight; I wrote six and seven into the patch script's literals. The
   assert fired, which is the system working, but the fix is not to count more
   carefully next time.
   **How it was found:** `AssertionError: anchor count 0 (want 1)`.
   **The rule:** a patch script writes its blocks with NO leading indentation and
   applies the depth programmatically (`ind(text, n)`), so the depth is a number
   that can be checked against the file with one `sed -n '<line>p' | cat -A`
   rather than whitespace nobody can see.

4. **Read a number out of a brief as a current measurement.**
   The brief named the object-coverage failure as "IoU 0.8179, identical on every
   exe since before NATIVEVIEW2". On the re-baked fixture the gate printed
   0.6190, and I spent the first minutes of the diagnosis wondering what had
   broken between those two runs. Nothing had: 0.8179 was measured against the
   `mnam` library and 0.6190 against the `near` library that became the default
   two hours earlier, which a discriminator bake reproduced to four decimals.
   **How it was found:** baking the same region with `--library mnam` and reading
   0.8179 back out of it.
   **The rule (CONSTITUTION 4, again):** a number in a brief is a number from
   some earlier run of some earlier exe. Re-measure it on the tree in front of
   you before you treat a difference as a regression.

5. **Did mistake 1 again, in the same lane, forty minutes after writing it down
   -- and this time it edited a file instead of failing to start.**
   I wanted one `awk` line added to my own chain script and reached for
   `python -c "... printf ...n ..."` rather than opening the file. The escape
   collapsed on the way through the shell, the newline I meant to be two
   characters inside an awk format string arrived as a REAL newline, and the
   script was written with an awk program split across two lines. `bash -n`
   passed it, because a newline inside single quotes is valid shell; awk would
   have refused the string at run time, in a step forty minutes into a chain.
   **How it was found:** the tool that applied the edit echoed the changed lines
   back and the newline was visible in them. Nothing in my own process caught it.
   **What is different from entry 1:** entry 1 said "if it contains a backslash
   or an apostrophe, write it to a file". I read that as advice about LONG or
   TRICKY commands and made an exception for a one-line edit. There is no size
   exemption. The rule is not "be careful with backslashes", it is "a backslash
   never goes through a shell argument", and the one-line edit is exactly where
   it will not be noticed. The script was rewritten whole with the Write tool.
