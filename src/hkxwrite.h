/* WRITER for Fallout 4 Havok animation files: an HkxAnimClip (src/hkxanim.h)
   out to a .hkx the engine's own loader accepts, as
   `hkaInterleavedUncompressedAnimation` + `hkaAnimationBinding` (+ the
   extracted motion and the resource container that shipped clips carry).

   Lane HKX5, 2026-09-10.  The byte contract is docs/HKX_WRITE_FORMAT.md; the
   reader's contract, which this file is the inverse of, is
   docs/HKX_ANIMATION_FORMAT.md.

   WHY INTERLEAVED AND NOT SPLINE.  Lane HKXCLASS measured the 1.10.155 exe:
   `hkaInterleavedUncompressedAnimation` is a registered, fully implemented
   class (reflection 0x02e988e0, vtable 0x02e98938, static-linked-classes slot
   136, HKXPACK signature 0xa5eff3f2), and the loader dispatches on the class
   NAME -- never on `hkaAnimation::type` and never on a signature -- so a clip
   of this class loads and samples through the same virtual call a spline clip
   does.  Writing spline-compressed instead would need our own NURBS
   compressor; writing interleaved needs a flat array of hkQsTransform.  The
   cost is size: 48 bytes per bone per frame.

   TWO ROUTES BEHIND ONE CALL, as the reader has:
     route A  our XML -> `hkxpack-cli.jar pack` -> the .hkx   (needs Java)
     route B  the Havok 2014 binary packfile, emitted directly (needs nothing)
   Route B mirrors, byte for byte, the layout HKXPACK produces for the same
   objects: the same six classes, the same object order, the same 16-byte
   alignment, the same three fixup tables.  Gate (e) packs and unpacks the
   route-B file with HKXPACK itself, and the round-trip gates decode both.

   Qt-Core-only (QProcess for route A), like src/hkxanim.h. */

#ifndef HKXWRITE_H
#define HKXWRITE_H

#include "hkxanim.h"

#include <QByteArray>
#include <QString>

//! How the clip is written.  Every default is what a shipped FO4 clip carries.
struct HkxWriteOptions
{
	enum Route
	{
		RouteDirect = 0,   //!< emit the packfile ourselves (the default: no Java)
		RouteXmlPack = 1   //!< write HKXPACK XML and run `hkxpack-cli.jar pack`
	};
	Route route = RouteDirect;

	//! hkaAnimationBinding::originalSkeletonName.  Empty = the clip's own.
	QString skeletonName;

	//! Write the hkaDefaultAnimatedReferenceFrame when the clip has root
	//! motion.  false leaves `extractedMotion` null, as 715 shipped clips do.
	bool writeRootMotion = true;
	//! Write the clip's annotations.  One (possibly empty) hkaAnnotationTrack
	//! per transform track is written either way -- that is what every shipped
	//! clip has and what `hkaAnimation::annotationTracks` is sized against.
	bool writeAnnotations = true;
	//! The empty "Resource Data" -> hkMemoryResourceContainer named variant.
	//! Shipped clips carry it; the Mixamo fixture does not (lane FIXTURE).
	bool writeResourceContainer = true;

	//! Route A only.  Empty = the machine's HKXPACK
	//! (E:/Tools/Fallout 4/HKXPACK/hkxpack-cli.jar).
	QString hkxpackJar;
	QString javaExe = QStringLiteral( "java" );
	//! Route A only: keep the intermediate XML at this path instead of a
	//! temporary file (it is the thing to look at when a pack fails).
	QString xmlKeepPath;
	int packTimeoutMs = 120000;
};

//! What the writer did, for the report and the gates.
struct HkxWriteReport
{
	QString error;          //!< empty on success
	QString routeUsed;      //!< "direct packfile" or "HKXPACK XML + pack"
	QString command;        //!< route A: the exact command line that was run
	int objects = 0;        //!< objects in __data__
	int tracks = 0, frames = 0;
	qint64 transformBytes = 0;   //!< 48 * tracks * frames
	qint64 fileBytes = 0;
	QString summary() const;
};

//! The HKXPACK XML for a clip (route A's input, and readable by
//! hkxAnimLoadXml).  Empty + a sentence in `error` on refusal.
QByteArray hkxWriteXml( const HkxAnimClip & clip, const HkxWriteOptions & opt, QString & error );

//! The binary packfile for a clip (route B).  Empty + a sentence on refusal.
QByteArray hkxWritePackfile( const HkxAnimClip & clip, const HkxWriteOptions & opt, QString & error );

//! One API: write `clip` to `path` by the chosen route.  Nothing is written
//! when it refuses, and `report.error` is a sentence naming the field.
bool hkxWrite( const HkxAnimClip & clip, const QString & path,
			   const HkxWriteOptions & opt, HkxWriteReport & report );

#endif // HKXWRITE_H
