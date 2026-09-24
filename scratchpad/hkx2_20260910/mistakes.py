import io

entry = """
## 2026-09-10, lane HKX2: a Bash heredoc halved the backslashes AGAIN

**What was done.** A Python patch for `NifSkope.pro` was passed to the Bash tool
as a quoted heredoc. Its anchors were `'\\tsrc/hkxanim.h \\\\\\n'` — tab, text,
line-continuation backslash, newline. The replace matched nothing and the
`assert` fired, on a file whose bytes `cat -A` had just printed as exactly that.

**What was true instead.** The heredoc does not deliver the backslashes intact
even with a quoted delimiter, so the anchor Python saw was not the anchor that
was typed. The same script written to a file with the Write tool and run with
`python <file>` matched on the first try.

**How it was found.** `cat -A` showed `^Isrc/hkxanim.h \\$` in the file while the
assert said the string was absent — the file and the anchor could not both be
right.

**The rule that prevents it.** It is already written down twice: lane HKX1's
mistake 5 the same day, and `nifskope-ww-build-verify` / `nifskope-ww-resume-pending`
("write that script with the WRITE TOOL, never a Bash heredoc or `python -c`").
Repeating an entry that is already in the file is its own entry (CONSTITUTION 2).
The trap only bites on backslashes, which is why three heredoc edits earlier in
the same lane worked and lulled it: the rule is not "use a file when it looks
hard", it is **any patch script whose anchor contains a backslash goes through
the Write tool**.

## 2026-09-10, lane HKX2: Node::local was read as public from the wrong header line

**What was done.** `src/hkxplayback.cpp` was written to assign `node->local` and
walk `node->parent` directly, after reading `class Node`'s member block in
`src/gl/glnode.h` and seeing it below a `public:`.

**What was true instead.** There is a second `protected:` between that `public:`
and the members: `local`, `parent`, `children` and `nodeId` are all protected,
and every class that writes them — `TransformController`,
`MultiTargetTransformController`, `KeyframeController`, `ControllerManager`,
`ProcLightningController` — is declared a `friend` at the top of the class. Six
compile errors.

**How it was found.** The syntax pass, before any build, which is the point of
running it. Cost: one 40-second pass.

**The rule that prevents it.** Read a member's access by finding the LAST
access specifier above it, not the first one below the class head — and when a
codebase has a list of friends at the top of a class, that list is the
documentation of who is allowed to write those members. `HkxPlayback` was added
to it rather than the members made public.
"""

p = 'MISTAKES.md'
s = io.open(p, encoding='utf-8', newline='').read()
before_cr = open(p, 'rb').read().count(b'\r')
if not s.endswith('\n'):
    s += '\n'
s = s + entry.lstrip('\n')
io.open(p, 'w', encoding='utf-8', newline='').write(s)
after_cr = open(p, 'rb').read().count(b'\r')
print('CR before', before_cr, 'after', after_cr)
