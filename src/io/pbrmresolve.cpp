#include "io/pbrmresolve.h"

#include "io/lodmfile.h"
#include "io/material.h"


const char * pbrmRouteName( PbrmRoute r )
{
	switch ( r ) {
	case PbrmRoute::Swap:
		return "swap";
	case PbrmRoute::Nifx:
		return "nifx";
	case PbrmRoute::Direct:
		return "direct";
	case PbrmRoute::Sibling:
		return "sibling";
	case PbrmRoute::Fo76:
		return "fo76";
	case PbrmRoute::Stem:
		return "stem";
	default:
		return "legacy";
	}
}

QList<PbrmRoute> pbrmDefaultOrder()
{
	return { PbrmRoute::Swap, PbrmRoute::Nifx, PbrmRoute::Direct, PbrmRoute::Sibling, PbrmRoute::Fo76,
		PbrmRoute::Stem };
}

bool pbrmParseOrder( const QString & spec, QList<PbrmRoute> & out, QString & why )
{
	out = pbrmDefaultOrder();
	why.clear();
	QList<PbrmRoute> parsed;
	const QList<PbrmRoute> all = pbrmDefaultOrder();
	for ( const QString & raw : spec.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) ) {
		const QString t = raw.trimmed().toLower();
		bool found = false;
		for ( PbrmRoute r : all ) {
			if ( t == QLatin1String( pbrmRouteName( r ) ) ) {
				if ( parsed.contains( r ) ) {
					why = QStringLiteral( "route \"%1\" named twice" ).arg( t );
					return false;
				}
				parsed.append( r );
				found = true;
				break;
			}
		}
		if ( !found ) {
			why = QStringLiteral( "unknown route \"%1\"" ).arg( t );
			return false;
		}
	}
	for ( PbrmRoute r : all ) {
		if ( r == PbrmRoute::Stem )
			continue;
		if ( !parsed.contains( r ) ) {
			why = QStringLiteral( "route \"%1\" missing" ).arg( QLatin1String( pbrmRouteName( r ) ) );
			return false;
		}
	}
	if ( !parsed.contains( PbrmRoute::Stem ) )
		parsed.append( PbrmRoute::Stem );
	out = parsed;
	return true;
}

bool pbrmNormaliseMaterialPath( const QString & authored, const QStringList & wantExt,
	QString & out, QString * why )
{
	out.clear();
	QString p = authored.trimmed();
	auto refuse = [why]( const QString & w ) {
		if ( why )
			*why = w;
		return false;
	};
	if ( p.isEmpty() )
		return refuse( QStringLiteral( "empty material path" ) );
	p.replace( QLatin1Char( '/' ), QLatin1Char( '\\' ) );
	if ( p.startsWith( QLatin1Char( '\\' ) ) || p.contains( QLatin1Char( ':' ) ) )
		return refuse( QStringLiteral( "material path is absolute or drive-qualified" ) );
	while ( p.contains( QLatin1String( "\\\\" ) ) )
		p.replace( QLatin1String( "\\\\" ), QLatin1String( "\\" ) );
	while ( p.startsWith( QLatin1String( ".\\" ) ) )
		p.remove( 0, 2 );
	for ( const QString & seg : p.split( QLatin1Char( '\\' ) ) ) {
		if ( seg == QLatin1String( ".." ) )
			return refuse( QStringLiteral( "parent traversal in material path" ) );
	}
	bool extOk = false;
	for ( const QString & e : wantExt ) {
		if ( p.endsWith( e, Qt::CaseInsensitive ) ) {
			extOk = true;
			break;
		}
	}
	if ( !extOk )
		return refuse( QStringLiteral( "material path is not a %1" ).arg( wantExt.join( QLatin1Char( '/' ) ) ) );
	if ( !p.startsWith( QLatin1String( "materials\\" ), Qt::CaseInsensitive ) )
		p.prepend( QLatin1String( "Materials\\" ) );
	out = p;
	return true;
}

