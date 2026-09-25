"""G3: bases in a .lodo that carry a card layer, by the plugin load index (form id top byte). usage: g3_cards.py <lodo>"""
import sys, collections
sys.path.insert(0, 'E:/Projects/NifskopeWWE-bake2/tests/spells')
import lodgen_native_decode as d
h, L = None, None
r = d.read_lodo(sys.argv[1])
L = r[1] if isinstance(r, tuple) else r
bases = L['bases']
card = [b for b in bases if b['cardLayer'] != d.NO_CARD]
by = collections.Counter('%02x' % (b['formId'] >> 24) for b in card)
allby = collections.Counter('%02x' % (b['formId'] >> 24) for b in bases)
print('bases', len(bases), 'with card', len(card), 'card by plugin index', dict(sorted(by.items())), 'all bases by index', dict(sorted(allby.items())))
print('card bases:', ['%08x' % b['formId'] for b in card][:12])
