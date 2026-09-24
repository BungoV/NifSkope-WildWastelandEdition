
### 7.2 MISTAKES entries written this lane

Nine, at the head of the repo-root `MISTAKES.md`, newest first, each written at
the moment it was recognised rather than collected at the end. They are listed
here so the director can see the shape of them without reading the ledger:

| when | the mistake | the rule it produced |
|---|---|---|
| 20:0x | a reader whose verdict was wider than its coverage: the `.lodm` row went into section 3 as the format's row after 23 card files, and the reader knew three of six kinds | a reader's verdict covers the inputs it was GIVEN; enumerate a format's families by grepping the writer, not by remembering |
| 19:0x | the addendum's build PATH pasted into Git Bash, where `/ucrt64/bin` does not exist | a PATH quoted for one shell is not a PATH for another; check the first directory exists before blaming the toolchain |
| 18:3x | `instanceCount` read at a remembered 0x80 instead of the decoder's 0x58, and the resulting mismatch reported as a product FAIL | when an independent number disagrees with the product, suspect the offset before the product; the tree's own decoder is the offset of record |
| 18:2x | a refuter aimed at a field its reader never reads, so it proved nothing while reading green | a refuter must mutate a byte the reader actually consumes, and it must be SEEN to go red |
| 18:1x | backslashes through a heredoc, twice in one session, against a rule already written down -- and a third time later the same evening | no text carrying a backslash or an apostrophe goes through a heredoc; a patch script asserts its inserted text is backslash-free |
| 17:5x | a bare `TypeError` traceback read as two failing checks | a traceback is a crashed reader, not a red check; read the exception before the count |
| 17:3x | CRLF spliced into an LF-only report | line endings are measured with Python byte counts, before and after every splice |
| 17:0x | the null incremental run with `--incremental` spelled LAST, so it full-baked for 44 s while I recorded it as incremental | when a run is supposed to take a different path, the proof is the line where the exe NAMES the path, not the clock |
| 16:27 | a patch script that truncated its target before it failed its own anchor check | validate every anchor before opening the target for writing, and write through a temp file plus `os.replace` |

Two of the nine turned into product or suite fixes rather than staying
confessions: 17:0x is F6, and 20:0x is the `.lodm` reader fix in 6.6.
