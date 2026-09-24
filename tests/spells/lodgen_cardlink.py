"""Lane CARDLINK1 gate helper, called by tests/spells/lodgen_cardlink.sh. Reads the files, never the exe's census.

  fields <pairdir> <cardsdir> <label>   G1 / G2 / G3a / FORCE_CARD: prints `  ok   ...` / `  FAIL ...` lines
  hash <pairdir>                        prints the contract hash recomputed from the arrays on disk (16 hex)
  bump <in.lodo> <out.lodo>             cardCount + 1, headerCrc32 recomputed over 0x10..0xFF (G4's input)
  flip <cardsdir> <outdir> <pairdir>    copy the card sets, change ONE albedo byte of ONE set the pair links (G3's input)
"""
import glob, json, os, shutil, struct, sys, zlib

NO_CARD = 0xFFFF
NO_MESH = 0xFFFF
FNV0 = 0xCBF29CE484222325
FORCE_CARD = 2


def fnv(b, h):
    for c in b:
        h ^= c
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def lodm(path):
    b = open(path, 'rb').read()
    return json.loads(b[12:].decode('utf-8'))


def arrays(pairdir):
    """The card-array .lodm files in the contract order: lower-cased UTF-8 file name, ascending."""
    objs = os.path.join(pairdir, 'Objects')
    names = [os.path.basename(p) for p in glob.glob(os.path.join(objs, '*.LodgenCards.*.lodm'))]
    names.sort(key=lambda n: n.lower().encode('utf-8'))
    return objs, names


def contract_hash(pairdir):
    objs, names = arrays(pairdir)
    h = FNV0
    for n in names:
        m = lodm(os.path.join(objs, n))
        t = m['textures']
        files = [n, t.get('diffuse') or t.get('baseColor'), t['normal'], t.get('gsaos') or t.get('rmaos'), t['emissive']]
        for f in files:
            f = f.replace('\\', '/').split('/')[-1]
            data = open(os.path.join(objs, f), 'rb').read()
            h = fnv(f.lower().encode('utf-8'), h)
            h = fnv(struct.pack('<Q', len(data)), h)
            h = fnv(data, h)
    return h if names else 0


def read_lodo(path):
    b = open(path, 'rb').read()
    ver, = struct.unpack_from('<I', b, 4)
    card_hash, = struct.unpack_from('<Q', b, 0x28)
    base_count, = struct.unpack_from('<I', b, 0x50)
    card_count, = struct.unpack_from('<I', b, 0xD0)
    # the base table offset: find it the way the decoder does
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import lodgen_native_decode as D
    L = D.read_lodo(path)
    return ver, card_hash, base_count, card_count, L['bases']


def read_lodi_flags(path):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import lodgen_native_decode as D
    return D.read_lodi(path)


