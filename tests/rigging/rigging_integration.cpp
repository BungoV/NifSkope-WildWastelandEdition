#include "spellbook.h"
#include "nifskope.h"
#include "glview.h"
#include "data/nifvalue.h"
#include "model/nifmodel.h"

#include <QApplication>
#include <QAction>
#include <QAbstractButton>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QDebug>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>
#include <QMenu>
#include <QLineEdit>
#include <QInputDialog>
#include <QLabel>
#include <QProgressDialog>
#include <QSet>
#include <QSettings>
#include <QSlider>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUndoStack>

#include <cmath>
#include <cstdio>
#include <limits>

static void checkpoint( const char * message )
{
	std::fprintf( stderr, "%s\n", message );
	std::fflush( stderr );
}

class DialogDriver final : public QObject
{
public:
	explicit DialogDriver( QString donor ) : donorPath( std::move( donor ) ) {}

	QStringList messages;
	int fileDialogs = 0;
	int inputDialogs = 0;
	int progressDialogs = 0;
	int donorShapeChoice = 0;
	bool cancelNextFileDialog = false;
	bool cancelNextProgress = false;
	bool rejectNextQuestion = false;
	void setDonorPath( const QString & path ) { donorPath = path; }

protected:
	bool eventFilter( QObject * watched, QEvent * event ) override
	{
		if ( event->type() != QEvent::Show )
			return QObject::eventFilter( watched, event );
		qInfo().noquote() << "SHOW" << watched->metaObject()->className()
			<< watched->property( "windowTitle" ).toString();

		if ( auto * dialog = qobject_cast<QFileDialog *>( watched ) ) {
			fileDialogs++;
			// Integration fixtures are immutable. The harness never needs a GUI
			// save dialog, so reject one instead of feeding it the donor path.
			if ( dialog->acceptMode() == QFileDialog::AcceptSave ) {
				QTimer::singleShot( 0, dialog, &QDialog::reject );
				return QObject::eventFilter( watched, event );
			}
			if ( cancelNextFileDialog ) {
				cancelNextFileDialog = false;
				QTimer::singleShot( 0, dialog, &QDialog::reject );
				return QObject::eventFilter( watched, event );
			}
			QTimer::singleShot( 0, dialog, [this, dialog]() {
				QFileInfo info( donorPath );
				dialog->setDirectory( info.absolutePath() );
				QTimer::singleShot( 100, dialog, [this, dialog, info]() {
					dialog->selectFile( info.fileName() );
					if ( auto * edit = dialog->findChild<QLineEdit *>( QStringLiteral( "fileNameEdit" ) ) )
						edit->setText( info.fileName() );
					qInfo().noquote() << "ACCEPT FILE" << donorPath << dialog->selectedFiles();
					QMetaObject::invokeMethod( dialog, "accept", Qt::DirectConnection );
				} );
			} );
		} else if ( auto * progress = qobject_cast<QProgressDialog *>( watched ) ) {
			progressDialogs++;
			if ( cancelNextProgress ) {
				cancelNextProgress = false;
				QTimer::singleShot( 0, progress, &QProgressDialog::cancel );
			}
		} else if ( auto * input = qobject_cast<QInputDialog *>( watched ) ) {
			inputDialogs++;
			QTimer::singleShot( 0, input, [this, input]() {
				QComboBox * combo = input->findChild<QComboBox *>();
				if ( !combo || combo->count() == 0 ) {
					input->reject();
					return;
				}
				combo->setCurrentIndex( qBound( 0, donorShapeChoice, combo->count() - 1 ) );
				input->accept();
			} );
		} else if ( auto * box = qobject_cast<QMessageBox *>( watched ) ) {
			messages << box->windowTitle() + QStringLiteral( ": " ) + box->text()
				+ ( box->informativeText().isEmpty() ? QString() : QStringLiteral( "\n" ) + box->informativeText() )
				+ ( box->detailedText().isEmpty() ? QString() : QStringLiteral( "\n" ) + box->detailedText() );
			QTimer::singleShot( 0, box, [this, box]() {
				if ( rejectNextQuestion ) {
					QAbstractButton * reject = box->button( QMessageBox::No );
					if ( !reject ) reject = box->button( QMessageBox::Cancel );
					if ( reject ) {
						rejectNextQuestion = false;
						reject->click();
						return;
					}
				}
				qInfo().noquote() << "CLOSE MESSAGE" << box->windowTitle() << box->text();
				if ( auto * discard = box->button( QMessageBox::Discard ) )
					discard->click();
				else if ( auto * yes = box->button( QMessageBox::Yes ) )
					yes->click();
				else if ( auto * ok = box->button( QMessageBox::Ok ) )
					ok->click();
				else
					box->accept();
			} );
		}
		return QObject::eventFilter( watched, event );
	}

private:
	QString donorPath;
};

static QModelIndex findShape( const NifModel & nif )
{
	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		QModelIndex block = nif.getBlockIndex( b );
		if ( nif.blockInherits( block, "BSTriShape" ) && nif.getIndex( block, "Vertex Data" ).isValid() )
			return block;
	}
	return {};
}

static QList<QModelIndex> findShapes( const NifModel & nif )
{
	QList<QModelIndex> result;
	for ( int b = 0; b < nif.getBlockCount(); b++ ) {
		QModelIndex block = nif.getBlockIndex( b );
		if ( nif.blockInherits( block, "BSTriShape" ) && nif.getIndex( block, "Vertex Data" ).isValid()
			&& nif.getBlockIndex( nif.getLink( block, "Skin" ) ).isValid() )
			result << block;
	}
	return result;
}

static QStringList boneNames( const NifModel & nif, const QModelIndex & shape )
{
	QStringList names;
	QModelIndex skin = nif.getBlockIndex( nif.getLink( shape, "Skin" ) );
	QModelIndex bones = nif.getIndex( skin, "Bones" );
	for ( int i = 0; i < nif.rowCount( bones ); i++ ) {
		QModelIndex node = nif.getBlockIndex( nif.getLink( nif.getIndex( bones, i ) ) );
		names << nif.get<QString>( node, "Name" );
	}
	return names;
}

static int countNodes( const NifModel & nif )
{
	int count = 0;
	for ( int b = 0; b < nif.getBlockCount(); b++ )
		if ( nif.blockInherits( nif.getBlockIndex( b ), "NiNode" ) ) count++;
	return count;
}

static bool skinCounts( const NifModel & nif, const QModelIndex & shape, int expected )
{
	QModelIndex skin = nif.getBlockIndex( nif.getLink( shape, "Skin" ) );
	QModelIndex bones = nif.getIndex( skin, "Bones" );
	QModelIndex data = nif.getBlockIndex( nif.getLink( skin, "Data" ) );
	QModelIndex list = nif.getIndex( data, "Bone List" );
	return nif.get<quint32>( skin, "Num Bones" ) == quint32( expected )
		&& nif.rowCount( bones ) == expected
		&& nif.get<quint32>( data, "Num Bones" ) == quint32( expected )
		&& nif.rowCount( list ) == expected;
}

static bool normalizedWeights( const NifModel & nif, const QModelIndex & shape )
{
	QStringList names = boneNames( nif, shape );
	QModelIndex vertices = nif.getIndex( shape, "Vertex Data" );
	for ( int v = 0; v < nif.rowCount( vertices ); v++ ) {
		QModelIndex vertex = nif.getIndex( vertices, v );
		QModelIndex weights = nif.getIndex( vertex, "Bone Weights" );
		QModelIndex indices = nif.getIndex( vertex, "Bone Indices" );
		if ( !weights.isValid() || !indices.isValid() || nif.rowCount( weights ) != nif.rowCount( indices ) )
			return false;
		double sum = 0.0;
		for ( int s = 0; s < nif.rowCount( weights ); s++ ) {
			double weight = nif.get<float>( nif.getIndex( weights, s ) );
			int bone = nif.get<quint8>( nif.getIndex( indices, s ) );
			if ( !std::isfinite( weight ) || weight < 0.0 || ( weight > 0.0 && bone >= names.size() ) )
				return false;
			sum += weight;
		}
		if ( std::fabs( sum - 1.0 ) > 0.002 )
			return false;
	}
	return true;
}