QStringList pbrmSwapCandidates( const QString & diffuse, QString * why )
{
	// FO4CS NormalizeTexturePath + ResolveTextureSwapMaterialPaths.
	QString t = diffuse.trimmed();
	auto refuse = [why]( const QString & w ) {
		if ( why )
			*why = w;
		return QStringList();
	};
	if ( t.isEmpty() )
		return refuse( QStringLiteral( "swap material has no diffuse" ) );
	if ( t.startsWith( QLatin1Char( '/' ) ) || t.startsWith( QLatin1Char( '\\' ) ) || t.contains( QLatin1Char( ':' ) ) )
		return refuse( QStringLiteral( "swap diffuse is not portable" ) );
	t.replace( QLatin1Char( '/' ), QLatin1Char( '\\' ) );
	QStringList segs;
	for ( const QString & s : t.split( QLatin1Char( '\\' ) ) ) {
		if ( s.isEmpty() || s == QLatin1String( "." ) )
			continue;
		if ( s == QLatin1String( ".." ) )
			return refuse( QStringLiteral( "parent traversal in swap diffuse" ) );
		segs << s;
	}
	if ( !segs.isEmpty() && segs.first().compare( QLatin1String( "textures" ), Qt::CaseInsensitive ) == 0 )
		segs.removeFirst();
	if ( segs.isEmpty() )
		return refuse( QStringLiteral( "swap diffuse has no resource name" ) );
	const QString file = segs.takeLast();
	if ( !file.endsWith( QLatin1String( "_d.dds" ), Qt::CaseInsensitive ) )
		return refuse( QStringLiteral( "swap diffuse does not end in _d.dds" ) );
	const QString stem = file.left( file.size() - 6 );
	if ( stem.isEmpty() )
		return refuse( QStringLiteral( "swap diffuse has an empty stem" ) );
	auto make = [&stem]( const QStringList & dir ) {
		QString c = QStringLiteral( "Materials\\" );
		if ( !dir.isEmpty() )
			c += dir.join( QLatin1Char( '\\' ) ) + QLatin1Char( '\\' );
		return c + stem + QLatin1String( ".pbrm" );
	};
	QStringList out { make( segs ) };
	if ( segs.size() >= 2 ) {
		QStringList parent = segs;
		parent.removeLast();
		const QString pc = make( parent );
		if ( pc != out.first() )
			out << pc;
	}
	return out;
}

bool pbrmFromFo76Bgsm( const QByteArray & bgsm, PbrmMaterial & out, QString & why )
{
	out = PbrmMaterial();
	const ShaderMaterial sm( bgsm );
	if ( !sm.isValid() ) {
		why = QStringLiteral( "not a readable BGSM" );
		return false;
	}
	const quint32 v = sm.fileVersion();
	if ( v < 20 || v > 22 ) {
		why = QStringLiteral( "BGSM version %1 is not Fallout 76 (20-22)" ).arg( v );
		return false;
	}
	if ( !( sm.shaderFlags2() & 0x20U ) ) {
		why = QStringLiteral( "BGSM v%1 without the PBR flag" ).arg( v );
		return false;
	}
	const QStringList & t = sm.textures();
	if ( t.isEmpty() || t.at( 0 ).trimmed().isEmpty() ) {
		why = QStringLiteral( "FO76 BGSM has no base colour texture" );
		return false;
	}

	out.envelopeVersion = 0;
	out.shader = QStringLiteral( "Standard" );
	auto slot = [&]( PbrmMaterial::Slot & s, const QString & path ) {
		s.path = path;
		s.enabled = !path.trimmed().isEmpty();
		s.pathValid = s.enabled && pbrmNormalisePath( path, s.lookupPath );
	};
	slot( out.baseColor, t.at( 0 ) );
	if ( t.size() > 1 )
		slot( out.normal, t.at( 1 ) );
	out.overrideColor = false;
	out.overrideNormal = !out.normal.pathValid;
	out.roughness = qBound( 0.0f, 1.0f - sm.smoothness(), 1.0f );
	out.metallic = 0.0f;
	out.ao = 1.0f;
	out.f0 = 0.04f;
	quint32 f = 0;
	if ( out.baseColor.pathValid )
		f |= PbrmMaterial::BaseColorTexture;
	if ( out.normal.pathValid )
		f |= PbrmMaterial::NormalTexture;
	out.features = f;
	out.ok = out.baseColor.pathValid;
	if ( !out.ok ) {
		why = QStringLiteral( "FO76 base colour path is unusable" );
		return false;
	}
	return true;
}

