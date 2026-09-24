#ifndef PBRMRESOLVE_H
#define PBRMRESOLVE_H

#include "io/pbrmfile.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>


/*! THE ONE .pbrm CANDIDATE FUNCTION (lane PBRR1, docs/NIFSKOPE_PBR_RENDERER.md
 * s2.3 + the 2026-09-23 RULINGS). The viewport (BSShaderLightingProperty::
 * resolvePbrm), the `-no-gui pbrm-resolve` command and lodgen's material mask
 * all walk THIS order through THIS code, so a card and the viewport can never
 * disagree about which material a shape uses:
 *
 *   1. swap     an active material swap: the swap material's `_d.dds` diffuse
 *               names Materials\<dir>\<stem>.pbrm, then Materials\<parent>\<stem>.pbrm
 *               (FO4CS ResolveTextureSwapMaterialPaths, PBRM.cpp:1396);
 *   2. nifx     the `material` entry for this shape's node in <nifstem>.nifx
 *               beside the .nif (src/io/nifxfile.h);
 *   3. direct   the shader property names a .pbrm itself            } exclusive
 *   4. sibling  the property names a .bgsm/.bgem and X.pbrm is beside it } by name
 *   5. fo76     the property's BGSM is a Fallout 76 PBR BGSM (version 20-22,
 *               the PBR flag, a diffuse), converted to the Standard slice;
 *   6. stem     lodgen only: no material at all, a .pbrm named by the diffuse
 *               stem (lodmSourceCandidate), the convention a source .lodm uses;
 *   -  legacy   none of the above: today's BGSM/BGEM rendering.
 *
 * A candidate that is absent, malformed or unsupported DECLINES to the next
 * step (every decline is kept in `tried` for the census). The first candidate
 * that parses and is supported wins. A texture that then fails to load aborts
 * the PBR binding in the renderer (whole-binding abort, as the game does);
 * that is the renderer's decision, not this function's.
 *
 * Pure: file access goes through the injected reader, so the same function
 * runs against the VFS (viewport), the lodgen resource stack and a test
 * folder.
 */

enum class PbrmRoute : int
{
	Legacy = 0,
	Swap,
	Nifx,
	Direct,
	Sibling,
	Fo76,
	Stem,
};

//! "legacy", "swap", "nifx", "direct", "sibling", "fo76", "stem".
const char * pbrmRouteName( PbrmRoute r );

//! The shipped order: swap, nifx, direct, sibling, fo76, stem.
QList<PbrmRoute> pbrmDefaultOrder();

/*! Parse a comma-separated route permutation (the WW_PBRM_ORDER red-control
 * pin). It must name swap, nifx, direct, sibling and fo76 exactly once each;
 * stem may be named and is appended last when it is not. Anything else is
 * refused with `why` set and `out` left at the default order.
 */
bool pbrmParseOrder( const QString & spec, QList<PbrmRoute> & out, QString & why );

/*! Normalise a material path the way FO4CS NormalizeMaterialPath does:
 * `/` -> `\`, repeated separators collapsed, leading `.\` dropped; empty,
 * drive-qualified, root-qualified and `..` paths refused; `Materials\`
 * prefixed when missing. `wantExt` is the list of accepted extensions
 * (lower-case, with the dot). Returns false with `why` set on refusal.
 */
bool pbrmNormaliseMaterialPath( const QString & authored, const QStringList & wantExt,
	QString & out, QString * why = nullptr );

//! The swap rule's candidates for a diffuse (empty when it is not a `_d.dds`).
QStringList pbrmSwapCandidates( const QString & diffuse, QString * why = nullptr );

struct PbrmResolveInput
{
	QString material;		//!< the shader property's Name, as authored
	QString shapeName;		//!< the geometry node owning the property (census + .nifx key)

	//! The .nifx `material` entry for this shape, already looked up (raw
	//! authored string; empty = no entry). `nifxNote` explains an absent or
	//! refused entry for the census.
	QString nifxPbrm;
	QString nifxNote;

	//! The active swap's replacement material diffuse (empty = no active swap).
	QString swapDiffuse;
	QString swapNote;

	bool sibling = true;			//!< the Auto-replace setting
	bool fo76 = true;				//!< the FO76 BGSM step applies (a lighting shader in an FO4 NIF)
	QString stemDiffuse;			//!< lodgen only: the diffuse for the stem step
	bool cutAuthoringPath = false;	//!< lodgen only: cut an absolute MNAM at its last `materials\`

	QList<PbrmRoute> order;			//!< empty = pbrmDefaultOrder()
};

struct PbrmResolveResult
{
	PbrmRoute route = PbrmRoute::Legacy;
	QString path;			//!< the file that served (a .pbrm, or the FO76 .bgsm)
	PbrmMaterial material;	//!< ok when route != Legacy
	QString envelope;		//!< "v4"/"v5"/"v6", "bgsm-v22", or "none"
	QString refusal;		//!< why no earlier step served (never empty when Legacy)
	QStringList tried;		//!< "<route> <path>: <outcome>" for every candidate looked at
};

//! Returns true and fills `out` when `path` exists; false (quietly) when it does not.
using PbrmReader = std::function<bool( const QString & path, QByteArray & out )>;

PbrmResolveResult pbrmResolve( const PbrmResolveInput & in, const PbrmReader & read );

/*! Convert an FO76 PBR BGSM (already parsed, bytes) to the Standard slice:
 * base colour = texture 0, normal = texture 1, roughness = 1 - smoothness,
 * metallic 0, F0 0.04. `_l` / `_r` are not sampled yet (owed to R3). Returns
 * false with `why` when the BGSM is not an FO76 PBR material.
 */
bool pbrmFromFo76Bgsm( const QByteArray & bgsm, PbrmMaterial & out, QString & why );

#endif // PBRMRESOLVE_H
