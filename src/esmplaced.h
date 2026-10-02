#ifndef ESMPLACED_H
#define ESMPLACED_H

/* lane PLACED1: placed content the cell view did not read.
 *
 * A DECAL is an ordinary placed reference whose base is a texture set carrying
 * decal data; the reference may add a box primitive and a size scale. It is
 * read here, as free functions over the plugin EsmWorld already holds, so the
 * shared reader stays untouched. (Placed actors: src/cellactor.cpp.) */

#include <QString>
#include <QVector>
#include <QtGlobal>

class EsmWorld;

//! One texture set (TXST) as a decal base.
struct EsmDecalBase
{
	bool exists = false;        //!< the form is a texture set
	bool hasDecalData = false;  //!< it carries decal data
	QString edid;
	QString diffuse, normal, spec;   //!< its own texture slots 0, 1, 7 (relative to textures/)
	QString material;                //!< a material file path; when set the textures come from it
	float minWidth = 0.0f, maxWidth = 0.0f, minHeight = 0.0f, maxHeight = 0.0f;
	float depth = 0.0f, shininess = 0.0f, parallaxScale = 0.0f;
	quint8 parallaxPasses = 0;
	quint8 flags = 0;           //!< 0x01 parallax shadows, 0x02 blend, 0x04 test, 0x08 whole texture, 0x10 multiply
	quint8 color[4] = { 255, 255, 255, 255 };
	bool wholeTexture() const { return ( flags & 0x08 ) != 0; }
	bool sizeIsFixed() const { return minWidth == maxWidth && minHeight == maxHeight; }
};

//! What a placed reference adds to a decal base.
struct EsmRefrDecal
{
	bool hasPrimitive = false;
	float half[3] = { 0.0f, 0.0f, 0.0f };   //!< the primitive's bounds, HALF extents
	quint32 primType = 0;                   //!< 1 = box
	bool hasScale = false;
	float widthScale = 1.0f, heightScale = 1.0f;
};

bool esmDecalBase( const EsmWorld & world, quint32 txstForm, EsmDecalBase & out );
bool esmRefrDecal( const EsmWorld & world, quint32 refrForm, EsmRefrDecal & out );

#endif // ESMPLACED_H
