# HOOK-UP CHANGE NEEDED -- lane NATIVE0b, 2026-09-10

**NOT APPLIED.** `src/lodgen.cpp` is held by lane CLAMP2b and `src/nifcli.cpp` is off-limits to
this lane. Everything below is an exact patch against the files as read at 03:3x
(`src/lodgen.cpp` sha256 `69de2e977898c8cf`, 8,848 lines; `src/nifcli.cpp` `af2a729cb574d6d9`,
5,990 lines). Line numbers are given for orientation; **the anchors are the claim** -- assert
`count == 1` on each before replacing (both files are LF-only, 0 CR; keep it so).

After applying: `qmake NifSkope.pro` (three new sources were added to `NifSkope.pro` by this
lane, so the dependency lists must be regenerated -- `nifskope-ww-resume-pending` step 3),
then the build, then section 5 of `scratchpad/lane_native0_report.md` for the gates.

The stock path is untouched by construction: every emitter call is a no-op unless
`lodgenNativeBegin()` ran, and only `--native <dir>` runs it.

---

## A. `src/lodgen.cpp` -- four sites

### A1. include (line 7-9)

anchor (unique):
```
#include "lodgen.h"
```
insert AFTER the `#include "esmdata.h"` line that follows it (line 9):
```
#include "nativeemit.h"
```

### A2. the model loader the emitter calls (a new static function)

Place it immediately AFTER the anonymous namespace that ends at the line `} // namespace`
following `struct ObjBucket` (line ~2039, the one just before
`/* ================= rung 3: per-placement AO bake`), so `LodSrcShape` and
`lodgenLoadModel` are in scope:

```cpp
/* The FO4CS-native emitter's model loader: lodgenLoadModel through the same
 * resource stack, one cache for the run, LodSrcShape flattened into the plain
 * arrays nativeemit.h takes. `user` is the data root (a QString). */
bool lodgenNativeLoadModel( void * user, const QString & model, std::vector<NativeSrcShape> * out )
{
	static QHash<QString, QVector<LodSrcShape>> cache;
	const QString & dataRoot = *static_cast<const QString *>( user );
	const QVector<LodSrcShape> & shapes = lodgenLoadModel( dataRoot, model, cache );
	out->clear();
	for ( const LodSrcShape & s : shapes ) {
		if ( s.pos.isEmpty() || s.tris.isEmpty() )
			continue;
		NativeSrcShape n;
		const int nv = s.pos.size();
		n.geom.pos.reserve( size_t( nv ) * 3 );
		n.geom.nrm.reserve( size_t( nv ) * 3 );
		n.geom.tan.reserve( size_t( nv ) * 3 );
		n.geom.uv.reserve( size_t( nv ) * 2 );
		for ( int v = 0; v < nv; v++ ) {
			const Vector3 & p = s.pos[v];
			const Vector3 nn = v < s.nrm.size() ? s.nrm[v] : Vector3( 0.0f, 0.0f, 1.0f );
			const Vector3 tt = v < s.tan.size() ? s.tan[v] : Vector3( 1.0f, 0.0f, 0.0f );
			const Vector2 uv = v < s.uv.size() ? s.uv[v] : Vector2( 0.0f, 0.0f );
			n.geom.pos.push_back( p[0] ); n.geom.pos.push_back( p[1] ); n.geom.pos.push_back( p[2] );
			n.geom.nrm.push_back( nn[0] ); n.geom.nrm.push_back( nn[1] ); n.geom.nrm.push_back( nn[2] );
			n.geom.tan.push_back( tt[0] ); n.geom.tan.push_back( tt[1] ); n.geom.tan.push_back( tt[2] );
			n.geom.uv.push_back( uv[0] ); n.geom.uv.push_back( uv[1] );
		}
		n.geom.tris.reserve( size_t( s.tris.size() ) * 3 );
		for ( const Triangle & t : s.tris ) {
			n.geom.tris.push_back( quint32( t.v1() ) );
			n.geom.tris.push_back( quint32( t.v2() ) );
			n.geom.tris.push_back( quint32( t.v3() ) );
		}
		n.tex0 = s.tex0; n.tex1 = s.tex1; n.tex7 = s.tex7; n.matName = s.matName;
		n.smoothness = s.smoothness; n.specMult = s.specMult;
		n.emitColor[0] = s.emitColor.red(); n.emitColor[1] = s.emitColor.green(); n.emitColor[2] = s.emitColor.blue();
		n.emitMult = s.emitMult; n.ownEmit = s.ownEmit;
		n.hasAlpha = s.hasAlpha; n.alphaThreshold = s.alphaThreshold;
		out->push_back( std::move( n ) );
	}
	return !out->empty();
}
```
and declare it in `src/nativeemit.h`?  No -- `nifcli.cpp` needs the symbol; add this ONE line to
`src/lodgen.h` after `bool lodgenIsTreeModel( const QString & model );` (line 308):
```cpp
struct NativeSrcShape;
bool lodgenNativeLoadModel( void * user, const QString & model, std::vector<NativeSrcShape> * out );
```
(`lodgen.h` must then `#include <vector>`; it includes `<QString>`/`<QStringList>` only today.)

