"""Census of every hkaSplineCompressedAnimation in Fallout4 - Animations.ba2,
read straight from the packfile bytes (no HKXPACK): quantisation types per
track, mask patterns, block counts, float tracks, blend hints, reference-frame
classes, annotation counts. Offsets are the hkClass reflection offsets read
from the 1.10.155 exe (hkclass_reflect.py):
  hkaAnimation:  +0x10 type, +0x14 duration, +0x18 nTransformTracks,
                 +0x1c nFloatTracks, +0x20 extractedMotion*, +0x28 annotationTracks[]
  hkaSplineCompressedAnimation: +0x38 numFrames, +0x3c numBlocks,
                 +0x40 maxFramesPerBlock, +0x44 maskAndQuantizationSize,
                 +0x48 blockDuration, +0x4c blockInverseDuration, +0x50 frameDuration,
                 +0x58 blockOffsets[], +0x68 floatBlockOffsets[], +0x78 transformOffsets[],
                 +0x88 floatOffsets[], +0x98 data[], +0xa8 endian
  hkaAnimationBinding: +0x10 originalSkeletonName, +0x18 animation*,
                 +0x20 transformTrackToBoneIndices[], +0x30 floatTrackToFloatSlotIndices[],
                 +0x40 partitionIndices[], +0x50 blendHint
hkArray = { ptr +0, int size +8, int capAndFlags +0xc }.
"""
import struct, sys, zlib, collections, re, time

ARCHIVE = sys.argv[1]
PATTERN = re.compile(sys.argv[2], re.I) if len(sys.argv) > 2 else None
OUT = sys.argv[3] if len(sys.argv) > 3 else None

def packfile(blob):
    """Return (classnames{nameoff:name}, dataStart, local{src:dst}, global{src:dst}, objects[(off, cls)])."""
    if struct.unpack_from('<II', blob, 0) != (0x57E0E057, 0x10C0C010):
        return None
    numSections, = struct.unpack_from('<i', blob, 20)
    # 0x40-byte file header, then the predicate array (its padded size is the
    # u16 at 0x3e; 0x10 in every FO4 animation packfile, 0 in a collision blob)
    sechdr = 0x40 + struct.unpack_from('<H', blob, 0x3e)[0]
    secs = []
    for s in range(numSections):
        off = sechdr + s * 0x40
        tag = blob[off:off + 19].split(b'\0')[0].decode('latin-1')
        absStart, localFix, globalFix, virtualFix, exports, imports, end = struct.unpack_from('<7i', blob, off + 20)
        secs.append((tag, absStart, localFix, globalFix, virtualFix, exports, imports, end))
    cn = [s for s in secs if s[0] == '__classnames__'][0]
    dt = [s for s in secs if s[0] == '__data__'][0]
    cnames = {}
    p = cn[1]; endp = cn[1] + cn[2]
    while p + 5 < endp:
        if blob[p + 4] != 0x09:
            break
        e = blob.index(b'\0', p + 5)
        cnames[p + 5 - cn[1]] = blob[p + 5:e].decode('latin-1')
        p = e + 1
    base = dt[1]
    local = {}
    for p in range(base + dt[2], min(base + dt[3], len(blob) - 7), 8):
        src, dst = struct.unpack_from('<ii', blob, p)
        if src != -1:
            local[base + src] = base + dst
    glob = {}
    for p in range(base + dt[3], min(base + dt[4], len(blob) - 11), 12):
        src, sec, dst = struct.unpack_from('<iii', blob, p)
        if src != -1:
            glob[base + src] = base + dst
    objs = []
    for p in range(base + dt[4], min(base + dt[5], len(blob) - 11), 12):
        src, sec, cno = struct.unpack_from('<iii', blob, p)
        if src != -1:
            objs.append((base + src, cnames.get(cno, '?')))
    return cnames, base, local, glob, objs

def arr(blob, local, at):
    """hkArray at `at`: (payload offset or None, size)."""
    size, = struct.unpack_from('<i', blob, at + 8)
    return local.get(at), size

def cstr(blob, local, at):
    p = local.get(at)
    if p is None:
        return None
    e = blob.index(b'\0', p)
    return blob[p:e].decode('latin-1')

