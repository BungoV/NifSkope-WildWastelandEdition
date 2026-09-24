
## 2026-09-10 - the heredoc halved a backslash AGAIN (lane BUILD6)

**What was done.** The hook-up patch script was written through a `cat >
file <<'EOF'` heredoc from the Bash tool. Its anchor for the usage line carried
`\\n` so that Python would see the two characters `\n` that the C string
literal holds. The file on disk held `\n` -- one backslash gone -- so Python
read a real newline, the anchor counted 0, and the dry run refused. Two
diagnostic passes were spent on the anchor before the byte-repr showed the
file's line ended `]\n"` while the script's ended `]<LF>"`.

**What was true instead.** The entry of 2026-09-10 line 310 in this file
already says it: *heredocs halve backslashes*, and `nifskope-ww-build-verify`
says *patch with a script FILE, never a heredoc*. A quoted heredoc through this
tool is not a script file; only the Write tool is. This is the FOURTH payment
of the same rule (the entry above counted three).

**How it was found.** `repr()` of the file's bytes beside `repr()` of the
script's string, progressive-prefix counting.

**The rule that prevents it.** Any script that holds a backslash is written
with the Write tool, full stop; the Bash heredoc is for scripts with none, and
the first thing such a script does is print `repr()` of every anchor that
contains one. Repeating an entry already in this file is its own entry
(CONSTITUTION 2), and this is that entry.

## 2026-09-10 - a region read off a one-line `X0=-24; Y0=24; ...` with the semicolon (lane BUILD6)

**What was done.** To reproduce the V9b pair outside the harness, the region
was pulled from `lodgen_terrain_vt.sh` with `grep -oP '^X0=\K\S+'`. The
harness writes all four assignments on ONE line, so `\S+` took `-24;` and the
next three variables came back empty; both bakes exited 2 and the first
"analysis" ran on files that did not exist (the Python traceback was the only
thing that said so).

**What was true instead.** `X0=-24; Y0=24; X1=-17; Y1=31`, the same region the
edge-band bakes use; it was already known and should have been typed.

**How it was found.** The bake's `rc=2` beside `region -24;` in the same
output.

**The rule that prevents it.** A value that is already known is typed, not
scraped; a scraped value is echoed and checked for shape before the command
that uses it runs; and a bake whose rc is not 0 stops the chain there rather
than letting the next step explain the failure.
