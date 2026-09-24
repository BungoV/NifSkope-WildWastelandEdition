#include "filestab.h"

#include "glview.h"
#include "hkxplayback.h"
#include "wwskin.h"
#include "gl/glscene.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QTemporaryDir>

/*
 *  WHICH FILES THE TAB LISTS
 */

const QStringList & wwFilesTabExtensions()
{
	/* One list, so the predicate below, the harness and any later caller
	 * cannot drift apart. Lower case with the dot; BA2File hands us lower-cased
	 * paths already, and the loose-file walk lower-cases too, but the compare
	 * below folds case anyway rather than relying on that.
	 */
	static const QStringList exts = {
		QStringLiteral( ".nif" ),		// the documents
		QStringLiteral( ".bto" ),		// object LOD chunks
		QStringLiteral( ".btr" ),		// terrain LOD chunks
		QStringLiteral( ".hkx" ),		// Havok animation clips and skeletons
		QStringLiteral( ".gltf" ),		// the interchange format (HKX4b/HKX5b)
		QStringLiteral( ".lodl" ),		// our land container
		QStringLiteral( ".lodt" )		// our terrain-texture container
	};
	return exts;
}

bool wwFilesTabAccepts( const std::string_view & path )
{
	for ( const QString & ext : wwFilesTabExtensions() ) {
		const QByteArray e = ext.toLatin1();
		if ( path.size() < size_t( e.size() ) )
			continue;
		// case-folded on the ASCII suffix only: an archive path is bytes, and
		// a QString round trip per file over 200,000 entries is not free
		bool same = true;
		const size_t off = path.size() - size_t( e.size() );
		for ( int i = 0; i < e.size() && same; i++ ) {
			char a = path[off + size_t( i )];
			if ( a >= 'A' && a <= 'Z' )
				a = char( a - 'A' + 'a' );
			same = ( a == e.at( i ) );
		}
		if ( same )
			return true;
	}
	return false;
}

bool wwFilesTabIsAnimation( const QString & path )
{
	return path.endsWith( QStringLiteral( ".hkx" ), Qt::CaseInsensitive );
}

/*
 *  THE MARK A CLIP CARRIES IN THE LOADED-FILES LIST
 */

QIcon wwFilesTabClipIcon()
{
	/* Rebuilt whenever the skin changed under us: the colour comes from the
	 * shared table, so a cached pixmap painted in the old theme would be the
	 * one control on the page that did not follow a theme switch.
	 */
	static QIcon cached;
	static QString cachedInk;
	const QString ink = wwSkinColor( "toggle" );
	if ( !cached.isNull() && ink == cachedInk )
		return cached;

	const int s = 16;
	QPixmap pm( s, s );
	pm.fill( Qt::transparent );
	QPainter p( &pm );
	p.setRenderHint( QPainter::Antialiasing, true );
	QPainterPath tri;
	tri.moveTo( 4.5, 3.0 );
	tri.lineTo( 12.5, 8.0 );
	tri.lineTo( 4.5, 13.0 );
	tri.closeSubpath();
	p.fillPath( tri, QColor( ink ) );
	p.end();

	cached = QIcon( pm );
	cachedInk = ink;
	return cached;
}

/*
 *  OPENING AN .hkx FROM THE TREE
 */

namespace
{

//! One temporary folder for the whole session's archive-staged clips.
QTemporaryDir * wwClipStage()
{
	static QTemporaryDir * dir = nullptr;
	if ( !dir )
		dir = new QTemporaryDir( QDir::tempPath() + QStringLiteral( "/ww-hkx-XXXXXX" ) );
	return ( dir && dir->isValid() ) ? dir : nullptr;
}

} // namespace

WwAnimOpen wwFilesTabOpenAnimation( GLView * ogl, const QString & diskPath,
									const QByteArray & bytes, const QString & label )
{
	WwAnimOpen r;
	const QString shown = label.isEmpty()
		? QFileInfo( diskPath ).fileName()
		: QFileInfo( label ).fileName();

	Scene * sc = ogl ? ogl->getScene() : nullptr;
	if ( !sc || !sc->hkx ) {
		r.sentence = QObject::tr( "%1 was not loaded: the 3D view has no scene to play "
								  "it on." ).arg( shown );
		return r;
	}

	/* "IS A MODEL LOADED" IS MEASURED, NOT GUESSED.
	 *
	 * The question a clip actually asks is "are there named nodes to bind to",
	 * and HkxPlayback::mapNames already counts exactly that. An empty list of
	 * bone names maps nothing and still fills in nodesInNif, so the refusal
	 * below quotes the same instrument the summary line quotes -- there is no
	 * second idea of "loaded" that could disagree with it.
	 */
	r.nodesInNif = HkxPlayback::mapNames( sc, QStringList() ).nodesInNif;
	if ( r.nodesInNif == 0 ) {
		r.sentence = QObject::tr( "%1 is an animation, not a model: there is nothing "
								  "open for it to play on. Open the rigged NIF first, "
								  "then open the animation." ).arg( shown );
		return r;
	}

	/* Lane HKX1's reader takes a PATH, so a clip that lives inside a .ba2 is
	 * staged to a temporary file first. The path is the only thing lost: the
	 * skeleton search's "beside the clip" and "CharacterAssets above it" arms
	 * cannot fire from a temporary folder, and resolveNames() falls through to
	 * the skeletons already loaded this session and then to the game archives,
	 * naming which arm served (CONSTITUTION rule 10).
	 */
	QString path = diskPath;
	if ( path.isEmpty() ) {
		QTemporaryDir * stage = wwClipStage();
		if ( !stage ) {
			r.sentence = QObject::tr( "%1 could not be staged: no writable temporary "
									  "folder." ).arg( shown );
			return r;
		}
		path = stage->filePath( shown );
		QFile f( path );
		if ( !f.open( QIODevice::WriteOnly ) || f.write( bytes ) != bytes.size() ) {
			r.sentence = QObject::tr( "%1 could not be staged to %2." )
				.arg( shown, QDir::toNativeSeparators( path ) );
			return r;
		}
		f.close();
	}

	QStringList added;
	const QString err = sc->hkx->load( path, &added );
	if ( !err.isEmpty() ) {
		// the reader's own refusal sentence, verbatim -- never reworded here
		r.sentence = err;
		return r;
	}

	if ( added.isEmpty() ) {
		// skeleton.hkx is a legitimate thing to open: it carries no clip, its
		// bones are remembered for clips that need names, and summary() says so
		r.sentence = sc->hkx->summary();
		return r;
	}

	r.loaded = true;
	r.name = added.first();
	// the same two calls the "Load Animation (.hkx)..." button makes
	ogl->setSceneSequence( r.name );
	emit ogl->sequencesUpdated();
	r.sentence = sc->hkx->summary();
	return r;
}

bool wwFilesTabUnloadAnimation( GLView * ogl, const QString & clipName )
{
	Scene * sc = ogl ? ogl->getScene() : nullptr;
	if ( !sc || !sc->hkx || clipName.isEmpty() )
		return false;
	if ( !sc->hkx->has( clipName ) )
		return false;

	/* Order matters. unload() restores every node this clip posed, byte for
	 * byte, and drops it from Scene::animGroups; only then is it safe to ask
	 * the view for another sequence, because "(no sequence)" against a list
	 * that still holds the dead name would re-bind it.
	 */
	const bool gone = sc->hkx->unload( clipName );
	if ( gone ) {
		emit ogl->sequencesUpdated();
		ogl->update();
	}
	return gone;
}
