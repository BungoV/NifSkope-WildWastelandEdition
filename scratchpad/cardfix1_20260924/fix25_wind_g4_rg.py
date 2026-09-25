# CARDFIX1 step 6: G4 also prints the SAME sheet's normal R/G error (M), the reference the encoder already
# accepts for a weight-1 channel -- so a red on the sway row can be read against it.
P = 'E:/Projects/NifskopeWWE-cardfix1/tests/spells/impostor_wind.py'
b = open(P, 'rb').read(); assert b.count(b'\r') == 0
s = b.decode('utf-8')
old = "    es = np.abs(Ad - Ash)[cov]\n"
assert s.count(old) == 1
s = s.replace(old, old + "    Rd = np.rint(img[..., 0] * 255.0); Gd = np.rint(img[..., 1] * 255.0)\n"
              "    erg = np.concatenate([np.abs(Rd - nrm[..., 0])[cov], np.abs(Gd - nrm[..., 1])[cov]])\n")
old2 = "          % (four.decode().strip(), w, hgt, cov.sum(), e.mean(), np.percentile(e, 95), e.max(), es.mean(), np.percentile(es, 95)))\n"
assert s.count(old2) == 1
s = s.replace(old2,
    "          % (four.decode().strip(), w, hgt, cov.sum(), e.mean(), np.percentile(e, 95), e.max(), es.mean(), np.percentile(es, 95))\n"
    "          + ' | normal R/G error mean %.3f p95 %.1f' % (erg.mean(), np.percentile(erg, 95)))\n")
out = s.encode('utf-8'); assert out.count(b'\r') == 0
open(P, 'wb').write(out); print('patched')
