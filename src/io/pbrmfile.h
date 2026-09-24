#ifndef PBRMFILE_H
#define PBRMFILE_H

#include <QString>
#include <QStringList>


/*! Reader for PBRM v4, v5 and v6 materials (the PBR Material Editor's format).
 *
 * v6 (lane PBRR1, 2026-09-24; PBRMaterialEditorQt/docs/PBRM-v6.md and the FO4CS
 * wave 88 reader, wt-spec1 src/Materials/PBRM.cpp:787-1010): the dielectric F0
 * is no longer authored. RMAOS alpha / the `specularWeight` constant is the
 * OpenPBR specular WEIGHT and the level comes from `primarySpecularColor`'s
 * `ior` (Pattern B: the feature parameters live on that slot's `values`, and
 * `ior`/`iorMax` are read whether or not the slot is enabled). The F0 law is
 * chosen BY ENVELOPE -- pbrmDielectricF0() -- so a v4/v5 file reads as before
 * and a v6 file is never read through the v5 law.
 *
 * Envelope: ASCII `PBRM`, uint32 version (4, 5 or 6), uint32 JSON payload
 * size, then that many bytes of compact UTF-8 JSON. The declared size must
 * consume the rest of the file exactly — truncation and trailing bytes are both
 * errors. Payload cap 64 MiB.
 *
 * Scope is the spec's own **"Minimal Standard runtime slice"**: shader
 * `Standard` and the four Primary UV sockets. Everything else in the document is
 * parseable authoring data that this reader deliberately ignores rather than
 * half-supports. Authoritative spec:
 * `PBRMaterialEditorQt/docs/PBRM-v5.md`.
 *
 * **Fail closed.** An unknown shader name, or a `requirements` entry this build
 * does not provide, leaves the document *valid* but unrenderable: `ok` is false
 * with `unsupported` set. Callers must fall back to the invalid-material
 * behaviour, never render it partially and never silently treat it as
 * `Standard`.
 */
struct PbrmMaterial
{
	//! One texture slot plus the constants that stand in when it is not sampled.
	struct Slot
	{
		bool enabled = false;
		QString path;			//!< exactly as authored, for diagnostics
		QString lookupPath;	//!< normalised per the spec's path contract
		bool pathValid = false;	//!< false => slot disabled + a diagnostic
	};

	// --- envelope / document ---
	int envelopeVersion = 0;
	QString shader;			//!< canonical shader family name
	bool ok = false;			//!< parsed AND supported by this build
	bool unsupported = false;	//!< valid document, unsupported shader/requirement
	QString error;			//!< hard parse error, empty when ok
	QStringList diagnostics;	//!< non-fatal (rejected paths, ...)

	// --- Minimal Standard slice: the four Primary UV sockets ---
	Slot baseColor, normal, rmaos, emissive;

	// Constants. Each `override*` true means "ignore the texture channel and use
	// the constant". Defaults are the spec's missing-value defaults.
	float baseColorRGB[3] = { 1.0f, 1.0f, 1.0f };
	bool overrideColor = true;
	float opacity = 1.0f;
	bool overrideOpacity = true;
	//! primaryBaseColor `diffuseRoughness` (OpenPBR base_diffuse_roughness), [0,1],
	//! default 0; > 0 shades the direct diffuse as EON (lane PBRR3). Any envelope.
	float diffuseRoughness = 0.0f;
	float normalStrength = 1.0f;
	bool overrideNormal = true;
	bool heightInBlue = false;
	bool curvatureInAlpha = false;
	float cavitySpecOcclusion = 1.0f;
	float roughness = 0.5f;
	bool overrideRoughness = true;
	float metallic = 0.0f;
	bool overrideMetallic = true;
	float ao = 1.0f;
	bool overrideAo = true;
	float f0 = 0.04f;
	bool overrideF0 = true;
	//! "Dielectric F0" or "Porosity" — F0 is only sampled from RMAOS alpha in
	//! the former case; under Porosity F0 always uses its constant.
	QString alphaCarries = QStringLiteral( "Dielectric F0" );
	float porosity = 0.5f;
	bool overridePorosity = false;	//!< the one flag never auto-forced
	//! primaryEmissive (lane PBRR4, docs s3.2 item 4): the constant colour as authored
	//! (raw sRGB), `luminance` in nits / 100 (a pre-2026-09-17 `intensity` is already
	//! that ratio), the constant A mask, and the editor's replace semantics: with a map,
	//! `overrideColor`/`overrideMask` default FALSE, so the map's RGB REPLACES the
	//! constant colour (materialpreviewwidget.cpp:542-545, 2519-2520).
	float emissiveRGB[3] = { 1.0f, 1.0f, 1.0f };
	float emissiveIntensity = 0.0f;			//!< luminance / 100: 100 nits = linear 1.0
	float emissiveMask = 1.0f;
	bool overrideEmissiveColor = true;
	bool overrideEmissiveMask = true;

