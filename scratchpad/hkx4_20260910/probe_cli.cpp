#include "gltfexportnif.h"
#include "model/nifmodel.h"
#include <QTextStream>
#include <QString>
#include <QStringList>
static QTextStream & out() { static QTextStream s( stdout ); return s; }
static QTextStream & err() { static QTextStream s( stderr ); return s; }
static bool loadNif( NifModel &, const QString & ) { return true; }
/*! `gltf <file.nif> -o out.gltf [--clip C.hkx] [--bones skeleton.hkx]
 *  [--root-motion]` -- lane HKX4.
 *
 *  Writes out.gltf and out.bin: the NIF's node tree, its skinned shapes and,
 *  when --clip is given, that clip as a glTF animation at its own frame rate.
 *  --bones names the file whose hkaSkeleton gives the tracks their bone names
 *  (a clip usually carries none of its own; use the game's skeleton.hkx).
 *  Without --root-motion the clip plays in place and the travel is recorded
 *  in the animation's extras. docs/GLTF_INTERCHANGE.md is the contract.
 */
int cmdGltf( const QString & file, const QString & outFile, const QString & clipFile,
			 const QString & bonesFile, bool rootMotion )
{
	if ( outFile.isEmpty() ) {
		err() << "gltf: -o <out.gltf> is required" << Qt::endl;
		return 2;
	}
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	HkxAnimClip clip;
	QStringList boneNames;
	bool haveClip = false;
	if ( !clipFile.isEmpty() ) {
		const HkxAnimFile cf = hkxAnimLoad( clipFile );
		if ( !cf.ok() ) {
			err() << "gltf: " << cf.error << Qt::endl;
			return 1;
		}
		if ( cf.clips.isEmpty() ) {
			err() << "gltf: " << clipFile << " carries no animation" << Qt::endl;
			return 1;
		}
		clip = cf.clips.first();
		haveClip = true;
		if ( !bonesFile.isEmpty() ) {
			const HkxAnimFile bf = hkxAnimLoad( bonesFile );
			if ( !bf.ok() || bf.skeletons.isEmpty() ) {
				err() << "gltf: " << ( bf.ok() ? QStringLiteral( "%1 carries no hkaSkeleton" ).arg( bonesFile )
											   : bf.error ) << Qt::endl;
				return 1;
			}
			boneNames = bf.skeletons.first().boneNames;
		} else if ( !cf.skeletons.isEmpty() ) {
			boneNames = cf.skeletons.first().boneNames;
		} else {
			err() << "gltf: " << clipFile << " carries no skeleton; pass --bones skeleton.hkx" << Qt::endl;
			return 1;
		}
	}

	GltfExportReport report;
	QString error;
	if ( !gltfExportNifWrite( &nif, QModelIndex(), haveClip ? &clip : nullptr, boneNames,
							  rootMotion, outFile, report, error ) ) {
		err() << "gltf: " << error << Qt::endl;
		return 1;
	}
	out() << "wrote " << outFile << ": " << report.nodes << " nodes, " << report.shapes
		  << " shapes (" << report.skinnedShapes << " skinned), " << report.vertices
		  << " vertices, " << report.triangles << " triangles" << Qt::endl;
	if ( haveClip )
		out() << "  clip '" << clip.name << "': " << clip.numFrames << " frames at "
			  << clip.frameDuration << " s, " << report.tracksMatched << " of "
			  << ( report.tracksMatched + report.tracksUnmatched ) << " tracks matched a node"
			  << Qt::endl;
	for ( const QString & n : report.notes )
		out() << "  " << n << Qt::endl;
	return 0;
}

