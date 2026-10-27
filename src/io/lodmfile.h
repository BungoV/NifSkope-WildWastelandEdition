#ifndef LODMFILE_H
#define LODMFILE_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>


/*! `.lodm` — the LOD material. Ours, for LOD, compact by design.
 *
 * A `.pbrm` describes an authored surface and a `.bgsm` a Creation Kit one;
 * neither can say what a LOD texture set carries (a coverage alpha, a sway
 * weight, a subsurface mask, an octahedral frame grid) and both carry pages a
 * LOD consumer never reads. So LOD gets its own material, sized to the job:
 * the FAMILY of the set, the three textures, and the few numbers a consumer
 * needs at load time. Nothing else. bungo, 2026-09-06: "compact by design,
 * where performance is the goal, aligned with the intention of LOD".
 *
 * Envelope, like PBRM's so a file is sniffable: ASCII `LODM`, uint32 version
 * (1), uint32 payload size, then that many bytes of compact UTF-8 JSON, which
 * must consume the rest of the file exactly.
 *
 * Payload — one flat object:
 *
 *   lodm      1 (2: card family with model sway; 3: an array of BOTH families)
 *   family    "legacy" | "pbr" ("mixed" on a lodm 3 array holding both)
 *   kind      "source" | "card" | "array" | "cardArray" | "terrainVT"
 *   textures  { diffuse|baseColor, normal, gsaos|rmaos,
 *               emissive }                                 game paths
 *   card      { oct, frame [w,h], half [w,h], center [x,y,z], depthSpan, mips }
 *   array     { class [w,h], layers [source per layer],
 *               emissiveScale [one per layer] }                 kind array
 *   array     { class [w,h], oct, frame [w,h], mips,
 *               emissiveScale [one per layer],
 *               layers [ {id, half, center, depthSpan, source} ] }  kind cardArray
 *   terrain   { worldspace, extent, cellUnits, content, border, stored, mips,
 *               aniso, rowOrder, compression, both corpus hashes, sheets[],
 *               cover{}, levels[] }                            kind terrainVT
 *   emissiveScale  what a consumer multiplies the emissive sheet by
 *   heightInBlue   true when a SOURCE normal's blue carries height
 *
 * `family` is VESTIGIAL for `kind: "terrainVT"`, and `kind` is the real
 * discriminator there. A tile pyramid is neither a legacy nor a PBR material;
 * it says `legacy` only because the parser hard-rejects a third family word
 * and a new one would split the corpus. The day a PBR terrain VT exists, that
 * is the collision to resolve, and this is where it was seen coming.
 *
 * The family decides the texture keys and what the third texture's channels
 * mean (docs/LODGEN_IMPOSTOR_SPEC.md): a `legacy` set is vanilla-sourced —
 * diffuse, normal, and GSAOS = gloss, specular, AO, subsurface mask; a `pbr`
 * set is base colour, normal, and RMAOS = roughness, metallic, AO,
 * subsurface mask. A FOURTH texture is the emissive sheet, `emissive` under
 * both families and `_g` on a legacy set / `_e` on a pbr one, BC1, RGB the
 * emissive colour and no alpha: LOD's light, which vanilla carries as the
 * diffuse's alpha on an opaque chunk shape. The emissive is OPTIONAL: a set
 * that names none emits nothing. An `array` or `cardArray` set whose
 * emissive is black on every layer names none and ships no `_g`/`_e`
 * (lane TIDY1, 2026-09-27: vanilla LOD has no glow source, so every vanilla
 * mesh set is one; a card set shot from a full model that emits keeps its
 * sheet); the terrain VT sheets already had it so.
 *
 * `emissiveScale` is the other half of that light, and the reason this
 * material carries a number at all. The sheet holds the emissive COLOUR
 * folded in (diffuse x its alpha x the source's emissive colour on a legacy
 * set); the MULTIPLE stays here, because it is one float per set and folding
 * it into eight-bit texels would clip everything above one. bungo,
 * 2026-09-06: "carry the multiplier in lodm". Absent means 1. It sits at the
 * top level of a `source` or `card` file and as a list parallel to the
 * layers, `array.emissiveScale`, on an `array` or `cardArray` one - two
 * layers of one array are two different materials and do not share it.
 *
 * LODM 3 (lane LODMNAME1, bungo 2026-10-27) is the ARRAY layout with no family
 * in the file name: one `<ws>.LodgenArrays.<WxH>.lodm` / `<ws>.LodgenCards.<WxH>.lodm`
 * per size, ONE shared `_n` sheet over every layer, and the colour/mask/emissive
 * sheets per family (`textures.legacy {diffuse, gsaos[, emissive]}`, `textures.pbr
 * {baseColor, rmaos[, emissive]}`), each holding only its own family's layers.
 * Every layer says `family`, `normalLayer` (= its index, the slice of `_n`) and
 * `familyLayer` (its slice of its family's sheets). docs/LODGEN_LODM_FORMAT.md 4.1.
 *
 * The generator WRITES one beside every set it makes and
 * READS one as a source override: a `<name>.lodm` beside a LOD material
 * (`<name>.bgsm`), or where a source shape names no material, the diffuse's
 * path under `materials\` with the `.lodm` extension.
 */
