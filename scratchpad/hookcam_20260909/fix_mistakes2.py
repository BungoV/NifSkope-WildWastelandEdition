P = r"E:\Projects\NifskopeWildWastelandEdition\MISTAKES.md"
b = open(P, "rb").read()
assert b.count(b"\r") == 0

anchor = b"## 2026-09-10 -- a one-sided control: the floor failed, and proved nothing\n"
assert b.count(anchor) == 1

new = (
    b"## 2026-09-10 -- lane BUILD3 patched a document with `python -c` and the shell ate it\n"
    b"\n"
    b"- **What was done:** the last edit of the round -- a six-line note appended to\n"
    b"  `scratchpad/lane_hookcam_report.md` -- was made with an inline\n"
    b"  `python -c \"...\"` instead of a script file, because it was small.\n"
    b"- **What was true instead:** the note named five source files in Markdown\n"
    b"  backticks. Bash command-substituted every one of them before Python ever\n"
    b"  saw the string, ran `src/lodgen.cpp` and `WW_CHANGES.md` as commands,\n"
    b"  printed sixty lines of `command not found`, and wrote the note to disk with\n"
    b"  all five filenames REMOVED. The script still exited 0 and still printed its\n"
    b"  byte count, so the only tell was the noise above it.\n"
    b"- **How it was found:** by reading the block back out of the file. The\n"
    b"  arithmetic the script printed (CR 0, bytes 33914) was correct and told\n"
    b"  nothing.\n"
    b"- **The rule, which already existed and was ignored:**\n"
    b"  `nifskope-ww-build-verify` says *\"Patch with a script file, never a heredoc\n"
    b"  or `python -c`\"*, and `nifskope-ww-resume-pending` section 2 says the same.\n"
    b"  Both were loaded this round. A patch being small is not an exemption -- it\n"
    b"  is the reason the exemption gets taken. Repaired with\n"
    b"  `fix_report2.py`, which asserts the mangled text appears exactly once and\n"
    b"  that the five filenames are present after the write.\n"
    b"- **The cheap check that would have caught it without reading anything:** a\n"
    b"  patch script asserts its OUTPUT, not just its byte count. Every one of the\n"
    b"  three script-file patches this round asserted a string that had to be in\n"
    b"  the file afterwards; the inline one asserted only the length.\n"
    b"\n"
)

open(P, "wb").write(b.replace(anchor, new + anchor))
b2 = open(P, "rb").read()
assert b2.count(b"\r") == 0
assert b2.count(b"python -c` and the shell ate it") == 1
print("ok, CR %d, bytes %d" % (b2.count(b"\r"), len(b2)))