static double boneWeightAt( const NifModel & nif, const QModelIndex & shape, int vertex, int bone )
{
	QModelIndex vertices = nif.getIndex( shape, "Vertex Data" );
	QModelIndex item = nif.getIndex( vertices, vertex );
	QModelIndex weights = nif.getIndex( item, "Bone Weights" );
	QModelIndex indices = nif.getIndex( item, "Bone Indices" );
	double result = 0.0;
	for ( int slot = 0; slot < qMin( nif.rowCount( weights ), nif.rowCount( indices ) ); slot++ )
		if ( nif.get<quint8>( nif.getIndex( indices, slot ) ) == bone )
			result += nif.get<float>( nif.getIndex( weights, slot ) );
	return result;
}

static QByteArray remapData( const NifModel & nif, const QModelIndex & shape )
{
	for ( int link : nif.getChildLinks( nif.getBlockNumber( shape ) ) ) {
		QModelIndex extra = nif.getBlockIndex( link, "NiBinaryExtraData" );
		if ( extra.isValid() && nif.get<QString>( extra, "Name" ) == QStringLiteral( "CustomizationRemapData" ) )
			return nif.get<QByteArray>( extra, "Binary Data" );
	}
	return {};
}

static QByteArray serializeModel( NifModel & nif )
{
	QByteArray data;
	QBuffer buffer( &data );
	if ( !buffer.open( QIODevice::WriteOnly ) || !nif.save( buffer ) )
		return {};
	return data;
}

static QByteArray readFileBytes( const QString & path )
{
	QFile file( path );
	return file.open( QIODevice::ReadOnly ) ? file.readAll() : QByteArray();
}

static bool runSpell( NifModel & nif, const QString & id, DialogDriver & driver, QString * error )
{
	qInfo().noquote() << "RUN" << id;
	QModelIndex shape = findShape( nif );
	SpellPtr spell = SpellBook::lookup( id );
	if ( !shape.isValid() || !spell ) {
		*error = QStringLiteral( "Missing shape or spell: " ) + id;
		return false;
	}
	if ( !spell->isApplicable( &nif, shape ) ) {
		*error = QStringLiteral( "Spell not applicable: " ) + id;
		return false;
	}
	int beforeMessages = driver.messages.size();
	spell->cast( &nif, shape );
	qInfo().noquote() << "RETURN" << id;
	QCoreApplication::processEvents();
	if ( driver.messages.size() == beforeMessages ) {
		*error = QStringLiteral( "Spell produced no result dialog: " ) + id;
		return false;
	}
	return true;
}

static bool compareWeights( const NifModel & target, const QModelIndex & targetShape,
	const NifModel & donor, const QModelIndex & donorShape, double * meanL1, double * maxL1 )
{
	QStringList targetBones = boneNames( target, targetShape );
	QStringList donorBones = boneNames( donor, donorShape );
	QModelIndex targetVD = target.getIndex( targetShape, "Vertex Data" );
	QModelIndex donorVD = donor.getIndex( donorShape, "Vertex Data" );
	if ( target.rowCount( targetVD ) != donor.rowCount( donorVD ) )
		return false;

	double total = 0.0, maximum = 0.0;
	for ( int v = 0; v < target.rowCount( targetVD ); v++ ) {
		QMap<QString, double> a, b;
		auto gather = []( const NifModel & nif, const QModelIndex & vertex, const QStringList & names,
			QMap<QString, double> & out ) {
			QModelIndex weights = nif.getIndex( vertex, "Bone Weights" );
			QModelIndex indices = nif.getIndex( vertex, "Bone Indices" );
			for ( int s = 0; s < qMin( nif.rowCount( weights ), nif.rowCount( indices ) ); s++ ) {
				double w = nif.get<float>( nif.getIndex( weights, s ) );
				int bi = nif.get<quint8>( nif.getIndex( indices, s ) );
				if ( w > 0.0 && bi >= 0 && bi < names.size() ) out[names[bi]] += w;
			}
		};
		gather( target, target.getIndex( targetVD, v ), targetBones, a );
		gather( donor, donor.getIndex( donorVD, v ), donorBones, b );
		QSet<QString> keys( a.keyBegin(), a.keyEnd() );
		keys.unite( QSet<QString>( b.keyBegin(), b.keyEnd() ) );
		double l1 = 0.0;
		for ( const QString & key : keys ) l1 += std::fabs( a.value( key ) - b.value( key ) );
		total += l1;
		maximum = qMax( maximum, l1 );
	}
	*meanL1 = total / double( target.rowCount( targetVD ) );
	*maxL1 = maximum;
	return true;
}

static double dominantAgreement( const NifModel & a, const QModelIndex & aShape,
	const NifModel & b, const QModelIndex & bShape )
{
	QStringList aBones = boneNames( a, aShape ), bBones = boneNames( b, bShape );
	QModelIndex aVD = a.getIndex( aShape, "Vertex Data" );
	QModelIndex bVD = b.getIndex( bShape, "Vertex Data" );
	if ( a.rowCount( aVD ) <= 0 || a.rowCount( aVD ) != b.rowCount( bVD ) )
		return -1.0;
	auto dominant = []( const NifModel & nif, const QModelIndex & vertex, const QStringList & bones ) {
		QModelIndex weights = nif.getIndex( vertex, "Bone Weights" );
		QModelIndex indices = nif.getIndex( vertex, "Bone Indices" );
		float best = -1.0f;
		QString result;
		for ( int slot = 0; slot < qMin( nif.rowCount( weights ), nif.rowCount( indices ) ); slot++ ) {
			float weight = nif.get<float>( nif.getIndex( weights, slot ) );
			int bone = nif.get<quint8>( nif.getIndex( indices, slot ) );
			if ( weight > best && bone >= 0 && bone < bones.size() ) {
				best = weight;
				result = bones.at( bone );
			}
		}
		return result;
	};
	int matches = 0;
	for ( int vertex = 0; vertex < a.rowCount( aVD ); vertex++ )
		if ( dominant( a, a.getIndex( aVD, vertex ), aBones )
			== dominant( b, b.getIndex( bVD, vertex ), bBones ) ) matches++;
	return double( matches ) / double( a.rowCount( aVD ) );
}

static int validationProblemCount( const QString & message )
{
	int marker = message.indexOf( QStringLiteral( "Found " ) );
	if ( marker < 0 )
		return 0;
	QString number = message.mid( marker + 6 ).section( QLatin1Char( ' ' ), 0, 0 );
	bool ok = false;
	int result = number.toInt( &ok );
	return ok ? result : 0;
}

