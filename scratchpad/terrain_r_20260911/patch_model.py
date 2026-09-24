p = 'tests/spells/lodgen_cover_model.py'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:70], s.count(a))
    s = s.replace(a, b)


rep("""		self.ltex = {}           # formid -> {'gnam': [...], 'edid': str}""",
    """		self.ltex = {}           # formid -> {'gnam': [...], 'edid': str, 'tnam': formid}
		self.txst = {}           # formid -> {'tx00','tx01','tx07','mnam'}""")

rep("""				elif t == b'LTEX':
					rec = {'gnam': [], 'edid': ''}
					for ft, fd in read_fields(body):
						if ft == b'EDID':
							rec['edid'] = fd.rstrip(b'\\0').decode('latin-1')
						elif ft == b'GNAM' and len(fd) >= 4:
							rec['gnam'].append(struct.unpack_from('<I', fd, 0)[0])
					self.ltex[formid] = rec
					self.ltexOrder.append(formid)""",
    """				elif t == b'LTEX':
					rec = {'gnam': [], 'edid': '', 'tnam': 0}
					for ft, fd in read_fields(body):
						if ft == b'EDID':
							rec['edid'] = fd.rstrip(b'\\0').decode('latin-1')
						elif ft == b'GNAM' and len(fd) >= 4:
							rec['gnam'].append(struct.unpack_from('<I', fd, 0)[0])
						elif ft == b'TNAM' and len(fd) >= 4:
							# the TXST this landscape texture names; the mask model
							# needs TX07 and MNAM, which the cover model never did
							rec['tnam'] = struct.unpack_from('<I', fd, 0)[0]
					self.ltex[formid] = rec
					self.ltexOrder.append(formid)
				elif t == b'TXST':
					rec = {'tx00': '', 'tx01': '', 'tx07': '', 'mnam': '', 'edid': ''}
					for ft, fd in read_fields(body):
						key = {b'EDID': 'edid', b'TX00': 'tx00', b'TX01': 'tx01',
							   b'TX07': 'tx07', b'MNAM': 'mnam'}.get(ft)
						if key:
							rec[key] = fd.rstrip(b'\\0').decode('latin-1')
					self.txst[formid] = rec""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
