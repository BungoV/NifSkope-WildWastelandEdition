#!/usr/bin/env python3
"""lane WSRESTORE1's independent reader: is a dock VISIBLE in a saved window layout?

Reads "UI/Window State" from a scratch settings scope
(HKCU\\Software\\NifTools\\NifSkope 2.0 <scope>) and answers from Qt's own
QMainWindow::saveState() byte format, not from NifSkope's code:

  * QSettings on Windows keeps a QByteArray as the string "@ByteArray(<bytes>)",
    one character per byte (Latin-1), stored as REG_BINARY UTF-16LE when the
    bytes hold a zero.
  * QDockAreaLayoutInfo::saveState writes each docked widget as
    uchar WidgetMarker (0xfb), QString objectName (uint32 byte length, UTF-16BE),
    uchar flags (bit 0 = visible, bit 1 = floating).

usage: cell_wsrestore.py <scope> <dock object name>...
prints one line per dock: "<name> visible=<0|1>", or "<name> missing" (exit 2).
"""
import struct
import sys
import winreg


def saved_state(scope):
    key = winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                         "Software\\NifTools\\NifSkope 2.0 %s\\UI" % scope)
    raw, kind = winreg.QueryValueEx(key, "Window State")
    text = raw if isinstance(raw, str) else bytes(raw).decode("utf-16-le")
    text = text.rstrip("\x00")
    if not text.startswith("@ByteArray(") or not text.endswith(")"):
        raise SystemExit("not a @ByteArray value: %r" % text[:20])
    return text[len("@ByteArray("):-1].encode("latin-1")


def dock_visible(blob, name):
    enc = name.encode("utf-16-be")
    needle = b"\xfb" + struct.pack(">I", len(enc)) + enc
    hits = []
    at = blob.find(needle)
    while at >= 0:
        hits.append(blob[at + len(needle)])
        at = blob.find(needle, at + 1)
    if len(hits) != 1:
        return None
    return hits[0] & 1


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    blob = saved_state(sys.argv[1])
    rc = 0
    for name in sys.argv[2:]:
        v = dock_visible(blob, name)
        if v is None:
            print("%s missing" % name)
            rc = 2
        else:
            print("%s visible=%d" % (name, v))
    sys.exit(rc)


if __name__ == "__main__":
    main()