tally = collections.Counter()
rows = []
t0 = time.time()
with open(ARCHIVE, 'rb') as f:
    magic, version, kind, numFiles, nameTableOffset = struct.unpack('<4sII I Q', f.read(24))
    recs = [struct.unpack('<IIIIQIII', f.read(36)) for _ in range(numFiles)]
    f.seek(nameTableOffset)
    names = []
    for _ in range(numFiles):
        n = struct.unpack('<H', f.read(2))[0]
        names.append(f.read(n).decode('latin-1'))
    for i, (r, nm) in enumerate(zip(recs, names)):
        if not nm.lower().endswith('.hkx'):
            continue
        if PATTERN and not PATTERN.search(nm):
            continue
        _, _, _, _, offset, packed, unpacked, _ = r
        f.seek(offset)
        data = f.read(packed if packed else unpacked)
        if packed:
            try:
                data = zlib.decompress(data)
            except zlib.error:
                tally['zlib-error'] += 1
                continue
        try:
            pf = packfile(data)
        except Exception as e:
            tally['parse-error'] += 1
            if tally['parse-error'] <= 5:
                print('parse-error', nm, repr(e))
            continue
        if pf is None:
            tally['not-packfile'] += 1
            if tally['not-packfile'] <= 5:
                print('not-packfile', nm, data[:8].hex())
            continue
        cnames, base, local, glob, objs = pf
        try:
            anims = [o for o in objs if o[1] == 'hkaSplineCompressedAnimation']
            others = [o[1] for o in objs if o[1].startswith('hka') and 'Animation' in o[1] and o[1] not in ('hkaAnimationContainer', 'hkaAnimationBinding', 'hkaSplineCompressedAnimation')]
            for o in others:
                tally['other-anim-class:' + o] += 1
            if not anims:
                tally['no-spline-anim'] += 1
                continue
            tally['files-with-spline-anim'] += 1
            tally['spline-anims'] += len(anims)
            if len(anims) > 1:
                tally['multi-anim-files'] += 1
            for (ao, _) in anims:
                atype, dur, nT, nF = struct.unpack_from('<ifii', data, ao + 0x10)
                em = glob.get(ao + 0x20)
                emcls = dict(objs).get(em, 'none') if em is not None else 'none'
                tally['extractedMotion:' + emcls] += 1
                annP, annN = arr(data, local, ao + 0x28)
                nFrames, nBlocks, maxFPB, mqs, bd, bid, fd = struct.unpack_from('<iiiifff', data, ao + 0x38)
                boP, boN = arr(data, local, ao + 0x58)
                fboP, fboN = arr(data, local, ao + 0x68)
                toP, toN = arr(data, local, ao + 0x78)
                foP, foN = arr(data, local, ao + 0x88)
                dP, dN = arr(data, local, ao + 0x98)
                endian, = struct.unpack_from('<i', data, ao + 0xa8)
                tally['numBlocks=%d' % nBlocks] += 1
                tally['maxFramesPerBlock=%d' % maxFPB] += 1
                tally['endian=%d' % endian] += 1
                tally['floatTracks>0' if nF else 'floatTracks=0'] += 1
                tally['transformOffsets.n=%d' % toN] += 1
                tally['floatOffsets.n=%d' % foN] += 1
                if mqs != 4 * (nT + nF):
                    tally['mqs!=4*(nT+nF)'] += 1
                if abs(bd - (maxFPB - 1) * fd) > 1e-4:
                    tally['blockDuration!=(maxFPB-1)*fd'] += 1
                if abs(dur - (nFrames - 1) * fd) > 1e-3:
                    tally['duration!=(numFrames-1)*fd'] += 1
                if boN != nBlocks or fboN != nBlocks:
                    tally['blockOffsets.n!=numBlocks'] += 1
                # annotations
                if annP is not None:
                    for k in range(annN):
                        _, an = arr(data, local, annP + k * 0x18 + 8)
                        if an:
                            tally['annotated-tracks'] += 1
                            tally['annotations'] += an
                # per-block masks
                if dP is not None and boP is not None:
                    for b in range(nBlocks):
                        bo, = struct.unpack_from('<I', data, boP + b * 4)
                        for t in range(nT):
                            q, pm, rm, sm = data[dP + bo + t * 4: dP + bo + t * 4 + 4]
                            tally['posQuant=%d' % (q & 3)] += 1
                            tally['rotQuant=%d' % ((q >> 2) & 0xF)] += 1
                            tally['sclQuant=%d' % ((q >> 6) & 3)] += 1
                            tally['posMask=%02x' % pm] += 1
                            tally['rotMask=%02x' % rm] += 1
                            tally['sclMask=%02x' % sm] += 1
                            if pm & 0x70 and pm & 0x07:
                                tally['pos static+spline mixed'] += 1
                            if sm & 0x70 and sm & 0x07:
                                tally['scl static+spline mixed'] += 1
                rows.append((nm, nT, nF, nFrames, nBlocks, maxFPB, dN, emcls))
            # binding
            for (bo_, c) in objs:
                if c != 'hkaAnimationBinding':
                    continue
                bh = data[bo_ + 0x50]
                tally['blendHint=%d' % bh] += 1
                tP, tN = arr(data, local, bo_ + 0x20)
                pP, pN = arr(data, local, bo_ + 0x40)
                tally['partitionIndices.n=%d' % pN] += 1
                skel = cstr(data, local, bo_ + 0x10)
                tally['originalSkeletonName=%s' % skel] += 1
                if tP is not None:
                    idx = struct.unpack_from('<%dh' % tN, data, tP)
                    if list(idx) == list(range(tN)):
                        tally['binding-identity'] += 1
                    else:
                        tally['binding-permuted'] += 1
        except Exception as e:
            tally['decode-error'] += 1
            if tally['decode-error'] <= 5:
                print('decode-error', nm, repr(e))
        if (tally['files-with-spline-anim'] % 2000) == 0:
            print('... %d files, %.0f s' % (tally['files-with-spline-anim'], time.time() - t0), flush=True)

for k, v in sorted(tally.items()):
    print('%-48s %d' % (k, v))
if OUT:
    with open(OUT, 'w', newline='\n') as o:
        for r in rows:
            o.write('\t'.join(str(x) for x in r) + '\n')
print('%.0f s' % (time.time() - t0))
