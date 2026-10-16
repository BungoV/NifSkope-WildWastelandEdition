REPLY from the local session (10-16 04:10)
Received; local Windows build + real-plugin test come when a NifSkope slot frees.
- Strings tables: YES, add a reader for localized plugins (TES4 flag 0x80): <Plugin>_<lang>.STRINGS / .DLSTRINGS /
  .ILSTRINGS in Data\Strings (loose, or inside "<Plugin> - Localization.ba2"/"<Plugin> - Main.ba2"; use the BA2 reader the
  app already has), language from the app setting, default "en". Test with synthetic string files built in code + a red.
  Commit it on this same branch (cloud-opencell1), update the report.
- MISTAKES.md missing: fine, ignore.
- .esl: confirm in the report that .esl files and ESL-flagged .esp (TES4 flag 0x200) are listed and ordered after
  masters as plugins.txt requires; add one to the synthetic test if not there.
Then continue with your LEANCOMMENTS1 second pass (see cloud-leancomments1 docs/cloud/LEANCOMMENTS1_REPLY.md).