def main():
    mode = sys.argv[1]
    if mode == 'hash':
        print('%016x' % contract_hash(sys.argv[2]))
        return 0
    if mode == 'bump':
        b = bytearray(open(sys.argv[2], 'rb').read())
        cc, = struct.unpack_from('<I', b, 0xD0)
        struct.pack_into('<I', b, 0xD0, cc + 1)
        struct.pack_into('<I', b, 0x0C, zlib.crc32(bytes(b[0x10:0x100])) & 0xFFFFFFFF)
        open(sys.argv[3], 'wb').write(bytes(b))
        print('cardCount %d -> %d, headerCrc32 recomputed' % (cc, cc + 1))
        return 0
    if mode == 'flip':
        src, dst, pair = sys.argv[2], sys.argv[3], sys.argv[4]
        shutil.copytree(src, dst)
        from PIL import Image
        bases = read_lodo(glob.glob(os.path.join(pair, '*.lodo'))[0])[4]
        linked = sorted(b['formId'] for b in bases if b['cardLayer'] != NO_CARD)
        if not linked:
            print('flip: the pair links no card set'); return 1
        albedo = [os.path.join(dst, '%08x_oct_albedo.png' % linked[0])]
        im = Image.open(albedo[0]).convert('RGBA')
        w, hgt = im.size
        # the first texel that is opaque, so the flip lands inside the card, not in the matte
        for y in range(hgt // 2, hgt):
            done = False
            for x in range(w):
                r, g, bl, a = im.getpixel((x, y))
                if a > 0:
                    im.putpixel((x, y), (r ^ 1, g, bl, a))
                    done = True
                    break
            if done:
                break
        im.save(albedo[0])
        print('flipped one byte of %s at (%d,%d)' % (os.path.basename(albedo[0]), x, y))
        return 0
    if mode != 'fields':
        print('unknown mode'); return 2
    pairdir, cards, label = sys.argv[2], sys.argv[3], sys.argv[4]
    fails = 0

    def ck(name, cond, val):
        nonlocal fails
        print('  %s %s [%s]: %s' % ('ok  ' if cond else 'FAIL', name, label, val))
        if not cond:
            fails += 1

    lodo = glob.glob(os.path.join(pairdir, '*.lodo'))[0]
    lodi = glob.glob(os.path.join(pairdir, '*.lodi'))[0]
    ver, card_hash, base_count, card_count, bases = read_lodo(lodo)
    carded = {b['formId'] for b in bases if b['cardLayer'] != NO_CARD}
    # G1: the tree bases of the table that HAVE a card set in the card directory
    trees_with_set = {b['formId'] for b in bases if (b['flags'] & 1)
                      and os.path.exists(os.path.join(cards, '%08x_oct_albedo.png' % b['formId']))}
    ck('G1 cardCount > 0', card_count > 0, card_count)
    ck('G1 cardCount == tree bases with a card set in the card dir', card_count == len(trees_with_set),
       '%d vs %d (of %d bases)' % (card_count, len(trees_with_set), base_count))
    ck('G1 the rows that name a layer ARE those bases', carded == trees_with_set,
       '%d rows, %d missing, %d extra' % (len(carded), len(trees_with_set - carded), len(carded - trees_with_set)))
    # G2: every layer resolves, in the contract's set order, to a layer whose id is the base
    objs, names = arrays(pairdir)
    layers = [lodm(os.path.join(objs, n))['array']['layers'] for n in names]
    bad = 0
    for b in bases:
        cl = b['cardLayer']
        if cl == NO_CARD:
            continue
        s, l = cl >> 11, cl & 0x7FF
        if s >= len(layers) or l >= len(layers[s]) or int(layers[s][l]['id'], 16) != b['formId']:
            bad += 1
    ck('G2 every cardLayer resolves to its own base\'s layer of its array .lodm', bool(carded) and bad == 0,
       '%d of %d unresolved over %d array(s)' % (bad, len(carded), len(names)))
    # G3a: the header hash is the contract recomputed from the bytes on disk, and not 0
    rh = contract_hash(pairdir)
    ck('G3 cardCorpusHash != 0', card_hash != 0, '%016x' % card_hash)
    ck('G3 cardCorpusHash == the contract recomputed from the arrays', card_hash == rh,
       '%016x vs %016x' % (card_hash, rh))
    # FORCE_CARD: set somewhere, and never on a base without a card
    T = read_lodi_flags(lodi)
    inst = T['instances']
    forced = [i for i in inst if i['flags'] & FORCE_CARD]
    wrong = [i for i in forced if bases[i['baseId']]['cardLayer'] == NO_CARD]
    ck('FORCE_CARD set on some instances, never on a base without a card', len(forced) > 0 and not wrong,
       '%d of %d forced, %d on a card-less base' % (len(forced), len(inst), len(wrong)))
    print('  cards-summary [%s]: version %d cardCount %d cardCorpusHash %016x forced %d arrays %d'
          % (label, ver, card_count, card_hash, len(forced), len(names)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