### A3. one placement per ring (line 3371)

anchor (unique):
```
		const bool swaying = opts.treeSway && isTree && localSpan > 1.0e-4f;
```
insert BEFORE it:
```cpp
		if ( lodgenNativeActive() ) {
			NativePlacement np;
			np.baseForm = r.base;
			np.refForm = r.ref;
			np.scolPart = r.part;
			for ( int k = 0; k < 3; k++ )
				np.pos[k] = r.pos[k];
			for ( int a = 0; a < 3; a++ )
				for ( int b = 0; b < 3; b++ )
					np.rot[a * 3 + b] = xf.rotation( a, b );   // the DRAWN rotation: ESM x tree yaw
			np.scale = r.scale;
			np.slot = qMin( lodLevel, 3 );
			np.model = model;
			np.isTree = isTree;
			np.mirrorU = mirrorU;
			np.treeHash = treeHash;
			for ( const LodSrcShape & s : shapes ) {
				np.hasAlpha = np.hasAlpha || s.hasAlpha;
				np.emits = np.emits || ( s.ownEmit && ( s.emitColor.red() > 0.0f || s.emitColor.green() > 0.0f || s.emitColor.blue() > 0.0f ) );
			}
			np.objectIndex = objectIndex;
			np.chunkX = chunkX; np.chunkY = chunkY; np.dim = dim;
			lodgenNativeAddPlacement( np );
		}
```
Notes: `xf.rotation` is the placement's `Matrix` AFTER the tree yaw was multiplied in
(`xf.rotation = xf.rotation * rz;`, line ~3284) -- that is the rotation the `.bto` draws and
the one the record carries (contract page, Deviations 2). `Matrix::operator()(r, c)` returns
`m[r][c]`, row-major, `world = R * local`. `model` here is the slot's path after fallback;
for a card placement (`cardShapes` non-empty) `shapes` are the card quads and `hasAlpha` is
theirs -- the emitter's library ignores this and loads the MNAM slot models itself.

### A4. one lit vertex (line 3630)

anchor (unique, two lines):
```
				const float ao = scene.ambientOcclusion( bucket.pos[v],
					bucket.nrm[v], 300.0f );
```
insert AFTER it (before `if ( opts.aoGrey )`):
```cpp
				if ( lodgenNativeActive() && opts.identity )
					lodgenNativeLighting( chunkX, chunkY, dim,
						qRound( bucket.col[v].red() * 255.0f ) + qRound( bucket.col[v].green() * 255.0f ) * 256,
						ao, opts.objectChannels ? bucket.sky[v] : 1.0f,
						opts.objectChannels ? bucket.groundBlend[v] : 0.0f );
```
`bucket.col[v]` still holds the identity colour here (R + G*256 = the object index; the AO
write into B comes on the next line). Without `--no-ao` the loop does not run and the emitter
writes ao 255 / sky 255 / ground 0 and counts the record as `unlit` in its census line.

---

## B. `src/nifcli.cpp` -- six sites

### B1. include (line 25)
after `#include "lodgen.h"`:
```
#include "nativeemit.h"
```

### B2. the option variables (line 5283)
anchor: `	QString lgLodmCheck, lgLodvCheck;`
insert after:
```cpp
	QString lgNativeDir, lgNativeVerifyLodo, lgNativeVerifyLodi, lgNativeFixture;
```

### B3. the option parse (line 5461)
anchor: `		else if ( t == QLatin1String( "--lodm-check" ) ) lgLodmCheck = next();`
insert after:
```cpp
		else if ( t == QLatin1String( "--native" ) ) lgNativeDir = next();
		else if ( t == QLatin1String( "--native-verify" ) ) { lgNativeVerifyLodo = next(); lgNativeVerifyLodi = next(); }
		else if ( t == QLatin1String( "--native-fixture" ) ) lgNativeFixture = next();
```

