#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
THE ONE HOOK-UP FOR LANE CELLVIEW2 (director clock 2026-09-19 17:15).

`--check` is the DEFAULT and writes nothing.  `--apply` refuses unless every
anchor still matches exactly once, and then writes every file at once.

The contract (skill `ww-anchored-hookup`):

  * AN ANCHOR IS READ OUT OF THE FILE, NEVER TYPED.  Each edit gives a list of
    line PREFIXES -- short, free of tabs-or-spaces runs and free of
    backslashes -- and the script finds the one place where those consecutive
    lines start with them and takes the ANCHOR from the file's own bytes,
    trailing comment, indentation, line ending and all.  The count is printed
    with `repr()` of the first line, which is the only rendering in which a tab
    and four spaces look different.
  * THE LINE ENDING IS IN THE ANCHOR because the anchor is a byte slice.
    src/glview.cpp is CRLF (23905 CRLF, 81 bare LF); NifSkope.pro,
    src/cellview.cpp, src/nifskope_ui.cpp and src/lodgen.cpp are LF-only.
    Measured, asserted per file, and the inserted text is converted to the
    file's own ending before anything is counted.
  * THE MARKER IS NOT THE ANCHOR.  Every inserted block carries the string
    `lane CELLVIEW2`; no anchor contains it.  The script prints the marker
    count before (which must be 0) and the count a successful apply must
    produce, so a resume decides "applied or not" from the marker and the byte
    delta, never from an anchor that still matches.
  * `after` inserts after the anchor, `before` inserts in front of it, and
    `replace` substitutes it.  No `after` anchors on a line ending in `{`.

WHAT IT WIRES
  (1) PICK WIRING   -- cellclick.* + cellpanel.*: one call in
      GLView::mouseReleaseEvent, one dock, one master row (OFF).
  (2) IDENTITY      -- cellidentity.*: CellOverlay::Identity keyed on the
      `.lodi` group id, with its census in the notes.
  (3) REAL TERRAIN  -- cellground.*: the LAND paint, welded by cellview.cpp's
      own writer; the old vertex-colour sheet stays as the refusal fallback.
  (4) MAGENTA       -- the two `materials/`-prepended absolute build paths in
      src/lodgen.cpp, repaired with the cut that already works in the same
      file at lodgenCollectMaterials (~1826).

