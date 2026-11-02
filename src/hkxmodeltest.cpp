// WW_HKXMODEL_TEST: the in-application gate of lane HKXEDIT1, the RAW layer -- an .hkx opened as a document is
// a block tree in the Blocks tab, every field editable like a NIF block, saved through the packfile writer.

#include "hkxmodel.h"
#include "wwswitch.h"	// the WW_* switch store
#include "hkxfile.h"
#include "nifskope.h"
#include "model/nifmodel.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QTreeView>

namespace {

struct WwHkxModelState
{
	QTextStream * out = nullptr;
	int checks = 0, fails = 0, skips = 0;
	QString outDir;
	void check( bool ok, const QString & what )
	{
		checks++;
		if ( !ok ) fails++;
		*out << ( ok ? "  ok   " : "  FAIL " ) << what << "\n";
	}
	void say( const QString & s ) { *out << s << "\n"; }
	void skip( const QString & why ) { skips++; *out << "  SKIP " << why << "\n"; }
};

static QByteArray readAll( const QString & p )
{
	QFile f( p );
	return f.open( QIODevice::ReadOnly ) ? f.readAll() : QByteArray();
}

static QModelIndex findChildNamed( const HkxModel * m, const QModelIndex & parent, const QString & name )
{
	for ( int r = 0; r < m->rowCount( parent ); r++ ) {
		const QModelIndex i = m->index( r, 0, parent );
		if ( i.data( Qt::DisplayRole ).toString() == name )
			return i;
	}
	return QModelIndex();
}

} // namespace

