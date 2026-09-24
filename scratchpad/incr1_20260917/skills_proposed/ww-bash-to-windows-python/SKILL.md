---
name: ww-bash-to-windows-python
description: Use when a Git Bash or MSYS2 harness calls the Windows python (or any Windows exe) and has to pass paths, read a digest back, or compare bytes. Covers the /e/ vs E:/ path boundary, the CRLF-on-stdout trap that makes two correct normalisers disagree, and how to make an empty result go red as vacuous instead of as a mismatch.
user-invocable: true
---

# Bash → Windows python, without lying to yourself

Three failures, all seen in one afternoon in `tests/spells/`, all of which
reported a **defect in the product** when the product was fine.

## 1. The path boundary

Git Bash says `/e/Projects/x`. The Windows python has never heard of it. It does
not error usefully — `sys.path.insert(0, '/e/...')` simply imports nothing and
`open('/e/...')` raises far from the cause.

Convert at the boundary. Every harness in this tree has the helper:

```sh
win () { ( cd "$1" && { pwd -W 2>/dev/null || pwd; } ); }
```

`pwd -W` only works on a directory, so a file goes over as its directory plus
its basename:

```sh
SPW="$(win "$R/tests/spells")"
RECW="$(win "$(dirname "$F")")/$(basename "$F")"
```

## 2. Pass paths as argv, never inside the quoted program

```sh
# NO -- the path is spliced into a shell-quoted -c string, and the nested
# quoting breaks silently
PYOUT="$(python -c "import x; x.f(r'$RECF')")"

# YES
PYOUT="$(python -c "import sys, x; x.f(sys.argv[1])" "$RECW")"
```

If the program body needs a quote, a tab or a backslash, build it from `chr(34)`
/ `chr(9)` / `chr(92)` — or put the program in a file. A `python - <<'PYEOF'`
heredoc is the other safe form, and the quoted delimiter is what stops the shell
touching the body.

## 3. A digest is taken over BYTES, never over a pipe

On Windows a text-mode stdout turns every `\n` into `\r\n`. So this:

```sh
H="$(python tool.py --normalise "$F" | sha1sum)"
```

hashes a byte stream that **nothing ever wrote to disk**, and it will disagree
with the C++ that hashed the real text — same line count, different sha1, and no
clue why. Two fixes, apply both:

* the tool writes bytes: `sys.stdout.buffer.write(text.encode('utf-8'))`;
* the harness takes the digest *inside* python:
  `hashlib.sha1(t.encode()).hexdigest()`.

Corollary, from the memory rule: **line endings are measured with python byte
counts only.** `io.open(p,'rb').read().count(b'\r')`. `grep`, `wc` and your eyes
all lie about this.

## 4. An empty result is a failure, and must look like one

When the python dies, `$(...)` is the empty string, and a comparison then reads

    RED  the two normalisers disagree: 7c50465347... vs

which accuses the product. Guard it:

```sh
if [ -z "${A:-}" ] || [ -z "${B:-}" ]; then
	vacuous "(f) one side printed nothing: nothing was compared"
elif [ "$A" = "$B" ]; then ...
```

and send stderr somewhere you can read (`2>"$W/py.err"`) rather than to
`/dev/null`. A tool that prints nothing has told you something.

## The habit

When a cross-check between two independent implementations goes red, **suspect
the harness first**. Run both halves by hand on the same input in one command
and look at them side by side before touching a line of product code. In the
case this skill came from, the two halves agreed exactly, on the first try, by
hand.
