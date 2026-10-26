#include "cellnifxlights.h"

#include <QFileInfo>
#include <QMap>

#include <algorithm>
#include <cmath>
#include <limits>

int wwNifxRedOf( const QByteArray & words )
{
	int r = 0;
	for ( const QByteArray & w : words.split( ',' ) ) {
		const QByteArray t = w.trimmed().toLower();
		r |= t == "ignore" ? WwNifxRedIgnore : t == "tintdims" ? WwNifxRedTintDims
			: t == "keepplaced" ? WwNifxRedKeepPlaced : t == "precedence" ? WwNifxRedPrecedence
			: t == "noscale" ? WwNifxRedNoScale : t == "nofollow" ? WwNifxRedNoFollow
			: t == "bulbsurface" ? WwNifxRedBulbSurface : t == "bulblit" ? WwNifxRedBulbLit
			: t == "norelight" ? WwNifxRedNoRelight : t == "nobulbgi" ? WwNifxRedNoBulbGi : 0;
	}
	return r;
}

namespace {

struct Made
{
	WwCellLight l;
	QString model;
	int carrierList = -1, carrier = -1;	// the placed light whose state it took (list 0 lights, 1 offLights, 2 nifxOffPlaced)
	float best = std::numeric_limits<float>::infinity();
};

struct MeshCount
{
	int placed = 0, perPlacement = 0, hero = 0, normal = 0, bulb = 0, tookOver = 0, niLight = 0, nodeMiss = 0;
	int followsOn = 0, followsUnbound = 0, warnings = 0;
	int dimmed = 0, sequences = 0;	// lane NIFXLIGHT2: lights a NiLight's dimmer scaled; dimmer sequences bound to them
	QString state;
};

const WwNifxNode * findNode( const WwNifxModel & m, const std::string & name, bool lightOnly )
{
	const QString n = QString::fromStdString( name ).trimmed();
	for ( const WwNifxNode & x : m.nodes )
		if ( ( !lightOnly || x.niLight ) && x.name.compare( n, Qt::CaseInsensitive ) == 0 )
			return &x;
	return nullptr;
}

void apply3( const float m[3][4], const double v[3], bool point, double out[3] )
{
	for ( int r = 0; r < 3; r++ )
		out[r] = m[r][0] * v[0] + m[r][1] * v[1] + m[r][2] * v[2] + ( point ? m[r][3] : 0.0f );
}

}	// namespace