### B4. the usage text (line 5051)
anchor: `		  << "  lodgen ... --terrain-region ... [--slot-fallback]\n"`
insert BEFORE it:
```cpp
		  << "  lodgen ... --terrain-region ... --native <dir>\n"
		  << "                                          ALSO write the FO4CS-native far\n"
		  << "                                          field, <dir>/<ws>.lodo + .lodi\n"
		  << "                                          (docs/LODGEN_NATIVE_LODO_LODI.md);\n"
		  << "                                          the stock .BTO set is unchanged\n"
		  << "  lodgen --native-verify <ws.lodo> <ws.lodi>  read a pair back, every check\n"
		  << "  lodgen --native-fixture <dir>           write the synthetic known-answer pair\n"
```

### B5. `cmdLodgen` -- signature (line 2449), the two early commands (line 2486), arming (3392), writing (3510)

signature: append `, const QString & nativeDir, const QString & nativeVerifyLodo,
const QString & nativeVerifyLodi, const QString & nativeFixture` after `int cardAuxDiv`.

BEFORE the anchor `	if ( !lodmCheck.isEmpty() ) {` (line 2486) insert:
```cpp
	if ( !nativeFixture.isEmpty() ) {
		QStringList rep; QString nerr;
		if ( !lodNativeFixtureWrite( nativeFixture, &rep, &nerr ) ) { err() << "error: " << nerr << Qt::endl; return 1; }
		for ( const QString & l : rep ) out() << l << Qt::endl;
		return 0;
	}
	if ( !nativeVerifyLodo.isEmpty() ) {
		QString rep, nerr;
		if ( !lodgenNativeVerify( nativeVerifyLodo, nativeVerifyLodi, &rep, &nerr ) ) { out() << "native REFUSED " << nerr << Qt::endl; return 1; }
		out() << rep << Qt::endl;
		return 0;
	}
```
(`lodNativeFixtureWrite` is declared in `lodifile.h`; add `#include "lodifile.h"` beside B1.)

AFTER the anchor `		QDir().mkpath( outDir );` (line 3392, inside `if ( haveRegion )`) insert:
```cpp
		const QString nativeDataRoot = dataRoot.isEmpty()
			? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) : dataRoot;
		if ( !nativeDir.isEmpty() )
			lodgenNativeBegin( &world, nativeDir, lodgenNativeLoadModel, const_cast<QString *>( &nativeDataRoot ) );
```

BEFORE the anchor `		if ( arrays && !writtenBto.isEmpty() ) {` (line 3510) insert:
```cpp
		if ( lodgenNativeActive() ) {
			QString nrep, nerr;
			if ( !lodgenNativeWrite( &nrep, &nerr ) ) {
				err() << "error: " << nerr << Qt::endl;
				lodgenNativeEnd();
				return 1;
			}
			out() << nrep << Qt::endl;
			lodgenNativeEnd();
		}
```
### B6. the call (line 5966-5978)
append `, lgNativeDir, lgNativeVerifyLodo, lgNativeVerifyLodi, lgNativeFixture` after
`lgCardAuxDiv` in the `cmdLodgen(` call.

---

## C. What the first build then runs (in this order)

1. `release/NifSkope.exe -no-gui lodgen --native-fixture <dir>` then
   `python tests/spells/lodgen_native_decode.py <dir>/Synthetic.lodo <dir>/Synthetic.lodi --expect <dir>/Synthetic.expect.txt`
   -> must print `46 checks, 0 failures` / `RESULT PASS` exactly as the standalone tool did
   (`scratchpad/native0_20260910/fixture/`).
2. `... lodgen <esm> --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 --no-ao --data-root <data> --out-dir <o> --native <o>/Native`
   -> `<o>/Native/Commonwealth.lodo` + `.lodi`, and the census line
   `native: Commonwealth.lodo N bytes (bases ... ) ... B a placement`.
3. `python tests/spells/lodgen_native_decode.py <o>/Native/Commonwealth.lodo <o>/Native/Commonwealth.lodi --esm <esm> --worldspace 3C --chunk -20 24 4 --manifest <o>/Commonwealth.4.-20.24.BTO.manifest.txt`
   -> the ESM leg (base form, X/Y <= 0.125 u, rotation <= 0.02 deg incl. the tree yaw) and
   the manifest leg (every row in the table, scale <= 1/16384).
4. `tests/spells/lodgen_native_baseline.sh --check` -> 0 differ (the stock bake untouched).
5. The bucket-cap chunk: `--terrain-region -32 0 -1 31 --dim 32 --slot-fallback --native ...`
   and the decoder's `--manifest` leg on `Commonwealth.32.-32.0.BTO.manifest.txt`: the spec's
   asymmetric proof is `instances in the table with geometry` MINUS `manifest rows with
   geometry in the .BTO` >= 2,000 -- the decoder reports `rows not in the table` (must be 0)
   and the audit's identity-drop count on that chunk is the other side.