	//! primaryTintMask (lane PBRR4, special layer 0, ED:734-735, 2310-2316, 5267-5268):
	//! four masks R/G/B/A, each the map channel unless overridden (override defaults
	//! TRUE = the constant mask, default 0); four colours as RAW sRGB (Q13); overlap
	//! 0 Normalize (default), 1 Add, 2 Priority RGBA. Off unless the slot is enabled.
	Slot tintMask;
	bool tintEnabled = false;
	float tintMaskConst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	bool tintUseTexture[4] = { false, false, false, false };	//!< valid path AND !overrideMask<c>
	float tintColor[4][3] = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } };
	int tintOverlap = 0;

	//! settings "Transparency/Composition" (lane PBRR4, docs s3.2 item 9; FO4CS
	//! PBRM.cpp:747-757, the editor ED:475-487): mode 0 Opaque, 1 Alpha Test, 2 Alpha
	//! Blend, 3 Premultiplied, 4 Additive, 5 Multiply, 6 Physical Transmission, 7 Water
	//! (6/7 blend like 2 until the merge). Default by shader family (Standard: Opaque).
	int composition = 0;
	float globalOpacity = 1.0f;			//!< `alpha`
	float alphaThreshold = 0.5f;		//!< `threshold`
	bool alphaSourceConstant = false;	//!< alphaSource "Constant": the source alpha is 1
	bool depthWrite = true;				//!< Transparency/Depth depthWrite, default composition <= 1
	bool twoSided = false;				//!< Shading/Core twoSided (read; not applied yet)

	// --- v6 specular (envelope 6 only; defaults are the spec's) ---
	//! true when the envelope is 6: `f0` above is then unused and the level is
	//! min(weight x iorF0, 1) x tint (pbrmDielectricF0).
	bool specularV6 = false;
	float specularWeight = 1.0f;			//!< RMAOS `specularWeight` constant, [0,1]
	bool overrideSpecularWeight = true;	//!< false + alphaCarries "Specular Weight" = RMAOS alpha
	float specularIor = 1.5f;				//!< primarySpecularColor `ior` (min(ior, iorMax) when overrideIor=false, no map)
	float specularIorMax = 4.25f;			//!< primarySpecularColor `iorMax`
	bool overrideSpecularIor = true;		//!< false + a map = the map's A x iorMax (not sampled here yet)
	bool overrideSpecularColor = true;	//!< false + a map = the map's RGB is the tint (not sampled here yet)
	float specularTint[3] = { 1.0f, 1.0f, 1.0f };	//!< the constant tint; white when the slot is disabled
	Slot specularColor;					//!< primarySpecularColor

	//! Derived, not serialised — the spec's bit assignments.
	enum Feature : quint32
	{
		BaseColorTexture = 1u << 0,
		NormalTexture    = 1u << 1,
		NormalHeightBlue = 1u << 2,
		RmaosTexture     = 1u << 3,
		RmaosRoughness   = 1u << 4,
		RmaosMetallic    = 1u << 5,
		RmaosAo          = 1u << 6,
		RmaosF0          = 1u << 7,
		EmissiveTexture  = 1u << 8,
		NormalCurvature  = 1u << 9,
		RmaosPorosity    = 1u << 10,
		//! Base-colour alpha is opacity. Derived from `overrideOpacity` being
		//! false — without it a shader cannot tell "this texture's alpha is
		//! coverage" from "this texture happens to have an alpha channel", and
		//! FO4 diffuse maps routinely carry meaningless or zero alpha.
		OpacityTexture   = 1u << 11,
		//! v6 names bit 7 RmaosSpecularWeight: RMAOS alpha is the WEIGHT, not
		//! an F0. The v5 shader reads bit 7 as "alpha is F0", so the renderer
		//! never hands it this bit for a v6 file (lane PBRR1).
		RmaosSpecularWeight = 1u << 7,
		//! PBRM-v6.md bit 25: enabled, valid primarySpecularColor path, overrideColor=false.
		SpecularColorTexture = 1u << 25,
		//! bit 30 in the FO4CS runtime: the map's A x iorMax is the IOR.
		SpecularIorTexture = 1u << 30,
	};
	quint32 features = 0;
};

//! Which F0 law to apply. Auto = by envelope, the only correct choice. V5 takes
//! the document's RMAOS scalar as the F0 whatever the envelope says; it exists
//! ONLY as the red control of gate R1 (c) (WW_PBRM_F0_LAW=v5).
enum class PbrmF0Law { Auto, V5 };

/*! The dielectric F0 LEVEL (before the tint), the FO4CS wave 88 upload law
 * `min( scalar x iorF0, cap )` with the same per-envelope parameters:
 *  - v6:    scalar = specularWeight, iorF0 = ((ior-1)/(ior+1))^2, cap 1.0
 *           (weight 1, ior 1.5 -> 0.040);
 *  - v4/v5: scalar = f0 (clamped to [0, 0.16] on read), iorF0 = 1 (the IOR-0
 *           upload), cap 0.16 -> the authored f0 (0.04 -> 0.040).
 * Law V5 on a v6 document takes its scalar (the weight) as the F0, the reading
 * a v5-only consumer makes: weight 1 -> 1.0.
 */
float pbrmDielectricF0( const PbrmMaterial & m, PbrmF0Law law = PbrmF0Law::Auto );

//! ((ior - 1) / (ior + 1))^2 with ior clamped at 0 -- FO4CS IorF0.
float pbrmIorF0( float ior );

//! Parse a .pbrm from raw bytes. Never throws; inspect `ok`/`error`.
PbrmMaterial pbrmParse( const QByteArray & bytes );

//! Parse a .pbrm from disk. Unreadable file => ok false with an error.
PbrmMaterial pbrmParseFile( const QString & path );

/*! Normalise a texture path per the spec's path contract: `/`->`\`, drop a
 * leading `.\`, collapse repeated separators, drop a leading `textures\` so
 * Data-relative and texture-root-relative authoring resolve alike, and lowercase
 * for case-insensitive comparison. Returns false for drive-qualified, UNC,
 * root-qualified and parent-traversal paths, which disable the slot.
 *
 * The authored string is never rewritten — this is a derived lookup value.
 */
bool pbrmNormalisePath( const QString & authored, QString & out );

#endif // PBRMFILE_H
