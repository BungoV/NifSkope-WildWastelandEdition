"""FLAT1 part 2: run patch_flat.py with the evaluation tail, then add the members and the report."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(HERE, 'patch_flat.py'), newline='').read()
tail = open(os.path.join(HERE, 'evaluate_tail.txt'), newline='').read()
assert src.count('@@TAIL@@\n') == 1
src = src.replace('@@TAIL@@\n', tail.replace('\\', '\\\\'))
exec(compile(src, 'patch_flat', 'exec'), {'__name__': 'patch'})

P = 'E:/Projects/NifskopeWWE-flat1/src/lodgen.cpp'
s = open(P, newline='').read()


def _dedent1(t):
    return '\n'.join(l[1:] if l.startswith('\t') else l for l in t.split('\n'))


def rep(a, b, n=1):
    global s
    c = s.count(a)
    if c != n:
        a2, b2 = _dedent1(a), _dedent1(b)
        if s.count(a2) == n:
            print('anchor matched one tab shallower:', repr(a.strip()[:50]))
            a, b = a2, b2
        else:
            sys.exit('anchor count %d != %d: %r' % (c, n, a[:80]))
    s = s.replace(a, b)


rep('''			flatRules.clear();
			flatProblems.clear();
''', '''			flatDataRoot = dataRoot;
			flatRules.clear();
			flatProblems.clear();
''')

rep('''	bool sidewalks = true;
	QSet<QString> sidewalkSeen;
	mutable QSet<size_t> decalHere;
};
''', '''	bool sidewalks = true;
	QSet<QString> sidewalkSeen;
	mutable QSet<size_t> decalHere;
	/* Lane FLAT1. */
	bool flatOn = false;
	QString flatFile, flatFileResolved, flatDataRoot;
	QVector<LodgenFlatOverride> flatRules;
	QStringList flatProblems;
	QVector<LodgenFlatCand> flatCands;
	QVector<LodgenFlatRec> flatRecs;
	QVector<LodgenFlatShape> flatShapes;
	//! texels each report row wrote last, tile content only (the tiles run in turn)
	mutable std::vector<qint64> flatRecTexels;
	int flatG2p = 0;
	QHash<qint64, QVector<float>> landCache;
	QHash<qint64, double> waterCache;
	QHash<qint64, double> roadZCache;
	QHash<qint64, QVector<QPair<int, int>>> roadGrid;
	QHash<QString, LodgenFlatMat> flatMatCache;
};

/*! THE PER-BAKE REPORT (lane FLAT1): one row per base model the flat rule looked
 *  at, most texels first. Tab-separated so it opens in a spreadsheet. */
QString LodgenRoadSet::flatReport( const QString & title ) const
{
	struct Row
	{
		QString model;
		QSet<QString> plugins;
		int placements = 0, painted = 0, overridden = 0, squares = 0;
		std::vector<double> top, under, ratio;
		QMap<QString, int> why;
		qint64 texels = 0;
		bool hasLod = false;
	};
	QHash<QString, Row> rows;
	for ( int i = 0; i < flatRecs.size(); i++ ) {
		const LodgenFlatRec & r = flatRecs[i];
		Row & w = rows[r.model.toLower()];
		if ( w.model.isEmpty() )
			w.model = r.model;
		w.plugins.insert( r.plugin.isEmpty() ? QStringLiteral( "?" ) : r.plugin );
		w.placements++;
		w.squares += r.squares;
		w.hasLod = w.hasLod || r.hasLod;
		if ( r.measured ) {
			w.top.push_back( r.top );
			w.under.push_back( r.under );
			w.ratio.push_back( r.ratio );
		}
		if ( i < int( flatRecTexels.size() ) )
			w.texels += flatRecTexels[size_t( i )];
		QString d;
		if ( r.override >= 0 ) {
			w.overridden++;
			d = QString( "overridden %1 (line %2; the rule: %3)" )
				.arg( flatRules[r.override].bake ? "bake" : "nobake" )
				.arg( flatRules[r.override].line )
				.arg( r.rulePaint ? QStringLiteral( "painted" )
					: QString( "refused, %1" ).arg( QString::fromLatin1( r.why ) ) );
		} else if ( r.painted ) {
			d = QStringLiteral( "painted" );
		} else {
			d = QString( "refused, %1" ).arg( QString::fromLatin1( r.why ) );
		}
		if ( r.painted )
			w.painted++;
		w.why[d]++;
	}
	QVector<const Row *> order;
	for ( auto it = rows.constBegin(); it != rows.constEnd(); ++it )
		order.append( &*it );
	std::sort( order.begin(), order.end(), []( const Row * a, const Row * b ) {
		if ( a->texels != b->texels )
			return a->texels > b->texels;
		if ( a->placements != b->placements )
			return a->placements > b->placements;
		return a->model.compare( b->model, Qt::CaseInsensitive ) < 0;
	} );
	QStringList out;
	int painted = 0, over = 0;
	qint64 tex = 0;
	for ( const LodgenFlatRec & r : flatRecs ) {
		painted += r.painted ? 1 : 0;
		over += r.override >= 0 ? 1 : 0;
	}
	for ( qint64 t : flatRecTexels )
		tex += t;
	out << QString( "# Flat ground objects -- %1" ).arg( title );
	out << QStringLiteral( "# The rule (measured per placement, mesh x scale x rotation, against LAND raised to the stamped"
		" road surface, over 16-unit squares): not under water; underside (median) <= 16 above the ground;"
		" top (90th percentile) >= -8 and <= 64; steep side rising > 8 / visible top <= 0.35; some top." );
	out << QString( "# Override file: %1 (%2 rule line(s)%3)" ).arg( flatFileResolved ).arg( flatRules.size() )
		.arg( flatProblems.isEmpty() ? QString() : QStringLiteral( "; " ) + flatProblems.join( QStringLiteral( "; " ) ) );
	out << QString( "# Placements examined %1, painted %2, decided by an override %3, models %4, texels written %5,"
		" shapes with a greyscale-to-palette material painted %6" )
		.arg( flatRecs.size() ).arg( painted ).arg( over ).arg( rows.size() ).arg( tex ).arg( flatG2p );
	out << QStringLiteral( "# top / underside in game units above the ground, medians over the model's placements;"
		" squares = 16-unit squares under its placements; texels = sheet texels it wrote last" );
	out << QStringLiteral( "model\tplugin\tplacements\ttop\tunderside\tside/top\tdecision\tkind\thas LOD\tsquares\ttexels" );
	auto med = []( const std::vector<double> & v ) {
		return v.empty() ? QStringLiteral( "-" ) : QString::number( lodgenFlatPercentile( v, 50.0 ), 'f', 1 );
	};
	auto med2 = []( const std::vector<double> & v ) {
		return v.empty() ? QStringLiteral( "-" ) : QString::number( lodgenFlatPercentile( v, 50.0 ), 'f', 2 );
	};
	for ( const Row * w : order ) {
		QStringList pl = w->plugins.values();
		pl.sort( Qt::CaseInsensitive );
		QStringList dec;
		for ( auto it = w->why.constBegin(); it != w->why.constEnd(); ++it )
			dec << QString( "%1 x%2" ).arg( it.key() ).arg( it.value() );
		out << QString( "%1\t%2\t%3\t%4\t%5\t%6\t%7\t%8\t%9\t%10\t%11" )
			.arg( w->model, pl.join( QChar( '+' ) ) ).arg( w->placements )
			.arg( med( w->top ), med( w->under ), med2( w->ratio ), dec.join( QStringLiteral( "; " ) ),
				QString::fromLatin1( lodgenFlatKindLabel( w->model ) ), w->hasLod ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) )
			.arg( w->squares ).arg( w->texels );
	}
	return out.join( QChar( '\\n' ) ) + QChar( '\\n' );
}
''')
open(P, 'w', newline='').write(s)
print('patched 2')
