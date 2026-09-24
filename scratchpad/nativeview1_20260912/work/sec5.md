## 5. Mistakes

Nine, written to `MISTAKES.md` at the top the moment each was recognised and
copied in `MISTAKES_ENTRIES.md`. In one line each:

1. the game check and the exe launch were the same shell command, so the check
   could not guard the launch — and bungo was in the game;
2. a bash heredoc halved the backslashes in a hook-up script, twice, after the
   repo skill that forbids exactly that had been read;
3. anchor counting SUBTRACTED CRLF hits from LF hits on a mixed file and
   cancelled a real match to zero;
4. the already-applied probe matched the anchor's own first line, so three
   edits were silently skipped;
5. `materials\` was prepended to material paths that already resolve, which is
   what MADE them miss (`get_full_path` erases everything before the archive
   folder, and only when it is not at offset 0);
6. I assumed the `.BTR` and the `.BTO` of one chunk share a camera space — the
   `.BTR` is chunk-local, the `.BTO` carries the chunk's world origin — and read
   the resulting empty frame as a failure of the thing under test;
7. `grep -c` prints `0` and exits 1, so `|| echo 0` appended a second zero and a
   census capture became `0` twice;
8. the heredoc-backslash trap a THIRD time, this one leaving a patch silently
   unapplied and costing a ten-frame render run;
9. I built a shader block pointing at an `_msn` without declaring it model-space,
   so the lit land came back 40 percent too dark — the flags were in the bake's
   own `.BTR` the whole time.