static int runRealAssetQa( const QString & targetPath, const QString & donorPath,
	const QString & outputPath, const QString & referencePath, DialogDriver & driver )
{
	auto fail = []( int code, const QString & message ) {
		qCritical().noquote() << "REAL_FAIL" << code << message;
		return code;
	};
	NifModel target;
	QUndoStack undo;
	target.undoStack = &undo;
	if ( !target.loadFromFile( targetPath ) ) return fail( 80, QStringLiteral( "Could not load target" ) );
	QModelIndex shape = findShape( target );
	if ( !shape.isValid() ) return fail( 81, QStringLiteral( "No skinned target shape" ) );
	int vertices = target.rowCount( target.getIndex( shape, "Vertex Data" ) );
	int initialBones = boneNames( target, shape ).size();
	QByteArray before = serializeModel( target );
	if ( vertices <= 0 || initialBones <= 0 || before.isEmpty() )
		return fail( 82, QStringLiteral( "Unreadable target skin" ) );

	QString error;
	if ( !runSpell( target, QStringLiteral( "Rigging/Validate FO4 Skin" ), driver, &error ) )
		return fail( 83, QStringLiteral( "Could not validate the original target" ) );
	int baselineProblems = validationProblemCount( driver.messages.last() );
	int messagesBefore = driver.messages.size();
	if ( !runSpell( target, QStringLiteral( "Rigging/Transfer Bones and Weights..." ), driver, &error ) )
		return fail( 84, error );
	QString transferMessages = driver.messages.mid( messagesBefore ).join( QStringLiteral( "\n" ) );
	shape = findShape( target );
	int finalBones = boneNames( target, shape ).size();
	QByteArray after = serializeModel( target );
	if ( undo.count() != 1 || finalBones < initialBones || after.isEmpty()
		|| !normalizedWeights( target, shape )
		|| remapData( target, shape ).size() != vertices * 12
		|| !transferMessages.contains( QStringLiteral( "as one Undo step" ) ) )
		return fail( 85, QStringLiteral( "Transfer result failed structural checks" ) );

	if ( !runSpell( target, QStringLiteral( "Rigging/Validate FO4 Skin" ), driver, &error ) )
		return fail( 86, QStringLiteral( "Could not validate transferred output" ) );
	int transferredProblems = validationProblemCount( driver.messages.last() );
	if ( transferredProblems > baselineProblems )
		return fail( 87, QStringLiteral( "Transfer introduced new validation problems\n" ) + driver.messages.last() );
	if ( !target.saveToFile( outputPath ) ) return fail( 88, QStringLiteral( "Could not save output" ) );

	NifModel reloaded;
	if ( !reloaded.loadFromFile( outputPath ) ) return fail( 89, QStringLiteral( "Could not reload output" ) );
	QModelIndex reloadedShape = findShape( reloaded );
	if ( !reloadedShape.isValid() || !normalizedWeights( reloaded, reloadedShape )
		|| boneNames( reloaded, reloadedShape ).size() != finalBones
		|| remapData( reloaded, reloadedShape ).size() != vertices * 12 )
		return fail( 90, QStringLiteral( "Reloaded output failed structural checks" ) );
	if ( !runSpell( reloaded, QStringLiteral( "Rigging/Validate FO4 Skin" ), driver, &error ) )
		return fail( 91, QStringLiteral( "Could not validate reloaded output" ) );
	int reloadedProblems = validationProblemCount( driver.messages.last() );
	if ( reloadedProblems > baselineProblems )
		return fail( 92, QStringLiteral( "Reload introduced new validation problems\n" ) + driver.messages.last() );

	double meanL1 = -1.0, maxL1 = -1.0, dominant = -1.0;
	if ( !referencePath.isEmpty() ) {
		NifModel reference;
		if ( !reference.loadFromFile( referencePath ) )
			return fail( 93, QStringLiteral( "Could not load vanilla reference" ) );
		QModelIndex referenceShape = findShape( reference );
		if ( !compareWeights( reloaded, reloadedShape, reference, referenceShape, &meanL1, &maxL1 ) )
			return fail( 94, QStringLiteral( "Reference geometry is not vertex-compatible" ) );
		dominant = dominantAgreement( reloaded, reloadedShape, reference, referenceShape );
		if ( meanL1 > 0.5 || dominant < 0.75 )
			return fail( 95, QStringLiteral( "Transfer diverges unexpectedly from vanilla reference" ) );
	}

	undo.undo();
	if ( serializeModel( target ) != before ) return fail( 96, QStringLiteral( "Undo was not byte-exact" ) );
	undo.redo();
	if ( serializeModel( target ) != after ) return fail( 97, QStringLiteral( "Redo was not byte-exact" ) );

	QString quality = transferMessages.contains( QStringLiteral( "Geometry match: Close" ) )
		? QStringLiteral( "Close" ) : ( transferMessages.contains( QStringLiteral( "Geometry match: Caution" ) )
			? QStringLiteral( "Caution" ) : ( transferMessages.contains( QStringLiteral( "Geometry match: Poor" ) )
				? QStringLiteral( "Poor" ) : QStringLiteral( "Unknown" ) ) );
	qInfo().noquote() << QStringLiteral( "REAL_PASS target=%1 donor=%2 vertices=%3 bones=%4->%5 remap=%6 snap=%7 meanL1=%8 maxL1=%9 dominant=%10 validation=%11->%12" )
		.arg( QFileInfo( targetPath ).fileName(), QFileInfo( donorPath ).fileName() )
		.arg( vertices ).arg( initialBones ).arg( finalBones ).arg( vertices * 12 ).arg( quality )
		.arg( meanL1, 0, 'g', 6 ).arg( maxL1, 0, 'g', 6 ).arg( dominant, 0, 'g', 6 )
		.arg( baselineProblems ).arg( reloadedProblems );
	return 0;
}