void wwHkxModelHarness( NifSkope * skope )
{
	if ( !skope || !wwEnvIsSet( "WW_HKXMODEL_TEST" ) )
		return;
	auto * st = new WwHkxModelState;
	st->outDir = wwEnvStr( "WW_HKXMODEL_OUT" );
	if ( st->outDir.isEmpty() )
		st->outDir = QApplication::applicationDirPath();

	QObject::connect( skope, &NifSkope::completeLoading, skope, [skope, st]( bool ok, QString & fname ) {
		const QString loaded = fname;
		QTimer::singleShot( 1500, skope, [skope, st, ok, loaded]() {
			QFile logf( QApplication::applicationDirPath() + "/ww_hkxmodel_test.log" );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
				return;
			QTextStream log( &logf );
			st->out = &log;
			st->say( "WW_HKXMODEL_TEST on " + loaded );
			st->check( ok, "the document loaded" );

			QTreeView * tree = skope->findChild<QTreeView *>( QStringLiteral( "tree" ) );
			HkxModel * hkx = tree ? qobject_cast<HkxModel *>( tree->model() ) : nullptr;
			st->check( tree != nullptr, "the Block Details view `tree` exists" );
			st->check( hkx != nullptr, "(a) the tree's model is an HkxModel after an .hkx load" );
			if ( !hkx ) {
				st->say( QStringLiteral( "the tree's model is %1" ).arg( tree && tree->model() ? QString::fromLatin1( tree->model()->metaObject()->className() ) : QStringLiteral( "null" ) ) );
			} else {
				const QByteArray src = readAll( loaded );
				st->check( hkx->objectCount() == 6, QStringLiteral( "(a) 6 blocks (%1)" ).arg( hkx->objectCount() ) );
				QStringList names;
				for ( int i = 0; i < hkx->objectCount(); i++ )
					names << hkx->objectClass( i );
				st->check( names.value( 0 ) == QLatin1String( "hkRootLevelContainer" ) && names.value( 2 ) == QLatin1String( "hkaSplineCompressedAnimation" )
					&& names.value( 4 ) == QLatin1String( "hkaAnimationBinding" ), "(a) blocks are the packfile's classes in file order: " + names.join( QLatin1String( ", " ) ) );
				// (d) the animation layer's view, unedited
				const HkxAnimFile af = hkx->animFile();
				st->check( af.ok() && af.clips.size() == 1 && af.clips[0].numFrames == 23,
					QStringLiteral( "(d) animFile() decodes the document: %1" ).arg( af.ok() ? QStringLiteral( "%1 clip(s), numFrames %2" ).arg( af.clips.size() ).arg( af.clips.value( 0 ).numFrames ) : af.error ) );
				// (c) the unedited save
				const QString outA = st->outDir + QLatin1String( "/hkxmodel_unedited.hkx" );
				st->check( hkx->saveToFile( outA ), "(c) saveToFile unedited" );
				const QByteArray a = readAll( outA );
				st->check( !src.isEmpty() && a == src, QStringLiteral( "(c) unedited save is byte-identical to the source (%1 / %2 bytes)" ).arg( a.size() ).arg( src.size() ) );
				// (b) the edit through the model
				st->check( hkx->undoStack && hkx->undoStack->isClean(), "(b) floor: the undo stack is clean before the edit" );
				const QModelIndex anim = hkx->objectIndex( 2 );
				const QModelIndex numFrames = findChildNamed( hkx, anim, QStringLiteral( "numFrames" ) );
				st->check( numFrames.isValid(), "(b) the animation block has a numFrames row" );
				if ( numFrames.isValid() ) {
					const QModelIndex cell = numFrames.sibling( numFrames.row(), BaseModel::ValueCol );
					NifValue v = cell.data( Qt::EditRole ).value<NifValue>();
					st->check( v.toCount( nullptr, nullptr ) == 23, QStringLiteral( "(b) numFrames reads 23 before (%1)" ).arg( v.toCount( nullptr, nullptr ) ) );
					v.setCount( 24, nullptr, nullptr );
					st->check( hkx->setData( cell, QVariant::fromValue( v ), Qt::EditRole ), "(b) setData numFrames = 24" );
					st->check( cell.data( Qt::EditRole ).value<NifValue>().toCount( nullptr, nullptr ) == 24, "(b) numFrames reads 24 after" );
					st->check( hkx->undoStack && !hkx->undoStack->isClean(), "(b) the undo stack is dirty after the edit" );
					if ( hkx->undoStack ) {
						hkx->undoStack->undo();
						st->check( cell.data( Qt::EditRole ).value<NifValue>().toCount( nullptr, nullptr ) == 23, "(b) undo reads 23" );
						hkx->undoStack->redo();
						st->check( cell.data( Qt::EditRole ).value<NifValue>().toCount( nullptr, nullptr ) == 24, "(b) redo reads 24" );
					}
					// (c) floor: the edited save differs, inside the animation object, and reads back 24
					const QString outB = st->outDir + QLatin1String( "/hkxmodel_edited.hkx" );
					st->check( hkx->saveToFile( outB ), "(c) saveToFile edited" );
					const QByteArray b = readAll( outB );
					int d = -1;
					for ( int i = 0; i < qMin( a.size(), b.size() ); i++ ) if ( a[i] != b[i] ) { d = i; break; }
					st->check( d >= 0 && b.size() == a.size(), QStringLiteral( "(c) floor: the edited save differs at 0x%1, same size" ).arg( d, 0, 16 ) );
					QString err;
					const Hkx::ClassDb * db = Hkx::ClassDb::instance( &err );
					Hkx::File f;
					if ( db && f.read( b, *db ) ) {
						const Hkx::Value * nv = f.find( QStringLiteral( "#2.numFrames" ), &err );
						st->check( nv && nv->toInt() == 24, QStringLiteral( "(c) reloading the edited bytes reads numFrames = %1" ).arg( nv ? nv->toInt() : -1 ) );
						st->check( f.objects.size() > 2 && d >= f.dataStart + f.objects[2].fileOffset && d < f.dataStart + f.objects[2].fileOffset + f.objects[2].cls->objectSize,
							QStringLiteral( "(c) the difference lies inside the animation object (@0x%1 + %2)" ).arg( f.objects.value( 2 ).fileOffset, 0, 16 ).arg( f.objects.value( 2 ).cls ? f.objects[2].cls->objectSize : 0 ) );
					} else {
						st->check( false, "(c) the edited bytes reload through Hkx::File: " + ( db ? f.error : err ) );
					}
					hkx->undoStack->setClean();
				}
			}
			log << st->checks << " checks, " << st->fails << " failures, " << st->skips << " skips\n";
			log << ( st->fails == 0 ? "PASS" : "FAIL" ) << "\n";
			log << "done\n";
			log.flush();
			logf.close();
			if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
				n->undoStack->setClean();
			skope->setWindowModified( false );
			QTimer::singleShot( 100, qApp, &QApplication::quit );
		} );
	} );
}
