#ifndef PBRMFILE_H
#define PBRMFILE_H

#include <QString>
#include <QStringList>


/*! Reader for PBRM v5 materials (the PBR Material Editor's format).
 *
 * Envelope: ASCII `PBRM`, uint32 version (5; 4 accepted), uint32 JSON payload
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
	float emissiveRGB[3] = { 1.0f, 1.0f, 1.0f };
	float emissiveIntensity = 0.0f;

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
	};
	quint32 features = 0;
};

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