PbrmResolveResult pbrmResolve( const PbrmResolveInput & in, const PbrmReader & read )
{
	PbrmResolveResult r;
	r.envelope = QStringLiteral( "none" );
	const QList<PbrmRoute> order = in.order.isEmpty() ? pbrmDefaultOrder() : in.order;

	// The property name, prepared once. lodgen's TXST MNAM may be an absolute
	// authoring path; it is cut at its last `materials\` first (lodgen rule).
	QString mat = in.material.trimmed();
	if ( in.cutAuthoringPath && !mat.isEmpty() ) {
		QString m2 = mat;
		m2.replace( QLatin1Char( '/' ), QLatin1Char( '\\' ) );
		const int mi = m2.lastIndexOf( QLatin1String( "materials\\" ), -1, Qt::CaseInsensitive );
		if ( mi > 0 )
			m2.remove( 0, mi );
		mat = m2;
	}
	const bool namesPbrm = mat.endsWith( QLatin1String( ".pbrm" ), Qt::CaseInsensitive );
	const bool namesBgsx = mat.endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive )
		|| mat.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive );
	const bool namesBgsm = mat.endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive );

	QStringList notes;	// why a step had no candidate at all
	auto tryPbrm = [&]( PbrmRoute route, const QString & path ) -> bool {
		const QString tag = QStringLiteral( "%1 %2: " ).arg( QLatin1String( pbrmRouteName( route ) ), path );
		QByteArray bytes;
		if ( !read( path, bytes ) || bytes.isEmpty() ) {
			r.tried << tag + QStringLiteral( "not found" );
			return false;
		}
		PbrmMaterial m = pbrmParse( bytes );
		if ( !m.error.isEmpty() ) {
			r.tried << tag + QStringLiteral( "malformed (%1)" ).arg( m.error );
			return false;
		}
		if ( !m.ok ) {
			r.tried << tag + QStringLiteral( "unsupported (%1)" ).arg( m.diagnostics.join( QStringLiteral( "; " ) ) );
			return false;
		}
		r.tried << tag + QStringLiteral( "served" );
		r.route = route;
		r.path = path;
		r.envelope = QStringLiteral( "v%1" ).arg( m.envelopeVersion );
		r.material = m;
		return true;
	};

	for ( PbrmRoute step : order ) {
		switch ( step ) {
		case PbrmRoute::Swap: {
			if ( in.swapDiffuse.isEmpty() ) {
				if ( !in.swapNote.isEmpty() )
					notes << QStringLiteral( "swap: " ) + in.swapNote;
				break;
			}
			QString why;
			const QStringList cands = pbrmSwapCandidates( in.swapDiffuse, &why );
			if ( cands.isEmpty() )
				notes << QStringLiteral( "swap: " ) + why;
			for ( const QString & c : cands )
				if ( tryPbrm( PbrmRoute::Swap, c ) )
					return r;
			break;
		}
		case PbrmRoute::Nifx: {
			if ( in.nifxPbrm.isEmpty() ) {
				if ( !in.nifxNote.isEmpty() )
					notes << QStringLiteral( "nifx: " ) + in.nifxNote;
				break;
			}
			QString c, why;
			if ( !pbrmNormaliseMaterialPath( in.nifxPbrm, { QStringLiteral( ".pbrm" ) }, c, &why ) ) {
				notes << QStringLiteral( "nifx: entry \"%1\" refused: %2" ).arg( in.nifxPbrm, why );
				break;
			}
			if ( tryPbrm( PbrmRoute::Nifx, c ) )
				return r;
			break;
		}
		case PbrmRoute::Direct: {
			if ( !namesPbrm )
				break;
			QString c, why;
			if ( !pbrmNormaliseMaterialPath( mat, { QStringLiteral( ".pbrm" ) }, c, &why ) ) {
				notes << QStringLiteral( "direct: \"%1\" refused: %2" ).arg( mat, why );
				break;
			}
			if ( tryPbrm( PbrmRoute::Direct, c ) )
				return r;
			break;
		}
		case PbrmRoute::Sibling: {
			if ( !namesBgsx )
				break;
			if ( !in.sibling ) {
				notes << QStringLiteral( "sibling: auto-replace off" );
				break;
			}
			QString c, why;
			if ( !pbrmNormaliseMaterialPath( mat, { QStringLiteral( ".bgsm" ), QStringLiteral( ".bgem" ) }, c, &why ) ) {
				notes << QStringLiteral( "sibling: \"%1\" refused: %2" ).arg( mat, why );
				break;
			}
			c.chop( 5 );
			c += QLatin1String( ".pbrm" );
			if ( tryPbrm( PbrmRoute::Sibling, c ) )
				return r;
			break;
		}
		case PbrmRoute::Fo76: {
			if ( !in.fo76 || !namesBgsm )
				break;
			QString c, why;
			if ( !pbrmNormaliseMaterialPath( mat, { QStringLiteral( ".bgsm" ) }, c, &why ) )
				break;
			QByteArray bytes;
			const QString tag = QStringLiteral( "fo76 %1: " ).arg( c );
			if ( !read( c, bytes ) || bytes.isEmpty() ) {
				r.tried << tag + QStringLiteral( "not found" );
				break;
			}
			PbrmMaterial m;
			if ( !pbrmFromFo76Bgsm( bytes, m, why ) ) {
				r.tried << tag + why;
				break;
			}
			r.tried << tag + QStringLiteral( "served" );
			r.route = PbrmRoute::Fo76;
			r.path = c;
			r.material = m;
			r.envelope = QStringLiteral( "bgsm-v%1" ).arg( ShaderMaterial( bytes ).fileVersion() );
			return r;
		}
		case PbrmRoute::Stem: {
			if ( !mat.isEmpty() || in.stemDiffuse.isEmpty() )
				break;
			const QString lodm = lodmSourceCandidate( QString(), in.stemDiffuse );
			if ( !lodm.endsWith( QLatin1String( ".lodm" ), Qt::CaseInsensitive ) )
				break;
			QString c = lodm.left( lodm.length() - 5 ) + QLatin1String( ".pbrm" );
			c.replace( QLatin1Char( '/' ), QLatin1Char( '\\' ) );
			if ( tryPbrm( PbrmRoute::Stem, c ) )
				return r;
			break;
		}
		default:
			break;
		}
	}

	// Legacy. Name every reason, in order: the declines first, then the steps
	// that had no candidate.
	r.route = PbrmRoute::Legacy;
	r.material = PbrmMaterial();
	QStringList why;
	for ( const QString & t : r.tried )
		why << t;
	why << notes;
	if ( why.isEmpty() ) {
		if ( mat.isEmpty() )
			why << QStringLiteral( "embedded shader, no material name" );
		else if ( !namesPbrm && !namesBgsx )
			why << QStringLiteral( "no .pbrm candidate for \"%1\"" ).arg( mat );
		else
			why << QStringLiteral( "no candidate" );
	}
	r.refusal = why.join( QStringLiteral( "; " ) );
	return r;
}