struct LodmMaterial
{
	bool ok = false;			//!< parsed, envelope and payload both sound
	QString error;				//!< why not, when !ok
	int version = 0;
	QString family;				//!< "legacy" or "pbr"
	QString kind;				//!< "source", "card", "array", "cardArray" or "terrainVT"
	bool pbr = false;			//!< family == "pbr"
	//! the three textures, as authored: colour (diffuse or base colour),
	//! normal, mask (GSAOS or RMAOS); empty when the file names none
	QString color, normal, mask;
	//! the emissive sheet (`textures.emissive`, `_g` legacy / `_e` pbr);
	//! empty when the file names none
	QString emissive;
	//! `emissiveScale`: what a consumer multiplies the emissive sheet by,
	//! the source's emissive multiple. 1 when the file names none. Array
	//! files carry one per layer under `array.emissiveScale` instead; read
	//! those from `root`.
	float emissiveScale = 1.0f;
	bool heightInBlue = false;
	QJsonObject root;			//!< the whole payload, for the card/array blocks
	int payloadVersion = 0;		//!< the payload's `lodm`: 1, 2 (card sway) or 3 (array layout)
	/*! lodm 3 only: the per-family sheets, [0] legacy and [1] pbr (`normal` above is
	 *  the shared one). `color`/`mask`/`emissive` above hold the FIRST family present. */
	struct FamilySheets { bool present = false; QString color, mask, emissive; int layers = 0; };
	FamilySheets fam[2];
	bool mixed = false;			//!< lodm 3 holding both families
	QVector<qint8> layerPbr;		//!< lodm 3: per layer, its family (1 = pbr)
	QVector<int> layerFamilyIndex;	//!< lodm 3: per layer, its slice in its family's sheets
};

/*! An array set's layer: its family and its slice in that family's colour/mask/emissive
 *  sheets (the `_n` slice is always `layer`). Before lodm 3 every layer is the file's own
 *  family at its own index. False when `layer` is out of range of a lodm 3 file. */
bool lodmArrayLayer( const LodmMaterial & m, int layer, bool * pbr, int * familyLayer );

//! Parse a .lodm from raw bytes. Never throws; inspect `ok`/`error`.
LodmMaterial lodmParse( const QByteArray & bytes );

//! The envelope around a compact serialisation of `root`.
QByteArray lodmSerialise( const QJsonObject & root );

//! Write `root` as a .lodm file. False when the file could not be written.
bool lodmWriteFile( const QString & path, const QJsonObject & root );

/*! Where a SOURCE .lodm lives for a shape: beside its material with the
 * .lodm extension (`materials\lod\foo.bgsm` -> `materials\lod\foo.lodm`),
 * or, when the shape names no material, at the diffuse's path under
 * `materials\` (`textures\lod\foo_d.dds` -> `materials\lod\foo_d.lodm`).
 * Backslashes, no leading `data\`; empty when there is nothing to key on. */
QString lodmSourceCandidate( const QString & material, const QString & diffuse );

//! The texture keys of a family. The emissive's key is `emissive` in both:
//! only its file suffix tells the families apart, the way vanilla's `_g` does.
inline const char * lodmColorKey( bool pbr ) { return pbr ? "baseColor" : "diffuse"; }
inline const char * lodmMaskKey( bool pbr ) { return pbr ? "rmaos" : "gsaos"; }
//! The file suffixes of a family: colour, mask and emissive; the normal is `_n` in both.
inline const char * lodmColorSuffix( bool pbr ) { return pbr ? "_bc" : "_d"; }
inline const char * lodmMaskSuffix( bool pbr ) { return pbr ? "_rmaos" : "_gsaos"; }
inline const char * lodmEmissiveSuffix( bool pbr ) { return pbr ? "_e" : "_g"; }

#endif // LODMFILE_H