void wwNifxLightsApply( WwCellLighting & L, const QHash<QString, nifxl::Lights> & docs, int red, QStringList * census )
{
	QVector<Made> made;
	QMap<QString, MeshCount> mesh;
	QMap<QString, QStringList> fileWarn;
	for ( const WwNifxFixture & f : L.nifxFixtures ) {
		if ( !f.model )
			continue;
		const WwNifxModel & m = *f.model;
		MeshCount & c = mesh[m.model];
		c.placed++;
		const auto d = docs.constFind( m.sidecar );
		if ( d == docs.constEnd() ) {
			c.state = QStringLiteral( "unread" );
			continue;
		}
		const nifxl::Lights & doc = *d;
		if ( c.placed == 1 ) {
			for ( const nifxl::Warning & w : doc.warnings )
				fileWarn[m.sidecar] << QStringLiteral( "%1 %2: %3 %4%5" ).arg( QString::fromStdString( w.at ),
					QString::fromStdString( w.key ), QString::fromStdString( w.warning ), QString::fromStdString( w.problem ),
					w.suggest.empty() ? QString() : QStringLiteral( " (did you mean %1)" ).arg( QString::fromStdString( w.suggest ) ) );
			c.warnings = int( doc.warnings.size() );
		}
		if ( doc.status == "refused" ) {
			c.state = QStringLiteral( "refused %1" ).arg( QString::fromStdString( doc.reason ) );
			continue;
		}
		if ( ( red & WwNifxRedIgnore ) || !doc.hasLights ) {
			c.state = ( red & WwNifxRedIgnore ) ? QStringLiteral( "lights ignored (red)" ) : QStringLiteral( "no lights section" );
			continue;
		}
		c.state = QStringLiteral( "ok" );
		c.perPlacement = int( doc.lights.size() );
		for ( const nifxl::Light & x : doc.lights ) {
			static const WwNifxNode root;
			const WwNifxNode * node = x.hasNode ? findNode( m, x.node, false ) : &root;
			if ( !node ) {
				node = &root;
				c.nodeMiss += c.placed == 1;
			} else if ( x.hasNode && node->niLight ) {
				c.niLight += c.placed == 1;	// edited in place: the NiLight's frame, these values
			}
			/* lane NIFXLIGHT2: the dimmer. follows copies the named NiLight's; an edited NiLight keeps its own (the values
			 * are the full-on light, the sequence's dimmer scales it). The cell view draws the On state: the dimmer
			 * there (src/cellview.cpp cellNifxModel). An unbound follows stays at full strength. */
			const WwNifxNode * dimNode = x.hasFollows ? findNode( m, x.follows, true ) : x.hasNode && node->niLight ? node : nullptr;
			if ( x.hasFollows && c.placed == 1 )
				( dimNode ? c.followsOn : c.followsUnbound )++;
			const float dim = dimNode && !( red & WwNifxRedNoFollow ) ? dimNode->dimmer : 1.0f;
			if ( c.placed == 1 && dimNode ) {
				c.dimmed += dim != 1.0f;
				c.sequences += dimNode->sequences;
			}
			double lp[3], ld[3];
			apply3( node->m, x.position, true, lp );
			apply3( node->m, x.direction, false, ld );
			Made o;
			o.model = m.model;
			WwCellLight & l = o.l;
			double dn = 0;
			for ( int r = 0; r < 3; r++ ) {
				double p = 0, q = 0;
				for ( int k = 0; k < 3; k++ ) {
					p += f.rot[r][k] * lp[k] * f.scale;
					q += f.rot[r][k] * ld[k];
				}
				l.pos[r] = float( f.pos[r] + p );
				l.dir[r] = float( q );
				dn += q * q;
			}
			dn = std::sqrt( dn );
			for ( int r = 0; r < 3 && dn > 0; r++ )
				l.dir[r] = float( l.dir[r] / dn );
			const nifxl::Converted cv = nifxl::convert( x, ( red & WwNifxRedTintDims ) != 0 );
			// lane NIFXLIGHT2: the reach grows with the fixture's placed scale, as its position does (then the spec's range)
			l.radius = ( red & WwNifxRedNoScale ) ? float( cv.radius ) : std::clamp( float( cv.radius ) * f.scale, 16.0f, 16384.0f );
			for ( int k = 0; k < 3; k++ ) {
				l.byteColor[k] = float( cv.color[k] ) / 255.0f;
				l.color[k] = std::pow( float( cv.color[k] ) / 255.0f, 2.2f ) * float( cv.fade ) * dim;
			}
			l.spot = x.type == nifxl::Spot;
			if ( l.spot )
				l.cosOuter = float( std::cos( x.cone * 0.5 * 3.14159265358979323846 / 180.0 ) );
			l.shape = x.type == nifxl::Hemisphere ? 1 : 0;
			l.tier = x.tier;
			l.shadow = x.tier != nifxl::Hero ? 0 : l.spot ? 1 : l.shape == 1 ? 2 : 3;
			l.nifxFog = x.hasFog ? float( x.fogScale ) : -1.0f;
			l.nifxName = QString::fromStdString( x.name );
			l.ref = f.ref;
			l.recxRefPlugin = f.refPlugin;
			l.recxRefLocal = f.refLocal;
			( x.tier == nifxl::Hero ? c.hero : x.tier == nifxl::Bulb ? c.bulb : c.normal )++;
			made.append( o );
		}
	}

	// placed lights inside a fixture: the nearest .nifx light (first on a tie) takes each, the nearest carries its state
	QVector<bool> taken[3];
	QVector<WwCellLight> * lists[3] = { &L.lights, &L.offLights, &L.nifxOffPlaced };
	for ( int li = 0; li < 3; li++ ) {
		taken[li].fill( false, lists[li]->size() );
		if ( red & WwNifxRedKeepPlaced )
			continue;
		for ( qsizetype i = 0; i < lists[li]->size(); i++ ) {
			const WwCellLight & p = lists[li]->at( i );
			if ( p.tier >= 0 )
				continue;
			int best = -1;
			float bd = kWwNifxReach * kWwNifxReach;
			for ( int j = 0; j < made.size(); j++ ) {
				float d2 = 0;
				for ( int k = 0; k < 3; k++ )
					d2 += ( p.pos[k] - made[j].l.pos[k] ) * ( p.pos[k] - made[j].l.pos[k] );
				if ( d2 < bd || ( best < 0 && d2 <= bd ) ) {
					bd = d2;
					best = j;
				}
			}
			if ( best < 0 )
				continue;
			taken[li][i] = true;
			mesh[made[best].model].tookOver++;
			Made & o = made[best];
			if ( bd < o.best ) {
				o.best = bd;
				o.carrierList = li;
				o.carrier = int( i );
			}
		}
	}
	int tookOver = 0, dark = 0, bulbDark = 0;
	for ( Made & o : made ) {
		if ( o.carrier < 0 )
			continue;
		const WwCellLight & p = lists[o.carrierList]->at( o.carrier );
		WwCellLight & l = o.l;
		l.flicker = p.flicker;
		l.flickerPeriod = p.flickerPeriod;
		l.flickerIntensity = p.flickerIntensity;
		l.flickerMovement = p.flickerMovement;
		std::copy( &p.flickerAxes[0][0], &p.flickerAxes[0][0] + 9, &l.flickerAxes[0][0] );
		l.ref = p.ref;
		l.recxRefPlugin = p.recxRefPlugin;
		l.recxRefLocal = p.recxRefLocal;
		l.recxBasePlugin = p.recxBasePlugin;
		l.recxBaseLocal = p.recxBaseLocal;
		l.recxBaseEdid = p.recxBaseEdid;
	}
	for ( int li = 0; li < 3; li++ ) {
		QVector<WwCellLight> keep;
		for ( qsizetype i = 0; i < lists[li]->size(); i++ )
			if ( !taken[li][i] )
				keep.append( lists[li]->at( i ) );
			else
				tookOver++;
		if ( keep.size() != lists[li]->size() )
			*lists[li] = keep;
	}
	for ( const Made & o : made ) {
		if ( o.l.tier == nifxl::Bulb && o.carrierList >= 1 && !( red & WwNifxRedBulbLit ) )
			bulbDark++;	// lane NIFXLIGHT2: a bulb that took a start-off light is dark with it (the bulb has no live switch)
		else if ( o.l.tier == nifxl::Bulb && !( red & WwNifxRedBulbSurface ) )
			L.nifxBulbs.append( o.l );
		else if ( o.l.tier == nifxl::Bulb )
			L.lights.append( o.l );	// the red: drawn on surfaces
		else if ( o.carrierList == 1 )
			L.offLights.append( o.l );	// starts off with the light it took over (kept for a live switch)
		else if ( o.carrierList == 2 )
			dark++;
		else
			L.lights.append( o.l );
	}
	if ( !census )
		return;
	for ( auto it = mesh.cbegin(); it != mesh.cend(); ++it ) {
		const MeshCount & c = it.value();
		census->append( QStringLiteral( "nifx lights: %1: placed %2, %3; %4 lights each (hero %5, normal %6, bulb %7), "
			"took over %13 placed, NiLight edits %8, node not found %9, follows %10 (on state) unbound %11, warnings %12" )
			.arg( it.key() ).arg( c.placed ).arg( c.state ).arg( c.perPlacement ).arg( c.hero / std::max( c.placed, 1 ) )
			.arg( c.normal / std::max( c.placed, 1 ) ).arg( c.bulb / std::max( c.placed, 1 ) ).arg( c.niLight ).arg( c.nodeMiss )
			.arg( c.followsOn ).arg( c.followsUnbound ).arg( c.warnings ).arg( c.tookOver ) );
		if ( c.dimmed || c.sequences )	// lane NIFXLIGHT2
			census->append( QStringLiteral( "nifx lights: %1: %2 lights scaled by a NiLight dimmer, %3 dimmer sequences bound" )
				.arg( it.key() ).arg( c.dimmed ).arg( c.sequences ) );
	}
	for ( auto it = fileWarn.cbegin(); it != fileWarn.cend(); ++it )
		for ( const QString & w : it.value() )
			census->append( QStringLiteral( "nifx lights: %1: %2" ).arg( QFileInfo( it.key() ).fileName(), w ) );
	census->append( QStringLiteral( "nifx lights: %1 fixtures, %2 lights made (%3 drawn, %4 bulb), %5 placed lights switched off, "
		"%6 dark (took a start-off light)%7" ).arg( L.nifxFixtures.size() ).arg( made.size() )
		.arg( made.size() - L.nifxBulbs.size() - dark - bulbDark ).arg( L.nifxBulbs.size() ).arg( tookOver ).arg( dark )
		.arg( red ? QStringLiteral( " red %1" ).arg( red ) : QString() ) );
	if ( bulbDark )	// lane NIFXLIGHT2
		census->append( QStringLiteral( "nifx lights: %1 bulb-tier lights dark (took a start-off light)" ).arg( bulbDark ) );
}