int main( int argc, char ** argv )
{
	checkpoint( "START" );
	QCoreApplication::setAttribute( Qt::AA_DontUseNativeDialogs );
	checkpoint( "BEFORE QApplication" );
	QApplication app( argc, argv );
	checkpoint( "AFTER QApplication" );
	qInstallMessageHandler( []( QtMsgType, const QMessageLogContext &, const QString & message ) {
		QByteArray bytes = message.toLocal8Bit();
		std::fprintf( stderr, "%s\n", bytes.constData() );
		std::fflush( stderr );
	} );
	bool realMode = argc == 6 && QByteArray( argv[1] ) == QByteArrayLiteral( "--real" );
	if ( !realMode && argc != 4 ) return 2;
	checkpoint( "ARGS OK" );
	QCoreApplication::setOrganizationName( QStringLiteral( "NifToolsRiggingTest" ) );
	QCoreApplication::setApplicationName( QStringLiteral( "RiggingIntegration" ) );
	QDir::setCurrent( app.applicationDirPath() );
	qRegisterMetaType<NifValue>( "NifValue" );
	checkpoint( "BEFORE loadXML" );
	if ( !NifModel::loadXML() ) return 3;
	checkpoint( "AFTER loadXML" );
	if ( realMode ) {
		DialogDriver realDriver( QString::fromLocal8Bit( argv[3] ) );
		app.installEventFilter( &realDriver );
		return runRealAssetQa( QString::fromLocal8Bit( argv[2] ), QString::fromLocal8Bit( argv[3] ),
			QString::fromLocal8Bit( argv[4] ), QString::fromLocal8Bit( argv[5] ), realDriver );
	}

	QString targetPath = QString::fromLocal8Bit( argv[1] );
	QString donorPath = QString::fromLocal8Bit( argv[2] );
	QString outputPath = QString::fromLocal8Bit( argv[3] );
	QByteArray targetFixtureBytes = readFileBytes( targetPath );
	QByteArray donorFixtureBytes = readFileBytes( donorPath );
	if ( targetFixtureBytes.isEmpty() || donorFixtureBytes.isEmpty() ) return 6;
	DialogDriver driver( donorPath );
	app.installEventFilter( &driver );

	NifModel target;
	QUndoStack undo;
	target.undoStack = &undo;
	checkpoint( "BEFORE target load" );
	if ( !target.loadFromFile( targetPath ) ) return 4;
	checkpoint( "AFTER target load" );
	QModelIndex initialShape = findShape( target );
	if ( !initialShape.isValid() || target.getBlockCount() != 16 || countNodes( target ) != 11
		|| !skinCounts( target, initialShape, 10 ) ) return 5;
	NifModel donorProbe;
	if ( !donorProbe.loadFromFile( donorPath )
		|| boneNames( donorProbe, findShape( donorProbe ) ).size() != 68 ) return 6;
	qInfo().noquote() << "INITIAL TARGET BONES" << boneNames( target, initialShape );
	qInfo().noquote() << "DONOR BONES" << boneNames( donorProbe, findShape( donorProbe ) );

	// Derive two non-destructive fixtures through NifModel itself: a donor with
	// two selectable skinned shapes and a target with one fewer valid triangle.
	// The originals remain untouched.
	QDir fixtureDir = QFileInfo( outputPath ).dir();
	QString multiDonorPath = fixtureDir.filePath( QStringLiteral( "donor_multi.nif" ) );
	QString malformedDonorPath = fixtureDir.filePath( QStringLiteral( "donor_bad_triangle.nif" ) );
	QString topologyTargetPath = fixtureDir.filePath( QStringLiteral( "target_topology.nif" ) );
	QString topologyResultPath = fixtureDir.filePath( QStringLiteral( "result_topology.nif" ) );
	NifModel multiDonor;
	SpellPtr duplicateBlock = SpellBook::lookup( QStringLiteral( "Block/Duplicate" ) );
	if ( !duplicateBlock || !multiDonor.loadFromFile( donorPath ) ) return 40;
	QModelIndex duplicateShape = duplicateBlock->cast( &multiDonor, findShape( multiDonor ) );
	if ( !duplicateShape.isValid() ) return 41;
	multiDonor.set<QString>( duplicateShape, "Name", QStringLiteral( "Harness alternate donor" ) );
	if ( findShapes( multiDonor ).size() != 2 || !multiDonor.saveToFile( multiDonorPath ) ) return 42;
	NifModel multiDonorReloaded;
	if ( !multiDonorReloaded.loadFromFile( multiDonorPath ) || findShapes( multiDonorReloaded ).size() != 2 ) return 43;

	NifModel malformedDonor;
	if ( !malformedDonor.loadFromFile( donorPath ) ) return 62;
	QModelIndex malformedShape = findShape( malformedDonor );
	QModelIndex malformedVertices = malformedDonor.getIndex( malformedShape, "Vertex Data" );
	QModelIndex malformedTriangles = malformedDonor.getIndex( malformedShape, "Triangles" );
	if ( !malformedVertices.isValid() || !malformedTriangles.isValid()
		|| malformedDonor.rowCount( malformedTriangles ) < 1
		|| malformedDonor.rowCount( malformedVertices ) > std::numeric_limits<quint16>::max() ) return 63;
	Triangle badTriangle = malformedDonor.get<Triangle>( malformedDonor.getIndex( malformedTriangles, 0 ) );
	badTriangle[0] = quint16( malformedDonor.rowCount( malformedVertices ) );
	if ( !malformedDonor.set<Triangle>( malformedDonor.getIndex( malformedTriangles, 0 ), badTriangle )
		|| !malformedDonor.saveToFile( malformedDonorPath ) ) return 64;

	NifModel topologyTarget;
	if ( !topologyTarget.loadFromFile( targetPath ) ) return 44;
	QModelIndex topologyShape = findShape( topologyTarget );
	QModelIndex topologyVertices = topologyTarget.getIndex( topologyShape, "Vertex Data" );
	QModelIndex topologyTriangles = topologyTarget.getIndex( topologyShape, "Triangles" );
	int originalVertices = topologyTarget.rowCount( topologyVertices );
	int originalTriangles = topologyTarget.rowCount( topologyTriangles );
	if ( originalVertices < 1 || originalTriangles < 2 ) return 45;
	topologyTarget.set<quint16>( topologyShape, "Num Vertices", quint16( originalVertices + 1 ) );
	topologyTarget.set<quint32>( topologyShape, "Num Triangles", quint32( originalTriangles - 1 ) );
	if ( !topologyTarget.updateArraySize( topologyVertices )
		|| !topologyTarget.updateArraySize( topologyTriangles )
		|| topologyTarget.rowCount( topologyVertices ) != originalVertices + 1
		|| topologyTarget.rowCount( topologyTriangles ) != originalTriangles - 1
		|| !topologyTarget.saveToFile( topologyTargetPath ) ) return 46;

	QString error;
	const QStringList spells = {
		QStringLiteral( "Rigging/Generate CustomizationRemapData" ),
		QStringLiteral( "Rigging/Import Donor Bone Nodes..." ),
		QStringLiteral( "Rigging/Bind Donor Bones (existing nodes)..." ),
		QStringLiteral( "Rigging/Transfer Weights (existing bones)..." ),
		QStringLiteral( "Rigging/Validate FO4 Skin" )
	};
	for ( const QString & id : spells ) {
		if ( !runSpell( target, id, driver, &error ) ) {
			qCritical().noquote() << error;
			return 10;
		}
		qInfo().noquote() << "AFTER" << id << "BLOCKS" << target.getBlockCount()
			<< "NODES" << countNodes( target ) << "BONES" << boneNames( target, findShape( target ) );
		if ( id == QStringLiteral( "Rigging/Generate CustomizationRemapData" ) ) {
			if ( undo.count() != 1 || !runSpell( target, id, driver, &error )
				|| undo.count() != 1
				|| !driver.messages.last().contains( QStringLiteral( "already current" ) ) ) return 7;
		}
	}

	QModelIndex shape = findShape( target );
	if ( target.getBlockCount() != 76 || countNodes( target ) != 70 || !skinCounts( target, shape, 69 )
		|| remapData( target, shape ).size() != 1689 * 12 ) return 11;
	if ( driver.messages.filter( QStringLiteral( "Imported 59 minimal donor bone node(s)" ) ).isEmpty()
		|| driver.messages.filter( QStringLiteral( "Bound 59 donor bone(s)" ) ).isEmpty()
		|| driver.messages.filter( QStringLiteral( "Transferred weights onto 1689 vertices" ) ).isEmpty()
		|| driver.messages.filter( QStringLiteral( "No structural skin problems found" ) ).isEmpty() ) return 12;

	if ( undo.count() != 4 ) return 13;
	while ( undo.canUndo() ) undo.undo();
	shape = findShape( target );
	if ( target.getBlockCount() != 16 || countNodes( target ) != 11
		|| !skinCounts( target, shape, 10 ) || !remapData( target, shape ).isEmpty() ) return 14;
	while ( undo.canRedo() ) undo.redo();
	shape = findShape( target );
	if ( target.getBlockCount() != 76 || countNodes( target ) != 70
		|| !skinCounts( target, shape, 69 ) || remapData( target, shape ).size() != 1689 * 12 ) return 14;
	if ( !target.saveToFile( outputPath ) ) return 15;

	NifModel reloaded;
	if ( !reloaded.loadFromFile( outputPath ) ) return 16;
	QModelIndex reloadedShape = findShape( reloaded );
	if ( reloaded.getBlockCount() != 76 || countNodes( reloaded ) != 70
		|| !skinCounts( reloaded, reloadedShape, 69 ) || remapData( reloaded, reloadedShape ).size() != 1689 * 12 ) return 17;

	NifModel donor;
	if ( !donor.loadFromFile( donorPath ) ) return 18;
	double meanL1 = 0.0, maxL1 = 0.0;
	if ( !compareWeights( reloaded, reloadedShape, donor, findShape( donor ), &meanL1, &maxL1 )
		|| meanL1 > 0.001 || maxL1 > 0.002 ) return 19;

	if ( !runSpell( reloaded, QStringLiteral( "Rigging/Validate FO4 Skin" ), driver, &error )
		|| driver.messages.last().contains( QStringLiteral( "Found " ) ) ) return 20;

	NifModel combined;
	QUndoStack combinedUndo;
	combined.undoStack = &combinedUndo;
	if ( !combined.loadFromFile( targetPath ) ) return 21;
	int fileDialogsBefore = driver.fileDialogs;
	int combinedMessagesBefore = driver.messages.size();
	if ( !runSpell( combined, QStringLiteral( "Rigging/Transfer Bones and Weights..." ), driver, &error ) ) return 22;
	QModelIndex combinedShape = findShape( combined );
	QString combinedMessages = driver.messages.mid( combinedMessagesBefore ).join( QStringLiteral( "\n" ) );
	if ( driver.fileDialogs != fileDialogsBefore + 1
		|| !combinedMessages.contains( QStringLiteral( "Target geometry: 1689 vertices, 3230 triangles" ) )
		|| !combinedMessages.contains( QStringLiteral( "New bone bindings: 59" ) )
		|| !combinedMessages.contains( QStringLiteral( "CustomizationRemapData: create (20268 bytes)" ) )
		|| !combinedMessages.contains( QStringLiteral( "Surface snap:" ) )
		|| !combinedMessages.contains( QStringLiteral( "Geometry match: Close" ) )
		|| combined.getBlockCount() != 76 || countNodes( combined ) != 70
		|| !skinCounts( combined, combinedShape, 69 )
		|| remapData( combined, combinedShape ).size() != 1689 * 12
		|| !driver.messages.last().contains( QStringLiteral( "as one Undo step" ) ) ) return 23;
	if ( combinedUndo.count() != 1 ) return 24;
	combinedUndo.undo();
	combinedShape = findShape( combined );
	if ( combined.getBlockCount() != 16 || countNodes( combined ) != 11
		|| !skinCounts( combined, combinedShape, 10 ) || !remapData( combined, combinedShape ).isEmpty() ) return 25;
	combinedUndo.redo();
	combinedShape = findShape( combined );
	if ( combined.getBlockCount() != 76 || countNodes( combined ) != 70
		|| !skinCounts( combined, combinedShape, 69 )
		|| remapData( combined, combinedShape ).size() != 1689 * 12 ) return 26;
	if ( !combined.saveToFile( outputPath ) ) return 27;

	NifModel combinedReloaded;
	if ( !combinedReloaded.loadFromFile( outputPath ) ) return 28;
	QModelIndex combinedReloadedShape = findShape( combinedReloaded );
	double combinedMeanL1 = 0.0, combinedMaxL1 = 0.0;
	if ( combinedReloaded.getBlockCount() != 76 || countNodes( combinedReloaded ) != 70
		|| !skinCounts( combinedReloaded, combinedReloadedShape, 69 )
		|| remapData( combinedReloaded, combinedReloadedShape ).size() != 1689 * 12
		|| !compareWeights( combinedReloaded, combinedReloadedShape, donor, findShape( donor ),
			&combinedMeanL1, &combinedMaxL1 )
		|| combinedMeanL1 > 0.001 || combinedMaxL1 > 0.002 ) return 29;
	if ( !runSpell( combinedReloaded, QStringLiteral( "Rigging/Validate FO4 Skin" ), driver, &error )
		|| driver.messages.last().contains( QStringLiteral( "Found " ) ) ) return 30;

	// Standard-only transfer must generate RemapData after writing the packed
	// weights. Generating it first is observably stale even for a self-transfer
	// because normalization can change a float16 value by one unit.
	driver.setDonorPath( targetPath );
	NifModel standardOnly;
	QUndoStack standardOnlyUndo;
	standardOnly.undoStack = &standardOnlyUndo;
	if ( !standardOnly.loadFromFile( targetPath ) ) return 68;
	QByteArray standardOnlyBefore = serializeModel( standardOnly );
	if ( !runSpell( standardOnly, QStringLiteral( "Rigging/Transfer Bones and Weights..." ), driver, &error ) ) return 69;
	QModelIndex standardOnlyShape = findShape( standardOnly );
	if ( standardOnlyUndo.count() != 1 || !skinCounts( standardOnly, standardOnlyShape, 10 )
		|| remapData( standardOnly, standardOnlyShape ).size() != 1689 * 12
		|| !normalizedWeights( standardOnly, standardOnlyShape )
		|| !runSpell( standardOnly, QStringLiteral( "Rigging/Validate FO4 Skin" ), driver, &error )
		|| driver.messages.last().contains( QStringLiteral( "Found " ) ) ) return 70;
	standardOnlyUndo.undo();
	if ( serializeModel( standardOnly ) != standardOnlyBefore ) return 71;
	standardOnlyUndo.redo();
	if ( remapData( standardOnly, findShape( standardOnly ) ).size() != 1689 * 12 ) return 72;

	// Exercise the real multi-shape donor picker while transferring onto a
	// topologically different (but still valid) target mesh.
	driver.setDonorPath( multiDonorPath );
	driver.donorShapeChoice = 1;
	NifModel varied;
	QUndoStack variedUndo;
	varied.undoStack = &variedUndo;
	if ( !varied.loadFromFile( topologyTargetPath ) ) return 47;
	int variedFileDialogs = driver.fileDialogs;
	int variedInputDialogs = driver.inputDialogs;
	int variedMessages = driver.messages.size();
	if ( !runSpell( varied, QStringLiteral( "Rigging/Transfer Bones and Weights..." ), driver, &error ) ) return 48;
	QModelIndex variedShape = findShape( varied );
	if ( driver.fileDialogs != variedFileDialogs + 1 || driver.inputDialogs != variedInputDialogs + 1
		|| !driver.messages.mid( variedMessages ).join( QStringLiteral( "\n" ) ).contains( QStringLiteral( "Harness alternate donor" ) )
		|| !driver.messages.mid( variedMessages ).join( QStringLiteral( "\n" ) ).contains(
			QStringLiteral( "Target geometry: 1690 vertices, 3229 triangles" ) )
		|| !driver.messages.mid( variedMessages ).join( QStringLiteral( "\n" ) ).contains(
			QStringLiteral( "CustomizationRemapData: create (20280 bytes)" ) )
		|| !driver.messages.mid( variedMessages ).join( QStringLiteral( "\n" ) ).contains(
			QStringLiteral( "Surface snap:" ) )
		|| variedUndo.count() != 1 || varied.getBlockCount() != 76 || countNodes( varied ) != 70
		|| !skinCounts( varied, variedShape, 69 )
		|| varied.rowCount( varied.getIndex( variedShape, "Vertex Data" ) ) != originalVertices + 1
		|| remapData( varied, variedShape ).size() != ( originalVertices + 1 ) * 12
		|| !normalizedWeights( varied, variedShape )
		|| varied.rowCount( varied.getIndex( variedShape, "Triangles" ) ) != originalTriangles - 1 ) return 49;
	if ( !varied.saveToFile( topologyResultPath ) ) return 50;
	NifModel variedReloaded;
	if ( !variedReloaded.loadFromFile( topologyResultPath ) ) return 51;
	QModelIndex variedReloadedShape = findShape( variedReloaded );
	if ( variedReloaded.rowCount( variedReloaded.getIndex( variedReloadedShape, "Vertex Data" ) ) != originalVertices + 1
		|| variedReloaded.rowCount( variedReloaded.getIndex( variedReloadedShape, "Triangles" ) ) != originalTriangles - 1
		|| remapData( variedReloaded, variedReloadedShape ).size() != ( originalVertices + 1 ) * 12
		|| !normalizedWeights( variedReloaded, variedReloadedShape ) ) return 52;
	if ( !runSpell( variedReloaded, QStringLiteral( "Rigging/Validate FO4 Skin" ), driver, &error )
		|| driver.messages.last().contains( QStringLiteral( "Found " ) ) ) return 53;
	driver.setDonorPath( donorPath );
	driver.donorShapeChoice = 0;

	SpellPtr combinedSpell = SpellBook::lookup( QStringLiteral( "Rigging/Transfer Bones and Weights..." ) );
	NifModel cancelled;
	QUndoStack cancelledUndo;
	cancelled.undoStack = &cancelledUndo;
	if ( !combinedSpell || !cancelled.loadFromFile( targetPath ) ) return 31;
	QByteArray cancelledBefore = serializeModel( cancelled );
	driver.cancelNextFileDialog = true;
	combinedSpell->cast( &cancelled, findShape( cancelled ) );
	if ( cancelledBefore.isEmpty() || serializeModel( cancelled ) != cancelledBefore || cancelledUndo.count() != 0 ) return 32;

	driver.rejectNextQuestion = true;
	combinedSpell->cast( &cancelled, findShape( cancelled ) );
	if ( serializeModel( cancelled ) != cancelledBefore || cancelledUndo.count() != 0 ) return 33;

	SpellPtr previewSpell = SpellBook::lookup( QStringLiteral( "Rigging/Preview Transfer from Donor..." ) );
	int previewProgress = driver.progressDialogs;
	driver.cancelNextProgress = true;
	if ( !previewSpell ) return 54;
	previewSpell->cast( &cancelled, findShape( cancelled ) );
	if ( driver.progressDialogs != previewProgress + 1 || serializeModel( cancelled ) != cancelledBefore
		|| cancelledUndo.count() != 0
		|| !driver.messages.last().contains( QStringLiteral( "preview cancelled" ), Qt::CaseInsensitive ) ) return 55;

	int combinedProgress = driver.progressDialogs;
	driver.cancelNextProgress = true;
	combinedSpell->cast( &cancelled, findShape( cancelled ) );
	if ( driver.progressDialogs != combinedProgress + 1 || serializeModel( cancelled ) != cancelledBefore
		|| cancelledUndo.count() != 0
		|| !driver.messages.last().contains( QStringLiteral( "geometry analysis" ), Qt::CaseInsensitive )
		|| !driver.messages.last().contains( QStringLiteral( "Nothing was changed" ) ) ) return 56;

	// A deliberately displaced target must be classified as a poor geometry
	// match before any mutation. Rejecting the default-Cancel confirmation must
	// preserve the already-displaced input byte-for-byte with no Undo entry.
	NifModel distant;
	if ( !distant.loadFromFile( targetPath ) ) return 57;
	QModelIndex distantShape = findShape( distant );
	QModelIndex distantVertices = distant.getIndex( distantShape, "Vertex Data" );
	if ( !distantVertices.isValid() ) return 58;
	for ( int vertex = 0; vertex < distant.rowCount( distantVertices ); vertex++ ) {
		QModelIndex vertexIndex = distant.getIndex( distantVertices, vertex );
		Vector3 position = distant.get<Vector3>( vertexIndex, "Vertex" );
		position += Vector3( 10000.0f, 0.0f, 0.0f );
		if ( !distant.set<HalfVector3>( distant.getIndex( vertexIndex, "Vertex" ), HalfVector3( position ) ) )
			return 58;
	}
	QUndoStack distantUndo;
	distant.undoStack = &distantUndo;
	QByteArray distantBefore = serializeModel( distant );
	int distantMessages = driver.messages.size();
	driver.rejectNextQuestion = true;
	combinedSpell->cast( &distant, distantShape );
	QString distantSummary = driver.messages.mid( distantMessages ).join( QStringLiteral( "\n" ) );
	if ( distantBefore.isEmpty() || serializeModel( distant ) != distantBefore || distantUndo.count() != 0
		|| !distantSummary.contains( QStringLiteral( "Geometry match: Poor" ) ) ) return 59;

	// An out-of-range donor triangle index used to reach unchecked incident-list
	// indexing in the transfer core. It must now be rejected during shape read,
	// before progress, confirmation, mutation, or Undo creation.
	driver.setDonorPath( malformedDonorPath );
	NifModel badTopology;
	QUndoStack badTopologyUndo;
	badTopology.undoStack = &badTopologyUndo;
	if ( !badTopology.loadFromFile( targetPath ) ) return 65;
	QByteArray badTopologyBefore = serializeModel( badTopology );
	int badTopologyMessages = driver.messages.size();
	combinedSpell->cast( &badTopology, findShape( badTopology ) );
	QString badTopologyResult = driver.messages.mid( badTopologyMessages ).join( QStringLiteral( "\n" ) );
	if ( badTopologyBefore.isEmpty() || serializeModel( badTopology ) != badTopologyBefore
		|| badTopologyUndo.count() != 0
		|| !badTopologyResult.contains( QStringLiteral( "not a readable FO4 BSTriShape" ) ) ) return 66;
	driver.setDonorPath( donorPath );

	// Force Import Donor Bone Nodes to fail after RemapData generation has
	// succeeded: duplicate a non-skin root name with an existing skin bone.
	// The combined operation must restore this intentionally malformed input
	// byte-for-byte and must not add an Undo command.
	NifModel rollback;
	if ( !rollback.loadFromFile( targetPath ) ) return 34;
	QModelIndex rollbackShape = findShape( rollback );
	QModelIndex rollbackSkin = rollback.getBlockIndex( rollback.getLink( rollbackShape, "Skin" ) );
	QModelIndex rollbackRoot = rollback.getBlockIndex( rollback.getLink( rollbackSkin, "Skeleton Root" ) );
	QString duplicateName = boneNames( rollback, rollbackShape ).value( 0 );
	if ( !rollbackRoot.isValid() || duplicateName.isEmpty() ) return 35;
	rollback.set<QString>( rollbackRoot, "Name", duplicateName );
	QUndoStack rollbackUndo;
	rollback.undoStack = &rollbackUndo;
	QByteArray rollbackBefore = serializeModel( rollback );
	int messagesBeforeRollback = driver.messages.size();
	combinedSpell->cast( &rollback, rollbackShape );
	if ( rollbackBefore.isEmpty() || serializeModel( rollback ) != rollbackBefore || rollbackUndo.count() != 0
		|| driver.messages.size() <= messagesBeforeRollback
		|| !driver.messages.last().contains( QStringLiteral( "Import Donor Bone Nodes" ) )
		|| !driver.messages.last().contains( QStringLiteral( "restored to its pre-transfer state" ) ) ) return 36;

	// Smoke-test the real workspace selector and manager dock. This exercises the
	// same construction path used by the application, not a standalone panel.
	QSettings().setValue( QStringLiteral( "UI/Workspace" ), 4 );
	NifSkope * window = NifSkope::createWindow();
	QCoreApplication::processEvents();
	QDockWidget * riggingDock = window->findChild<QDockWidget *>( QStringLiteral( "RiggingManagerDock" ) );
	QAction * objectModeAction = window->findChild<QAction *>( QStringLiteral( "ViewportObjectModeAction" ) );
	QAction * editModeAction = window->findChild<QAction *>( QStringLiteral( "ViewportEditModeAction" ) );
	QAction * vertexPaintAction = window->findChild<QAction *>( QStringLiteral( "ViewportVertexPaintAction" ) );
	QAction * weightPaintAction = window->findChild<QAction *>( QStringLiteral( "ViewportWeightPaintAction" ) );
	QAction * segmentPaintAction = window->findChild<QAction *>( QStringLiteral( "ViewportSegmentPaintAction" ) );
	QMenu * modeMenu = objectModeAction ? qobject_cast<QMenu *>( objectModeAction->parent() ) : nullptr;
	QToolButton * modeButton = modeMenu ? qobject_cast<QToolButton *>( modeMenu->parentWidget() ) : nullptr;
	QToolButton * workspaceButton = nullptr;
	for ( QToolButton * button : window->findChildren<QToolButton *>() )
		if ( button->text() == QStringLiteral( "Workspaces" ) ) workspaceButton = button;
	QAction * riggingWorkspace = nullptr;
	if ( workspaceButton && workspaceButton->menu() )
		for ( QAction * action : workspaceButton->menu()->actions() )
			if ( action->text() == QStringLiteral( "Rigging" ) ) riggingWorkspace = action;
	QAbstractButton * transferButton = riggingDock
		? riggingDock->findChild<QAbstractButton *>( QStringLiteral( "RiggingTransferButton" ) ) : nullptr;
	QTreeWidget * riggingBones = riggingDock
		? riggingDock->findChild<QTreeWidget *>( QStringLiteral( "RiggingBoneTree" ) ) : nullptr;
	QStringList expectedModes = { QStringLiteral( "Object Mode" ), QStringLiteral( "Edit Mode" ),
		QStringLiteral( "Vertex Paint" ), QStringLiteral( "Weight Paint" ), QStringLiteral( "Segment Paint" ) };
	QStringList actualModes;
	if ( modeMenu )
		for ( QAction * action : modeMenu->actions() ) actualModes.append( action->text() );
	if ( !riggingDock || !transferButton || transferButton->isEnabled() || !riggingBones
		|| !riggingWorkspace || !riggingWorkspace->isChecked() ) return 37;
	if ( !modeButton || !objectModeAction || !editModeAction || !vertexPaintAction
		|| !weightPaintAction || !segmentPaintAction || actualModes != expectedModes
		|| !objectModeAction->isChecked() || !weightPaintAction->isEnabled()
		|| vertexPaintAction->isEnabled() || segmentPaintAction->isEnabled() ) return 83;
	window->close();
	QCoreApplication::processEvents();
	if ( readFileBytes( targetPath ) != targetFixtureBytes
		|| readFileBytes( donorPath ) != donorFixtureBytes ) return 67;
	window = NifSkope::createWindow( targetPath );
	QCoreApplication::processEvents();
	objectModeAction = window->findChild<QAction *>( QStringLiteral( "ViewportObjectModeAction" ) );
	editModeAction = window->findChild<QAction *>( QStringLiteral( "ViewportEditModeAction" ) );
	vertexPaintAction = window->findChild<QAction *>( QStringLiteral( "ViewportVertexPaintAction" ) );
	weightPaintAction = window->findChild<QAction *>( QStringLiteral( "ViewportWeightPaintAction" ) );
	segmentPaintAction = window->findChild<QAction *>( QStringLiteral( "ViewportSegmentPaintAction" ) );
	modeMenu = objectModeAction ? qobject_cast<QMenu *>( objectModeAction->parent() ) : nullptr;
	modeButton = modeMenu ? qobject_cast<QToolButton *>( modeMenu->parentWidget() ) : nullptr;
	riggingDock = window->findChild<QDockWidget *>( QStringLiteral( "RiggingManagerDock" ) );
	transferButton = riggingDock
		? riggingDock->findChild<QAbstractButton *>( QStringLiteral( "RiggingTransferButton" ) ) : nullptr;
	riggingBones = riggingDock
		? riggingDock->findChild<QTreeWidget *>( QStringLiteral( "RiggingBoneTree" ) ) : nullptr;
	QAbstractButton * overlayLoad = riggingDock
		? riggingDock->findChild<QAbstractButton *>( QStringLiteral( "RiggingLoadDonorOverlayButton" ) ) : nullptr;
	QAbstractButton * overlayClear = riggingDock
		? riggingDock->findChild<QAbstractButton *>( QStringLiteral( "RiggingClearDonorOverlayButton" ) ) : nullptr;
	QLabel * overlayStatus = riggingDock
		? riggingDock->findChild<QLabel *>( QStringLiteral( "RiggingDonorOverlayStatus" ) ) : nullptr;
	QCheckBox * weightHeatmap = riggingDock
		? riggingDock->findChild<QCheckBox *>( QStringLiteral( "RiggingWeightHeatmapToggle" ) ) : nullptr;
	QLabel * weightHeatmapStatus = riggingDock
		? riggingDock->findChild<QLabel *>( QStringLiteral( "RiggingWeightHeatmapStatus" ) ) : nullptr;
	QAbstractButton * weightPaintButton = riggingDock
		? riggingDock->findChild<QAbstractButton *>( QStringLiteral( "RiggingWeightPaintButton" ) ) : nullptr;
	QSlider * weightPaintRadius = riggingDock
		? riggingDock->findChild<QSlider *>( QStringLiteral( "RiggingWeightPaintRadius" ) ) : nullptr;
	QLabel * weightPaintStatus = riggingDock
		? riggingDock->findChild<QLabel *>( QStringLiteral( "RiggingWeightPaintStatus" ) ) : nullptr;
	NifModel * windowNif = nullptr;
	for ( NifModel * candidate : window->findChildren<NifModel *>() )
		if ( candidate->getBlockCount() > 0 ) windowNif = candidate;
	GLView * windowView = window->getGLView();
	if ( !riggingDock || !transferButton || !riggingBones || !overlayLoad || !overlayClear
		|| !overlayStatus || !weightHeatmap || !weightHeatmapStatus || !weightPaintButton
		|| !weightPaintRadius || !weightPaintStatus || !windowNif || !windowView || !modeButton
		|| !objectModeAction || !editModeAction || !vertexPaintAction || !weightPaintAction
		|| !segmentPaintAction || vertexPaintAction->isEnabled() || segmentPaintAction->isEnabled() ) return 38;
	QModelIndex windowShape = findShape( *windowNif );
	window->select( windowShape );
	QCoreApplication::processEvents();
	if ( !windowShape.isValid() || !transferButton->isEnabled() || riggingBones->topLevelItemCount() != 10 ) return 39;

	// Load the donor through the real Rigging Manager and prove that the cyan
	// preview is viewport-only: no target bytes or Undo state may change. Hiding
	// the workspace removes it from GL, showing restores the cached soup.
	QByteArray overlayBefore = serializeModel( *windowNif );
	int overlayFiles = driver.fileDialogs;
	overlayLoad->click();
	QCoreApplication::processEvents();
	int overlayTriangles = windowView->riggingDonorPreviewTriangleCount();
	if ( driver.fileDialogs != overlayFiles + 1 || overlayTriangles <= 0
		|| serializeModel( *windowNif ) != overlayBefore
		|| ( windowNif->undoStack && windowNif->undoStack->count() != 0 )
		|| !overlayClear->isEnabled() || !overlayStatus->text().contains( QStringLiteral( "cyan preview only" ) ) ) return 73;
	riggingBones->setCurrentItem( riggingBones->topLevelItem( 0 ) );
	weightHeatmap->setChecked( true );
	QCoreApplication::processEvents();
	int heatmapTriangles = windowView->riggingWeightPreviewTriangleCount();
	if ( heatmapTriangles <= 0 || serializeModel( *windowNif ) != overlayBefore
		|| ( windowNif->undoStack && windowNif->undoStack->count() != 0 )
		|| !weightHeatmapStatus->text().contains( QStringLiteral( "weighted vertices" ) ) ) return 76;

	riggingDock->hide();
	QCoreApplication::processEvents();
	if ( windowView->riggingDonorPreviewTriangleCount() != 0
		|| windowView->riggingWeightPreviewTriangleCount() != 0 ) return 74;
	riggingDock->show();
	QCoreApplication::processEvents();
	if ( windowView->riggingDonorPreviewTriangleCount() != overlayTriangles
		|| windowView->riggingWeightPreviewTriangleCount() != heatmapTriangles ) return 75;

	// Drive the same stroke signals emitted by real mouse painting. This avoids
	// fragile headless screen coordinates while exercising the manager's actual
	// normalization, model write, heatmap refresh, and one-snapshot Undo path.
	int paintVertex = -1;
	QModelIndex windowVD = windowNif->getIndex( windowShape, "Vertex Data" );
	for ( int vertex = 0; vertex < windowNif->rowCount( windowVD ); vertex++ )
		if ( boneWeightAt( *windowNif, windowShape, vertex, 0 ) < 0.75 ) {
			paintVertex = vertex;
			break;
		}
	if ( paintVertex < 0 || !weightPaintButton->isEnabled() ) return 77;
	weightPaintRadius->setValue( 48 );
	weightPaintAction->trigger();
	QCoreApplication::processEvents();
	if ( !weightPaintButton->isChecked() || !windowView->riggingWeightPaintModeActive()
		|| !weightPaintAction->isChecked() || modeButton->text() != QStringLiteral( "Weight Paint" )
		|| std::fabs( windowView->riggingWeightPaintBrushRadius() - 48.0f ) > 0.01f ) return 78;
	QByteArray paintBefore = serializeModel( *windowNif );
	double paintWeightBefore = boneWeightAt( *windowNif, windowShape, paintVertex, 0 );
	windowView->riggingWeightStrokeBegan();
	windowView->riggingWeightBrushSample( windowNif->getBlockNumber( windowShape ),
		QVector<int>{ paintVertex }, QVector<float>{ 1.0f }, 0, 1.0f, 0.5f );
	windowView->riggingWeightStrokeEnded( true );
	QCoreApplication::processEvents();
	QByteArray paintAfter = serializeModel( *windowNif );
	if ( paintAfter == paintBefore || boneWeightAt( *windowNif, windowShape, paintVertex, 0 ) <= paintWeightBefore
		|| !normalizedWeights( *windowNif, windowShape ) || !windowNif->undoStack
		|| windowNif->undoStack->count() != 1 || !weightPaintStatus->text().contains( QStringLiteral( "one Undo step" ) )
		|| windowView->riggingWeightPreviewTriangleCount() <= 0 ) return 79;
	double addedWeight = boneWeightAt( *windowNif, windowShape, paintVertex, 0 );
	windowView->riggingWeightStrokeBegan();
	windowView->riggingWeightBrushSample( windowNif->getBlockNumber( windowShape ),
		QVector<int>{ paintVertex }, QVector<float>{ 1.0f }, 1, 1.0f, 0.2f );
	windowView->riggingWeightStrokeEnded( true );
	QCoreApplication::processEvents();
	if ( boneWeightAt( *windowNif, windowShape, paintVertex, 0 ) >= addedWeight
		|| !normalizedWeights( *windowNif, windowShape ) || windowNif->undoStack->count() != 2 ) return 81;
	windowView->riggingWeightStrokeBegan();
	windowView->riggingWeightBrushSample( windowNif->getBlockNumber( windowShape ),
		QVector<int>{ paintVertex }, QVector<float>{ 1.0f }, 2, 0.35f, 1.0f );
	windowView->riggingWeightStrokeEnded( true );
	QCoreApplication::processEvents();
	if ( std::fabs( boneWeightAt( *windowNif, windowShape, paintVertex, 0 ) - 0.35 ) > 0.003
		|| !normalizedWeights( *windowNif, windowShape ) || windowNif->undoStack->count() != 3 ) return 82;
	objectModeAction->trigger();
	QCoreApplication::processEvents();
	if ( weightPaintButton->isChecked() || weightPaintAction->isChecked()
		|| !objectModeAction->isChecked() || modeButton->text() != QStringLiteral( "Object Mode" ) ) return 84;
	while ( windowNif->undoStack->canUndo() ) windowNif->undoStack->undo();
	QCoreApplication::processEvents();
	windowShape = findShape( *windowNif );
	if ( serializeModel( *windowNif ) != paintBefore || windowNif->undoStack->index() != 0
		|| windowView->riggingWeightPaintModeActive() ) return 80;

	// Exercise the actual primary-button path used by artists. It must dispatch
	// through NifSkope::castSpell, refresh the manager after the model reset, and
	// still produce exactly one Undo entry for the complete workflow.
	int workspaceFiles = driver.fileDialogs;
	int workspaceProgress = driver.progressDialogs;
	transferButton->click();
	QCoreApplication::processEvents();
	windowShape = findShape( *windowNif );
	if ( driver.fileDialogs != workspaceFiles + 1 || driver.progressDialogs != workspaceProgress + 1
		|| windowNif->getBlockCount() != 76 || countNodes( *windowNif ) != 70
		|| !skinCounts( *windowNif, windowShape, 69 )
		|| remapData( *windowNif, windowShape ).size() != 1689 * 12
		|| !windowNif->undoStack || windowNif->undoStack->count() != 1
		|| riggingBones->topLevelItemCount() != 69
		|| windowView->riggingDonorPreviewTriangleCount() != 0 || overlayClear->isEnabled()
		|| windowView->riggingWeightPreviewTriangleCount() != 0 || weightHeatmap->isChecked()
		|| !driver.messages.last().contains( QStringLiteral( "as one Undo step" ) ) ) return 60;
	windowNif->undoStack->undo();
	QCoreApplication::processEvents();
	windowShape = findShape( *windowNif );
	if ( windowNif->getBlockCount() != 16 || countNodes( *windowNif ) != 11
		|| !skinCounts( *windowNif, windowShape, 10 )
		|| !remapData( *windowNif, windowShape ).isEmpty()
		|| windowNif->undoStack->index() != 0 ) return 61;
	window->close();
	QCoreApplication::processEvents();

	qInfo().noquote() << QStringLiteral( "PASS blocks=%1 nodes=%2 bones=69 remap=%3 stepsUndo=%4 combinedUndo=%5 "
		"multishape=clean topology=%6 vertices=%7 standardOnly=clean donorOverlay=clean weightHeatmap=clean weightPaint=clean paintModes=clean cancel=clean reject=clean progressCancel=clean summary=clean distanceGuard=clean topologyGuard=clean rollback=clean workspace=Rigging workspaceFlow=clean fixtures=immutable "
		"meanL1=%8 maxL1=%9 combinedMeanL1=%10 combinedMaxL1=%11" )
		.arg( combinedReloaded.getBlockCount() ).arg( countNodes( combinedReloaded ) )
		.arg( remapData( combinedReloaded, combinedReloadedShape ).size() )
		.arg( undo.count() ).arg( combinedUndo.count() )
		.arg( originalTriangles - 1 )
		.arg( originalVertices + 1 )
		.arg( meanL1, 0, 'g', 6 ).arg( maxL1, 0, 'g', 6 )
		.arg( combinedMeanL1, 0, 'g', 6 ).arg( combinedMaxL1, 0, 'g', 6 );
	return 0;
}

// main.cpp also owns IPCsocket's generated-metaobject dependencies. Include it
// under a renamed entry point so the harness can link without the production main.o.
#ifndef NIFSKOPE_VERSION
#define NIFSKOPE_VERSION "2.0.dev11"
#endif
#ifndef NIFSKOPE_REVISION
#define NIFSKOPE_REVISION "integration-test"
#endif
#define main nifskope_original_main
#include "main.cpp"
#undef main
