import struct, sys, os

def rd(f, fmt):
    n = struct.calcsize(fmt)
    return struct.unpack('<'+fmt, f.read(n))

def parse(path):
    with open(path,'rb') as f:
        data = f.read()
    off = 0
    # header line terminated by \n
    nl = data.index(b'\n')
    hdr = data[:nl].decode('latin1')
    off = nl+1
    def u32():
        nonlocal off
        v = struct.unpack_from('<I', data, off)[0]; off+=4; return v
    def u16():
        nonlocal off
        v = struct.unpack_from('<H', data, off)[0]; off+=2; return v
    def u8():
        nonlocal off
        v = data[off]; off+=1; return v
    def sstr():  # uint32 length + bytes
        nonlocal off
        n = u32()
        s = data[off:off+n]; off+=n; return s.decode('latin1')
    version = u32()
    endian = u8()
    user_version = u32()
    num_blocks = u32()
    # BSStreamHeader export info: bs version
    bs_version = u32()
    # author (short string: byte length + bytes)
    def shortstr():
        nonlocal off
        n = u8()
        s = data[off:off+n]; off+=n; return s.decode('latin1')
    author = shortstr()
    process = shortstr()
    export = shortstr()
    if bs_version >= 130:
        maxfilepath = shortstr()
    # num block types
    num_types = u16()
    types = [sstr() for _ in range(num_types)]
    block_type_index = [u16() for _ in range(num_blocks)]
    block_sizes = [u32() for _ in range(num_blocks)]
    num_strings = u32()
    max_str_len = u32()
    strings = [sstr() for _ in range(num_strings)]
    num_groups = u32()
    groups = [u32() for _ in range(num_groups)]
    # now blocks start at off
    print(f"file={os.path.basename(path)} ver={hdr} bs={bs_version} blocks={num_blocks}")
    blocks = []
    for i in range(num_blocks):
        tname = types[block_type_index[i]]
        start = off
        size = block_sizes[i]
        blocks.append((i, tname, start, size))
        off += size
    return data, hdr, strings, blocks

if __name__ == '__main__':
    path = sys.argv[1]
    data, hdr, strings, blocks = parse(path)
    for i,tname,start,size in blocks:
        extra = ''
        if tname == 'NiBinaryExtraData':
            # name string index (uint32), then uint32 size, bytes
            nameidx = struct.unpack_from('<I', data, start)[0]
            bsize = struct.unpack_from('<I', data, start+4)[0]
            nm = strings[nameidx] if 0 <= nameidx < len(strings) else f'#{nameidx}'
            extra = f" name={nm!r} payload={bsize}B blocksize={size}"
        print(f"  [{i}] {tname} start={start} size={size}{extra}")