bool wwNifxGiLights( const WwCellLighting & L, int red, WwCellLighting & out )
{
	if ( L.nifxBulbs.isEmpty() || ( red & WwNifxRedNoBulbGi ) )
		return false;
	out = L;
	out.lights += L.nifxBulbs;
	return true;
}

QString wwNifxLightsDump( const WwCellLighting & L )
{
	QString s;
	auto row = [&]( const char * where, const WwCellLight & l ) {
		s += QStringLiteral( "%1 %2 tier %3 pos %4 %5 %6 radius %7 color %8 %9 %10 bytes %11 %12 %13 spot %14 cos %15 shape %16 "
			"dir %17 %18 %19 shadow %20 fog %21 flicker %22 ref %23 recxref %24|%25 recxbase %26|%27\n" )
			.arg( QLatin1String( where ), l.nifxName ).arg( l.tier )
			.arg( double( l.pos[0] ), 0, 'f', 3 ).arg( double( l.pos[1] ), 0, 'f', 3 ).arg( double( l.pos[2] ), 0, 'f', 3 )
			.arg( double( l.radius ), 0, 'f', 3 )
			.arg( double( l.color[0] ), 0, 'g', 9 ).arg( double( l.color[1] ), 0, 'g', 9 ).arg( double( l.color[2] ), 0, 'g', 9 )
			.arg( qRound( l.byteColor[0] * 255.0f ) ).arg( qRound( l.byteColor[1] * 255.0f ) ).arg( qRound( l.byteColor[2] * 255.0f ) )
			.arg( l.spot ? 1 : 0 ).arg( double( l.cosOuter ), 0, 'f', 6 ).arg( l.shape )
			.arg( double( l.dir[0] ), 0, 'f', 6 ).arg( double( l.dir[1] ), 0, 'f', 6 ).arg( double( l.dir[2] ), 0, 'f', 6 )
			.arg( l.shadow ).arg( double( l.nifxFog ), 0, 'g', 6 ).arg( l.flicker )
			.arg( l.ref, 8, 16, QLatin1Char( '0' ) ).arg( l.recxRefPlugin ).arg( l.recxRefLocal, 8, 16, QLatin1Char( '0' ) )
			.arg( l.recxBasePlugin ).arg( l.recxBaseLocal, 8, 16, QLatin1Char( '0' ) );
	};
	for ( const WwCellLight & l : L.lights )
		if ( l.tier >= 0 )
			row( "drawn", l );
	for ( const WwCellLight & l : L.offLights )
		if ( l.tier >= 0 )
			row( "off", l );
	for ( const WwCellLight & l : L.nifxBulbs )
		row( "bulb", l );
	s += QStringLiteral( "placed lights left: on %1 off %2\n" )
		.arg( std::count_if( L.lights.cbegin(), L.lights.cend(), []( const WwCellLight & l ) { return l.tier < 0; } ) )
		.arg( std::count_if( L.offLights.cbegin(), L.offLights.cend(), []( const WwCellLight & l ) { return l.tier < 0; } ) );
	return s;
}