AFTER APPLYING: qmake, then make.  Four new headers land in HEADERS and two of
them (cellclick.h, cellpanel.h) carry Q_OBJECT, so moc must be regenerated; an
incremental make without qmake will not link.
"""

import io
import os
import sys

ROOT = "E:/Projects/NifskopeWildWastelandEdition/"
T = chr(9)
NL = chr(10)
MARKER = "lane CELLVIEW2"

# line endings, measured 2026-09-19 with python byte counts (never grep)
EOL_EXPECT = {
    "NifSkope.pro": "lf",
    "src/cellview.cpp": "lf",
    "src/glview.cpp": "crlf",
    "src/nifskope_ui.cpp": "lf",
    "src/lodgen.cpp": "lf",
}

# ---------------------------------------------------------------------------
# the edits.  (path, mode, [line prefixes], text)
# ---------------------------------------------------------------------------

EDITS = []


def E(path, mode, prefixes, text):
    EDITS.append((path, mode, prefixes, text))


# --- 1/2: the project file -------------------------------------------------

E("NifSkope.pro", "after", [T + "src/cellpick.h"],
  T + "src/cellclick.h \\" + NL +
  T + "src/cellground.h \\" + NL +
  T + "src/cellidentity.h \\" + NL +
  T + "src/cellpanel.h \\" + NL +
  T + "src/cellpicktest.h \\" + NL)

E("NifSkope.pro", "after", [T + "src/cellpick.cpp"],
  T + "src/cellclick.cpp \\" + NL +
  T + "src/cellground.cpp \\" + NL +
  T + "src/cellidentity.cpp \\" + NL +
  T + "src/cellpanel.cpp \\" + NL +
  T + "src/cellpicktest.cpp \\" + NL)

# --- 3..14: the cell view --------------------------------------------------

E("src/cellview.cpp", "after", ['#include "cellpick.h"'],
  '#include "cellclick.h"' + T + T + "// lane CELLVIEW2" + NL +
  '#include "cellground.h"' + T + T + "// lane CELLVIEW2" + NL +
  '#include "cellidentity.h"' + T + "// lane CELLVIEW2" + NL)

E("src/cellview.cpp", "after",
  [T + "CellPickTable & picks = cellPickTableMutable();", T + "picks.clear();"],
  NL +
  T + "/* THE .lodi IDENTITY GROUPS (lane CELLVIEW2). One read per build, before a" + NL +
  T + " * single placement is drawn, so CellOverlay::Identity can colour a reference" + NL +
  T + " * by the LOD group the native lodgen put it in -- the view for judging the" + NL +
  T + " * ruled 64-unit proximity join. An absent, old or unreadable bake is NOT an" + NL +
  T + " * error here: the index stays empty, every reference draws grey, and the" + NL +
  T + " * notes line says which of those it was. */" + NL +
  T + "QHash<quint32, CellIdentity> identity;" + NL +
  T + "CellIdentityCensus identityCensus;" + NL +
  T + "QString identityError;" + NL +
  T + "if ( !spec.lodiPath.isEmpty() )" + NL +
  T * 2 + "cellIdentityLoad( spec.lodiPath, identity, &identityCensus, &identityError );" + NL)

E("src/cellview.cpp", "replace",
  [T * 2 + "case CellOverlay::Precombined:",
   T * 2 + "case CellOverlay::Identity:",
   T * 3 + "// both need a fact this build does not have",
   T * 3 + "break;"],
  T * 2 + "case CellOverlay::Precombined:" + NL +
  T * 3 + "// XCRI is not read by this build; see the notes line" + NL +
  T * 3 + "break;" + NL +
  T * 2 + "case CellOverlay::Identity: {" + NL +
  T * 3 + "/* THE .lodi GROUP (lane CELLVIEW2). The key is the group the bake put" + NL +
  T * 3 + " * this reference in, made file-wide in cellidentity.cpp because a" + NL +
  T * 3 + " * `.lodi` group id is dense PER CHUNK. A reference the bake never saw" + NL +
  T * 3 + " * -- no LOD model, or outside the baked area -- leaves haveKey false" + NL +
  T * 3 + " * and falls to the grey `unknown` bucket below, which the legend" + NL +
  T * 3 + " * counts: a grey reference is a stated fact, not a gap. */" + NL +
  T * 3 + "const auto idIt = identity.constFind( p.ref );" + NL +
  T * 3 + "if ( idIt != identity.constEnd() ) {" + NL +
  T * 4 + "okey = quint64( idIt->group ) | 0x400000000000ULL;" + NL +
  T * 4 + "haveKey = true;" + NL +
  T * 3 + "}" + NL +
  T * 3 + "break;" + NL +
  T * 2 + "}" + NL)

E("src/cellview.cpp", "after", [T * 2 + "pick.hasLod = lb.hasLod;"],
  T * 2 + "{   // lane CELLVIEW2: the `.lodi` group, for the pick panel's rows" + NL +
  T * 3 + "const auto idIt = identity.constFind( p.ref );" + NL +
  T * 3 + "if ( idIt != identity.constEnd() ) {" + NL +
  T * 4 + "pick.group = idIt->group;" + NL +
  T * 4 + "pick.groupSize = idIt->size;" + NL +
  T * 4 + "pick.haveGroup = true;" + NL +
  T * 3 + "}" + NL +
  T * 2 + "}" + NL)

E("src/cellview.cpp", "after", [T + "int landsDrawn = 0, waterCells = 0;"],
  T + "QString groundNote;   // lane CELLVIEW2: the painted ground's own census" + NL)

E("src/cellview.cpp", "after", [T * 2 + "gridB.withColour = true;"],
  NL +
  T * 2 + "/* THE PAINTED GROUND (lane CELLVIEW2). Built for the WHOLE rectangle in" + NL +
  T * 2 + " * one call, so every landscape texture is resolved once, then welded" + NL +
  T * 2 + " * into this file's own buckets with this file's own vertex writer --" + NL +
  T * 2 + " * there is no second geometry path. The HEIGHTS were never missing" + NL +
  T * 2 + " * (VHGT is decoded in EsmWorld::land and the first build already drew" + NL +
  T * 2 + " * the relief); what was missing is the PAINT, and that is what this" + NL +
  T * 2 + " * adds. It does not blend -- it is a hard-edged mosaic of the strongest" + NL +
  T * 2 + " * layer per quad, and the census line says so (see cellground.h). */" + NL +
  T * 2 + "QVector<Bucket> groundBuckets;" + NL +
  T * 2 + "if ( spec.terrain ) {" + NL +
  T * 3 + "CellGroundBuild gb;" + NL +
  T * 3 + "QString gerr;" + NL +
  T * 3 + "if ( cellBuildGround( world, x0, y0, x1, y1, origin[0], origin[1]," + NL +
  T * 5 + "CELL_GROUND_TILING, gb, &gerr ) && !gb.quads.empty() ) {" + NL +
  T * 4 + "landsDrawn = gb.cells;" + NL +
  T * 4 + "groundNote = cellGroundLegend( gb );" + NL +
  T * 4 + "groundBuckets.resize( gb.buckets.size() );" + NL +
  T * 4 + "for ( int bi = 0; bi < gb.buckets.size(); bi++ ) {" + NL +
  T * 5 + "Bucket & gbk = groundBuckets[bi];" + NL +
  T * 5 + "const CellGroundBucket & src = gb.buckets.at( bi );" + NL +
  T * 5 + "/* matString takes EITHER a `.bgsm` or a diffuse texture -- the" + NL +
  T * 5 + " * same slot the model buckets use, so a material-backed TXST" + NL +
  T * 5 + " * and a plain TX00 travel the one path. An EMPTY diffuse is" + NL +
  T * 5 + " * not neutral: it binds the missing-texture magenta under" + NL +
  T * 5 + " * Scene::DoErrorColor, which is why the bare bucket keeps the" + NL +
  T * 5 + " * `#AARRGGBB` pseudo-texture emitBucket already gives it. */" + NL +
  T * 5 + "gbk.name = src.diffuse.isEmpty()" + NL +
  T * 6 + "? QStringLiteral( \"landscape\" )" + NL +
  T * 6 + ": QFileInfo( src.diffuse ).fileName();" + NL +
  T * 5 + "gbk.matString = src.diffuse;" + NL +
  T * 5 + "gbk.normalTex = src.normal;" + NL +
  T * 5 + "gbk.withColour = true;" + NL +
  T * 4 + "}" + NL +
  T * 4 + "for ( const CellGroundQuad & q : gb.quads ) {" + NL +
  T * 5 + "if ( q.bucket < 0 || q.bucket >= groundBuckets.size() )" + NL +
  T * 6 + "continue;" + NL +
  T * 5 + "Bucket & gbk = groundBuckets[q.bucket];" + NL +
  T * 5 + "const int base = int( gbk.verts.size() );" + NL +
  T * 5 + "for ( int k = 0; k < 4; k++ ) {" + NL +
  T * 6 + "OutVert o;" + NL +
  T * 6 + "o.pos = Vector3( q.v[k].p[0], q.v[k].p[1], q.v[k].p[2] );" + NL +
  T * 6 + "o.nrm = Vector3( q.nrm[0], q.nrm[1], q.nrm[2] );" + NL +
  T * 6 + "o.tan = Vector3( 1.0f, 0.0f, 0.0f );" + NL +
  T * 6 + "o.bit = Vector3::crossproduct( o.nrm, o.tan );" + NL +
  T * 6 + "if ( o.bit.length() < 1.0e-6f )" + NL +
  T * 7 + "o.bit = Vector3( 0.0f, 1.0f, 0.0f );" + NL +
  T * 6 + "o.uv = Vector2( q.v[k].uv[0], q.v[k].uv[1] );" + NL +
  T * 6 + "for ( int c = 0; c < 3; c++ )" + NL +
  T * 7 + "o.chan[c] = q.v[k].rgb[c];" + NL +
  T * 6 + "gbk.verts.push_back( o );" + NL +
  T * 5 + "}" + NL +
  T * 5 + "// the same quint16 index pair appendQuad writes; emitBucket does" + NL +
  T * 5 + "// the splitting, on triangle boundaries" + NL +
  T * 5 + "gbk.tris.push_back( Triangle( quint16( base ), quint16( base + 1 )," + NL +
  T * 6 + "quint16( base + 2 ) ) );" + NL +
  T * 5 + "gbk.tris.push_back( Triangle( quint16( base ), quint16( base + 2 )," + NL +
  T * 6 + "quint16( base + 3 ) ) );" + NL +
  T * 4 + "}" + NL +
  T * 3 + "} else {" + NL +
  T * 4 + "groundNote = QStringLiteral( \"ground: the painted mosaic REFUSED (%1)" +
  " -- the vertex-colour sheet was drawn instead\" )" + NL +
  T * 5 + ".arg( gerr.isEmpty() ? QStringLiteral( \"no LAND in the rectangle\" ) : gerr );" + NL +
  T * 3 + "}" + NL +
  T * 2 + "}" + NL)

E("src/cellview.cpp", "replace",
  [T * 4 + "const bool haveLand = spec.terrain && world.land( x, y, land );"],
  T * 4 + "/* lane CELLVIEW2: the PAINTED ground above owns this rectangle when" + NL +
  T * 4 + " * it built anything. The vertex-colour-only sheet stays as the" + NL +
  T * 4 + " * fallback for a refusal, so the viewer is never left with no ground" + NL +
  T * 4 + " * at all -- and the two are never drawn on top of each other. */" + NL +
  T * 4 + "const bool haveLand = spec.terrain && groundBuckets.isEmpty()" + NL +
  T * 5 + "&& world.land( x, y, land );" + NL)

E("src/cellview.cpp", "after",
  [T * 2 + "if ( !ground.verts.empty() )",
   T * 3 + "buckets.insert( QStringLiteral( "],
  T * 2 + "for ( int bi = 0; bi < groundBuckets.size(); bi++ ) {   // lane CELLVIEW2" + NL +
  T * 3 + "if ( groundBuckets[bi].verts.empty() )" + NL +
  T * 4 + "continue;" + NL +
  T * 3 + "// sorted after the sheet and before the water, one shape per texture" + NL +
  T * 3 + "buckets.insert( QStringLiteral( \"\\x01land%1\" )" + NL +
  T * 4 + ".arg( bi, 3, 10, QLatin1Char( '0' ) ), groundBuckets[bi] );" + NL +
  T * 2 + "}" + NL)

E("src/cellview.cpp", "before",
  [T + "nif->holdUpdates( false );", T + "nif->updateModel();"],
  T + "/* THE PICK HIGHLIGHT (lane CELLVIEW2) -- created ONCE, here, and MOVED on" + NL +
  T + " * every click (src/cellclick.h). The scene WELDS, so the picked reference" + NL +
  T + " * is a few hundred vertices inside a shape holding a hundred thousand and" + NL +
  T + " * there is nothing for the application's own selection highlight to light" + NL +
  T + " * up. This is twelve edge bars of the picked placement's world box, and it" + NL +
  T + " * is document geometry, so a screenshot of a pick is a screenshot of the" + NL +
  T + " * document and no block appears or disappears when one is made. Forget" + NL +
  T + " * first: the previous scene's shape went with its document, and the dock" + NL +
  T + " * has to be told the rows it is showing are gone. */" + NL +
  T + "cellHighlightForget();" + NL +
  T + "{" + NL +
  T * 2 + "const float hlOrigin[3] = { origin[0], origin[1], origin[2] };" + NL +
  T * 2 + "cellHighlightCreate( nif, iRoot, hlOrigin );" + NL +
  T + "}" + NL +
  NL)

E("src/cellview.cpp", "replace",
  [T * 2 + 's << "  ground: " << landsDrawn',
   T * 2 + '  << " (the splat layers are NOT sampled'],
  T * 2 + "if ( groundNote.isEmpty() ) {   // lane CELLVIEW2" + NL +
  T * 3 + "s << \"  ground: \" << landsDrawn << \" LAND cells, vertex colour only\"" + NL +
  T * 3 + "  << \" (the splat layers are NOT sampled -- that is the terrain bake's compositor)\\n\";" + NL +
  T * 2 + "} else {" + NL +
  T * 3 + "s << \"  \" << groundNote << \"\\n\";" + NL +
  T * 2 + "}" + NL)

E("src/cellview.cpp", "after", [T * 2 + 's << "  water: " << waterCells'],
  T * 2 + "if ( !identityCensus.path.isEmpty() || !identityError.isEmpty() ) {   // lane CELLVIEW2" + NL +
  T * 3 + "s << \"  \" << cellIdentityLegend( identityCensus );" + NL +
  T * 3 + "if ( !identityError.isEmpty() )" + NL +
  T * 4 + "s << \" -- REFUSED: \" << identityError;" + NL +
  T * 3 + "s << \"\\n\";" + NL +
  T * 2 + "}" + NL)

E("src/cellview.cpp", "replace",
  [T * 2 + "if ( spec.overlay == CellOverlay::Identity )",
   T * 3 + 's << " -- REFUSED: no `.lodi` bake is loaded'],
  T * 2 + "if ( spec.overlay == CellOverlay::Identity && identity.isEmpty() )   // lane CELLVIEW2" + NL +
  T * 3 + "s << \" -- REFUSED: \" << ( identityError.isEmpty()" + NL +
  T * 4 + "? QStringLiteral( \"no `.lodi` bake is loaded (WW_CELL_LODI), so every placement is grey\" )" + NL +
  T * 4 + ": identityError );" + NL)

# --- 15/16: the viewport ---------------------------------------------------

E("src/glview.cpp", "after", ['#include "nifskope.h"'],
  '#include "cellclick.h"' + T + T + "// lane CELLVIEW2: one call, in mouseReleaseEvent" + NL)

E("src/glview.cpp", "before",
  ["", T * 2 + "if ( !isColorPicker && event->button() == selectMouseButton() ) {"],
  NL +
  T * 2 + "/* THE CELL VIEW'S PICK (lane CELLVIEW2). A `.wwcell` scene WELDS every" + NL +
  T * 2 + " * placement of a (base, material) bucket into ONE BSTriShape, so the" + NL +
  T * 2 + " * ordinary selection below would hand back a shape holding a hundred" + NL +
  T * 2 + " * thousand vertices of unrelated references -- the one selection a" + NL +
  T * 2 + " * person clicking in a cell view never wants. cellPickClick does the" + NL +
  T * 2 + " * ray test against the placement table (src/cellpick.h), moves the" + NL +
  T * 2 + " * highlight and tells the dock. It returns false, and touches nothing," + NL +
  T * 2 + " * whenever its master row is unticked or no cell scene is open, so a" + NL +
  T * 2 + " * user who never ticks the row loses nothing they had. */" + NL +
  T * 2 + "if ( !isColorPicker && event->button() == selectMouseButton() ) {" + NL +
  T * 3 + "Vector3 cellRayO, cellRayD;" + NL +
  T * 3 + "mouseRayWorld( QPointF( evtPos ), cellRayO, cellRayD );" + NL +
  T * 3 + "const float cellO[3] = { cellRayO[0], cellRayO[1], cellRayO[2] };" + NL +
  T * 3 + "const float cellD[3] = { cellRayD[0], cellRayD[1], cellRayD[2] };" + NL +
  T * 3 + "if ( cellPickClick( model, cellO, cellD ) ) {" + NL +
  T * 4 + "update();" + NL +
  T * 4 + "return;" + NL +
  T * 3 + "}" + NL +
  T * 2 + "}" + NL)

# --- 17/18: the window -----------------------------------------------------

E("src/nifskope_ui.cpp", "after", ['#include "esmdata.h"'],
  '#include "cellclick.h"' + T + T + "// lane CELLVIEW2" + NL +
  '#include "cellpanel.h"' + T + T + "// lane CELLVIEW2" + NL +
  '#include "cellpicktest.h"' + T + "// lane CELLVIEW2" + NL)

E("src/nifskope_ui.cpp", "after",
  [T * 2 + "connect( this, &NifSkope::completeLoading, bodyBuild,",
   T * 4 + " [bodyBuild]( bool, QString & ) { bodyBuild->onSceneRebuilt(); } );",
   T + "}"],
  NL +
  T + "/* THE CELL PICK dock and its master (lane CELLVIEW2). Flat Name|Value rows" + NL +
  T + " * for the reference under the cursor in a `.wwcell` scene, straight out of" + NL +
  T + " * CellPickTable::rowsFor() so the dock and the harness read the same" + NL +
  T + " * function. A LOCAL, exactly like the Body Build dock above: the harness" + NL +
  T + " * finds the panel by object name and nothing else in the window addresses" + NL +
  T + " * it, so nifskope.h gains no member while another lane holds that file. */" + NL +
  T + "{" + NL +
  T * 2 + "QDockWidget * dCellPick = new QDockWidget( tr( \"Reference\" ), this );" + NL +
  T * 2 + "dCellPick->setObjectName( \"CellPickDock\" );" + NL +
  T * 2 + "CellPickPanel * cellPick = new CellPickPanel( dCellPick );" + NL +
  T * 2 + "dCellPick->setWidget( cellPick );" + NL +
  T * 2 + "dCellPick->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea" + NL +
  T * 3 + "| Qt::BottomDockWidgetArea );" + NL +
  T * 2 + "addDockWidget( Qt::RightDockWidgetArea, dCellPick );" + NL +
  T * 2 + "dCellPick->hide();" + NL +
  NL +
  T * 2 + "/* THE MASTER SHIPS OFF, WITH A ROW (bungo's standing rule: every feature" + NL +
  T * 2 + " * master ships off, and nothing exists without a row to reach it)." + NL +
  T * 2 + " * While it is unticked cellPickClick() returns false, the viewport's" + NL +
  T * 2 + " * click falls through to the ordinary block selection and this lane" + NL +
  T * 2 + " * changes nothing. The colour overlays INSIDE the cell view are not" + NL +
  T * 2 + " * masters and get no row -- they are one view's display setting. */" + NL +
  T * 2 + "QAction * aCellPick = new QAction( tr( \"Cell Pick Panel\" ), this );" + NL +
  T * 2 + "aCellPick->setCheckable( true );" + NL +
  T * 2 + "aCellPick->setChecked( false );" + NL +
  T * 2 + "cellPickSetEnabled( false );" + NL +
  T * 2 + "connect( aCellPick, &QAction::toggled, this, [dCellPick]( bool on ) {" + NL +
  T * 3 + "cellPickSetEnabled( on );" + NL +
  T * 3 + "dCellPick->setVisible( on );" + NL +
  T * 3 + "CellPickBus::instance()->sceneChanged();" + NL +
  T * 2 + "} );" + NL +
  T * 2 + "ui->mRender->addAction( aCellPick );" + NL +
  NL +
  T * 2 + "/* THE SELF-TEST (WW_CELLPICK_TEST=<report path>, src/cellpicktest.h)." + NL +
  T * 2 + " * The click path starts in the viewport and ends in this dock, so it" + NL +
  T * 2 + " * is measured INSIDE the running window and the answers are written" + NL +
  T * 2 + " * to a file tests/spells/cell_pick.sh greps -- never judged from a" + NL +
  T * 2 + " * screenshot. It runs after completeLoading because the cell scene" + NL +
  T * 2 + " * the placement table describes is built by that load, and it ticks" + NL +
  T * 2 + " * the master itself: a harness forces the state it measures rather" + NL +
  T * 2 + " * than inheriting it from QSettings. */" + NL +
  T * 2 + "if ( !qgetenv( \"WW_CELLPICK_TEST\" ).isEmpty() ) {" + NL +
  T * 3 + "aCellPick->setChecked( true );" + NL +
  T * 3 + "connect( this, &NifSkope::completeLoading, this," + NL +
  T * 4 + "[this, cellPick]( bool, QString & ) {" + NL +
  T * 5 + "cellPickSelfTest( nif, cellPick, QString::fromLocal8Bit(" + NL +
  T * 6 + "qgetenv( \"WW_CELLPICK_TEST\" ) ) );" + NL +
  T * 4 + "} );" + NL +
  T * 2 + "}" + NL +
  T + "}" + NL)

# --- 19/20: the magenta ----------------------------------------------------

E("src/lodgen.cpp", "replace",
  [T * 5 + "if ( !mp.startsWith( QStringLiteral( \"materials/\" ), Qt::CaseInsensitive ) )",
   T * 6 + "mp.prepend( QStringLiteral( \"materials/\" ) );"],
  T * 5 + "/* AN ABSOLUTE BETHESDA BUILD PATH IS CUT, NOT PREFIXED (lane" + NL +
  T * 5 + " * CELLVIEW2). A BSLightingShaderProperty can name its material with" + NL +
  T * 5 + " * a path off the build machine; prepending `materials/` to one of" + NL +
  T * 5 + " * those produced `materials/c:/.../materials/x.bgsm`, which resolves" + NL +
  T * 5 + " * to nothing, leaves the diffuse slot EMPTY, and an empty diffuse" + NL +
  T * 5 + " * binds the missing-texture MAGENTA under Scene::DoErrorColor" + NL +
  T * 5 + " * (src/gl/renderer.cpp ~951) rather than reading as untextured." + NL +
  T * 5 + " * The right shape is already in this file at lodgenCollectMaterials" + NL +
  T * 5 + " * (~1826): cut everything before the LAST `materials/`, and only" + NL +
  T * 5 + " * prepend when the path has none at all. */" + NL +
  T * 5 + "const int mmi = mp.lastIndexOf( QStringLiteral( \"materials/\" ), -1," + NL +
  T * 6 + "Qt::CaseInsensitive );" + NL +
  T * 5 + "if ( mmi > 0 )" + NL +
  T * 6 + "mp.remove( 0, mmi );" + NL +
  T * 5 + "else if ( !mp.startsWith( QStringLiteral( \"materials/\" ), Qt::CaseInsensitive ) )" + NL +
  T * 6 + "mp.prepend( QStringLiteral( \"materials/\" ) );" + NL)

E("src/lodgen.cpp", "replace",
  [T * 2 + "if ( !path.startsWith( QStringLiteral( \"materials/\" ), Qt::CaseInsensitive ) )",
   T * 3 + "path.prepend( QStringLiteral( \"materials/\" ) );"],
  T * 2 + "/* THE SAME CUT (lane CELLVIEW2) -- this is the LANDSCAPE texture" + NL +
  T * 2 + " * loader, which a material-backed TXST sends through a `.bgsm`, and" + NL +
  T * 2 + " * it is the path the cell view's painted ground now takes as well." + NL +
  T * 2 + " * See the note at the model loader above. */" + NL +
  T * 2 + "const int pmi = path.lastIndexOf( QStringLiteral( \"materials/\" ), -1," + NL +
  T * 3 + "Qt::CaseInsensitive );" + NL +
  T * 2 + "if ( pmi > 0 )" + NL +
  T * 3 + "path.remove( 0, pmi );" + NL +
  T * 2 + "else if ( !path.startsWith( QStringLiteral( \"materials/\" ), Qt::CaseInsensitive ) )" + NL +
  T * 3 + "path.prepend( QStringLiteral( \"materials/\" ) );" + NL)


# ---------------------------------------------------------------------------
# the machinery
# ---------------------------------------------------------------------------

def read(path):
    with open(ROOT + path, "rb") as f:
        return f.read()


def eol_of(raw):
    crlf = raw.count(b"\r\n")
    lf = raw.count(b"\n") - crlf
    return ("crlf" if crlf > lf else "lf"), crlf, lf


def line_table(raw):
    """(list of line byte-slices without the newline, list of start offsets)."""
    lines = raw.split(b"\n")
    offs = []
    o = 0
    for ln in lines:
        offs.append(o)
        o += len(ln) + 1
    return lines, offs


def locate(raw, prefixes):
    """The exact bytes of the consecutive lines whose starts match `prefixes`.

    Returns (anchor_bytes, first_line_number, real_first_line) or raises."""
    lines, offs = line_table(raw)
    pb = [p.encode("utf-8") for p in prefixes]
    hits = []
    for i in range(0, len(lines) - len(pb) + 1):
        ok = True
        for j, p in enumerate(pb):
            if not lines[i + j].startswith(p):
                ok = False
                break
        if ok:
            hits.append(i)
    if len(hits) != 1:
        raise AssertionError("prefix block matches %d times (want 1): %r"
                             % (len(hits), prefixes[0]))
    i = hits[0]
    k = i + len(pb) - 1
    start = offs[i]
    end = offs[k] + len(lines[k]) + 1          # include the newline
    if end > len(raw):                          # the file's last line, no newline
        end = len(raw)
    return raw[start:end], i + 1, lines[i]


def to_eol(text, kind):
    b = text.encode("utf-8")
    return b.replace(b"\n", b"\r\n") if kind == "crlf" else b


def run(apply_it):
    files = {}
    for path, _m, _p, _t in EDITS:
        files.setdefault(path, None)

    total_bad = 0
    plan = {}

    for path in files:
        raw = read(path)
        kind, crlf, lf = eol_of(raw)
        expect = EOL_EXPECT.get(path)
        flag = "OK" if kind == expect else "*** UNEXPECTED (was %s) ***" % expect
        before_marker = raw.count(MARKER.encode("utf-8"))
        print("=== %s  bytes=%d  crlf=%d  bare_lf=%d  -> %s %s"
              % (path, len(raw), crlf, lf, kind, flag))
        print("    marker %r before: %d (must be 0 for a fresh tree)"
              % (MARKER, before_marker))
        if kind != expect:
            total_bad += 1
        files[path] = (raw, kind, crlf)
        plan[path] = []

    for n, (path, mode, prefixes, text) in enumerate(EDITS, 1):
        raw, kind, _crlf = files[path]
        try:
            anchor, lineno, real = locate(raw, prefixes)
        except AssertionError as e:
            print("  [%02d] %-20s %-7s REFUSED: %s" % (n, path, mode, e))
            total_bad += 1
            continue
        count = raw.count(anchor)
        ins = to_eol(text, kind)
        if mode == "after":
            new = anchor + ins
        elif mode == "before":
            new = ins + anchor
        elif mode == "replace":
            new = ins
        else:
            raise AssertionError("bad mode " + mode)
        mark = "ok " if count == 1 else "BAD"
        if count != 1:
            total_bad += 1
        print("  [%02d] %-20s %-7s line %-6d anchor x%d %s  %s"
              % (n, path, mode, lineno, count, mark, repr(real[:64])))
        print("       first inserted line: %s"
              % repr(ins.split(b"\n")[0][:78]))
        plan[path].append((anchor, new))

    print()
    for path in files:
        raw, kind, crlf_before = files[path]
        new_raw = raw
        for anchor, new in plan[path]:
            if new_raw.count(anchor) != 1:
                print("!! %s: an anchor stopped being unique while planning" % path)
                total_bad += 1
                continue
            new_raw = new_raw.replace(anchor, new, 1)
        crlf_after = new_raw.count(b"\r\n")
        added = 0
        for _a, nw in plan[path]:
            added += nw.count(b"\r\n")
        for a, _nw in plan[path]:
            added -= a.count(b"\r\n")
        print("--- %s: %d edits, %d -> %d bytes (delta %+d), CR %d -> %d (edits carry %+d)"
              % (path, len(plan[path]), len(raw), len(new_raw),
                 len(new_raw) - len(raw), crlf_before, crlf_after, added))
        print("    marker %r after: %d" % (MARKER, new_raw.count(MARKER.encode("utf-8"))))
        if crlf_after != crlf_before + added:
            print("    !! CR accounting does not balance")
            total_bad += 1
        files[path] = (raw, kind, crlf_before, new_raw)

    print()
    if total_bad:
        print("REFUSED: %d problem(s). Nothing was written." % total_bad)
        return 1
    if not apply_it:
        print("CHECK ONLY: %d edits over %d files all match exactly once. "
              "Nothing was written." % (len(EDITS), len(files)))
        return 0

    for path, tup in files.items():
        with open(ROOT + path, "wb") as f:
            f.write(tup[3])
        print("wrote %s" % path)
    print()
    print("APPLIED. Now: qmake, then make -- four new headers join HEADERS and")
    print("cellclick.h / cellpanel.h carry Q_OBJECT, so moc must be regenerated.")
    return 0


if __name__ == "__main__":
    sys.exit(run("--apply" in sys.argv))
