"""VTFIX1 fix01: lodgen.cpp -- (1) maskRules counts form 0, (2) blend axis swap, (4) generator identity
in the chunk input digest. Anchors asserted exactly once; CR count asserted unchanged (file is LF-only)."""
P = 'E:/Projects/NifskopeWWE-vtfix1/src/lodgen.cpp'
with open(P, 'rb') as f:
    src = f.read()
cr0 = src.count(b'\r')
T = chr(9)

def sub(old, new, label):
    global src
    o = old.replace('\t', T).encode('utf-8')
    n = new.replace('\t', T).encode('utf-8')
    c = src.count(o)
    assert c == 1, '%s: anchor count %d' % (label, c)
    src = src.replace(o, n)
    print('applied', label)

# ---- (1) the null LTEX is served by the none-default rule, and now counted there
sub("""			if ( m.mat.haveRoughnessMap )
				withRoughnessMap++;
		}
		return *byForm.insert( form, m );
""", """			if ( m.mat.haveRoughnessMap )
				withRoughnessMap++;
		} else {
			/* FORM 0, THE NULL LTEX (lane VTFIX1, 2026-09-24). A NULL-LTEX layer in
			 * a chunk with no dominant base paints through here, and what serves its
			 * mask is the none-default constants: `m.mat` as constructed, rule
			 * LODGEN_MASK_NONE. It used to be stored and never counted, so
			 * `distinctLtex` (this hash's size) ran one past the rule sum on the
			 * whole Commonwealth -- 101 against 100 (lane VTBAKE1). Counted where it
			 * is served, so pbrm + legacyInverted + noneDefault == distinctLtex. */
			ruleCounts[int( m.mat.rule )]++;
		}
		return *byForm.insert( form, m );
""", 'maskRules form 0')

# ---- (2) the blend unpacks the word the way lodgenTerrainMsnPixel packed it
sub("""	for ( size_t i = 0; i < nrm.size(); i++ ) {
		const quint32 p = nrm[i];
		float e = float( p & 0xFFU ) / 255.0f * 2.0f - 1.0f;
		float n = float( ( p >> 16 ) & 0xFFU ) / 255.0f * 2.0f - 1.0f;
		e = qBound( -1.0f, e + dE[i], 1.0f );
		n = qBound( -1.0f, n + dN[i], 1.0f );
		const float s = qMin( e * e + n * n, 1.0f );
		const float up = std::sqrt( qMax( 0.0f, 1.0f - s ) );
		nrm[i] = ( p & 0xFF000000U ) | enc( e ) | ( enc( up ) << 8 ) | ( enc( n ) << 16 );
	}
""", """	for ( size_t i = 0; i < nrm.size(); i++ ) {
		/* THE PACKING IS lodgenTerrainMsnPixel's, quoted from it:
		 *     `nEast << 16 | nUp << 8 | nNorth`
		 * -- bits 16-23 EAST, 8-15 UP, 0-7 NORTH. Until lane VTFIX1 (2026-09-24)
		 * this read east out of bits 0-7 and north out of 16-23 and wrote them
		 * back into the same slots, so dE was added to the NORTH component and
		 * dN to the EAST one: nothing looked transposed, the relief just leaned
		 * the wrong way (found by TERRAINFMT1; measured by VTFIX1's G2, an
		 * east-only known-answer sheet whose detail landed in north, 77.15
		 * levels against 4.21 in east). The unit-length recompute is symmetric
		 * in e and n and cannot catch it. */
		const quint32 p = nrm[i];
		float e = float( ( p >> 16 ) & 0xFFU ) / 255.0f * 2.0f - 1.0f;
		float n = float( p & 0xFFU ) / 255.0f * 2.0f - 1.0f;
		e = qBound( -1.0f, e + dE[i], 1.0f );
		n = qBound( -1.0f, n + dN[i], 1.0f );
		const float s = qMin( e * e + n * n, 1.0f );
		const float up = std::sqrt( qMax( 0.0f, 1.0f - s ) );
		nrm[i] = ( p & 0xFF000000U ) | ( enc( e ) << 16 ) | ( enc( up ) << 8 ) | enc( n );
	}
""", 'blend axis')

# ---- (4) the generator's identity heads every chunk input digest
sub("""QString lodgenChunkInputDigest( const EsmWorld & world, int dim, int cx, int cy,
	const QString & dataRoot )
{
	QCryptographicHash h( QCryptographicHash::Sha1 );
	auto feed = [&h]( const QByteArray & b ) { h.addData( b ); };
""", """/*! THE GENERATOR'S OWN IDENTITY -- the word that heads every chunk input digest
 *  (lane VTFIX1, 2026-09-24; the gap was found by DEFAULTS1 and DEFAULTS2).
 *
 *  The ledger compared the typed argument vector and the chunk's inputs, and
 *  nothing about the program that turns inputs into bytes. So a default flip --
 *  bungo's rulings move them, and they live in three places (nifcli's `lg*`
 *  locals, lodgen.h's option initialisers, this file's `g_*` globals) -- left
 *  argv and inputs as they were, and `--incremental` on the new exe kept the old
 *  exe's chunks and called them clean. A code change that moves bytes with no
 *  default flipped at all (this lane's own axis fix) is the same hole.
 *
 *  The word is the sha1 of the RUNNING EXECUTABLE's bytes: every default and every
 *  byte-moving code change is inside it, and nobody has to remember to bump a
 *  version constant. It can only over-rebake -- a rebuild that moves no output
 *  byte still dirties every chunk once -- which is the ledger's stated direction
 *  ("it can only over-rebake, never under-rebake"). An exe that cannot be read
 *  gets a word that can never match a stored ledger, for the same reason.
 *  Hashed once per process (~25 MB, tens of milliseconds). */
static QByteArray lodgenGeneratorIdentity()
{
	static const QByteArray id = []() {
		QByteArray out( "generator " );
		const QString exe = QCoreApplication::instance()
			? QCoreApplication::applicationFilePath() : QString();
		QFile f( exe );
		QCryptographicHash eh( QCryptographicHash::Sha1 );
		if ( !exe.isEmpty() && f.open( QIODevice::ReadOnly ) && eh.addData( &f ) )
			out += eh.result().toHex();
		else
			out += "unreadable " + QByteArray::number( QDateTime::currentMSecsSinceEpoch() );
		out += ';';
		return out;
	}();
	return id;
}

QString lodgenChunkInputDigest( const EsmWorld & world, int dim, int cx, int cy,
	const QString & dataRoot )
{
	QCryptographicHash h( QCryptographicHash::Sha1 );
	auto feed = [&h]( const QByteArray & b ) { h.addData( b ); };
	feed( lodgenGeneratorIdentity() );
""", 'generator identity')

sub("""#include <QCryptographicHash>
#include <QJsonDocument>
""", """#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
""", 'includes')

assert src.count(b'\r') == cr0, 'CR count moved'
data = src
with open(P, 'wb') as f:
    f.write(data)
print('CR', cr0, 'unchanged; bytes', len(data))
