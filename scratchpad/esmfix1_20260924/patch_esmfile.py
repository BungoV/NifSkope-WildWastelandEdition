"""ESMFIX1: resolve a form ID whose file index is beyond the plugin's master list to the
plugin itself (the game's and xEdit's rule), instead of refusing the whole plugin.
Anchors must each match exactly once; the file stays LF-only."""
P = r'E:/Projects/NifskopeWWE-esmfix1/lib/libfo76utils/src/esmfile.cpp'
with open(P, 'rb') as f:
    src = f.read()
cr0 = src.count(b'\r')
assert cr0 == 0
edits = [
(b'''     * master is absent, preserving the old raw-prefix behaviour). The
     * file's own records use the byte after its last master. */
''',
b'''     * master is absent, preserving the old raw-prefix behaviour). The
     * file's own records use the byte after its last master.
     *
     * ESMFIX1: in the Creation Engine games up to Fallout 4 (form version
     * below 0xC0) EVERY index at or beyond the master count names the
     * plugin itself -- the game's rule, and xEdit's
     * (TwbFile.FileFileIDtoLoadOrderFileID: FullSlot < MasterCount -> that
     * master, else the file's own slot). A plugin saved with FF-prefixed
     * IDs over one master (TestWorldspace.esp, 483 records) is legal and
     * loads in game; it used to be refused whole as "invalid form ID".
     * Fallout 76 / Starfield files (form version >= 0xC0) keep the old
     * mapping and the old refusal. */
    std::vector< unsigned char >  ownBeyondMasters(esmFiles.size(), 0);
''',),
(b'''      size_t  masterCnt = 0;
      while ((buf.getPosition() + 6) <= endPos)
''',
b'''      size_t  masterCnt = 0;
      // the file's own form version (the member esmVersion is the LAST
      // file's); Oblivion's 20-byte header carries none
      unsigned int  fileFormVersion = 0U;
      if (recordHdrSize >= 24 && buf.size() >= 24)
        fileFormVersion = FileBuffer::readUInt16Fast(buf.data() + 20);
      ownBeyondMasters[i] = (unsigned char) (fileFormVersion < 0xC0U);
      while ((buf.getPosition() + 6) <= endPos)
''',),
(b'''      if (masterCnt < 256)
        m[masterCnt] = std::uint32_t(i);
    }
''',
b'''      if (ownBeyondMasters[i])
      {
        for (size_t k = masterCnt; k < 256; k++)
          m[k] = std::uint32_t(i);
      }
      else if (masterCnt < 256)
      {
        m[masterCnt] = std::uint32_t(i);
      }
    }
''',),
(b'''          if (formID > 0x0FFFFFFFU && ((formID + 0x03000000U) & 0xFE000000U))
          {
''',
b'''          // any top byte is a legal file index under the game's rule (the
          // map above sends one beyond the masters to this file)
          if (!ownBeyondMasters[i] &&
              formID > 0x0FFFFFFFU && ((formID + 0x03000000U) & 0xFE000000U))
          {
''',),
]
for a, b in edits:
    n = src.count(a)
    assert n == 1, (n, a[:60])
    src = src.replace(a, b)
assert src.count(b'\r') == cr0
with open(P, 'wb') as f:
    f.write(src)
print('patched, LF', src.count(b'\n'), 'CR', src.count(b'\r'))
