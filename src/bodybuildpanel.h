/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef BODYBUILDPANEL_H
#define BODYBUILDPANEL_H

/*! @file bodybuildpanel.h  The Body Build dock -- Fallout 4's character-creator
 *  build triangle, previewed on the character that is loaded.
 *
 *  Lane GLTFEXPORT1, sub-lane, 2026-09-19. The ARITHMETIC is not here: every
 *  number this panel shows comes out of `src/bodybuild.h` (the RACE record's
 *  Bone Scale Data and TESNPC::FillBoneScaleMap's own combination), and this
 *  file holds only the control, the preview and the sentence.
 *
 *  IT WRITES NO FILE, EVER. It is a viewer preview and nothing else: the
 *  weighted per-axis scales go straight onto the `*_skin` NiNodes of the loaded
 *  scene, and every node's pre-preview local transform is kept BY VALUE so that
 *  switching the preview off puts back the same bit patterns that were read.
 *  That is `HkxClipEntry::saved` / `HkxPlayback::restore()`'s discipline, and it
 *  is followed here for the same reason: a preview that cannot be undone exactly
 *  is an edit nobody asked for.
 *
 *  IT COMPOSES ON TOP OF A PLAYING CLIP. A `*_skin` bone carries no animation
 *  track in any shipped FO4 clip -- they exist only in skeleton.nif, and
 *  skeleton.hkx has none of them -- so the preview and `HkxPlayback::applyLocal`
 *  never write the same node, and the preview touches the ROTATION basis alone
 *  (a NifSkope Transform carries one scale, three are needed, so a per-axis
 *  scale is folded into the 3x3 the way `Transform( translation, Vector3 )`
 *  already does it). A NIF with no `*_skin` node at all -- skeleton.hkx is the
 *  case that matters -- is REFUSED in one sentence and nothing is written.
 *
 *  THE HOOK-UP is the parent lane's. Two touches are needed and neither is made
 *  here: `bodybuildpanel.cpp` under SOURCES **and `bodybuildpanel.h` under
 *  HEADERS** in NifSkope.pro (both classes below carry Q_OBJECT, so moc must
 *  run over this header or the vtables are missing at link), and one call to
 *  `wwBodyBuildHarness( this )` beside the other harnesses in nifskope_ui.cpp.
 *
 *  THE HARNESS forces the state it measures and never inherits QSettings
 *  (CONSTITUTION rule 6): with `WW_BODY_BUILD=<race>|<gender>|<t>,<m>,<f>` in
 *  the environment the panel reads no saved setting and writes none, takes that
 *  race, that gender and those three weights, and `wwBodyBuildHarness` runs the
 *  gates and writes `release/ww_body_build_test.log`. `WW_BODY_BUILD_SHOT=<png>`
 *  also grabs the dock.
 */

#include "bodybuild.h"
#include "data/niftypes.h"

#include <QHash>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class GLView;
class Node;
class NifModel;
class NifSkope;
class Scene;

class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QSplitter;
class QToolButton;

/*! THE TRIANGLE -- the game's own control, corners THIN (top left),
 *  MUSCULAR (bottom) and FAT (top right), a draggable handle, barycentric
 *  weights that always sum to 1, and the centre as the neutral default.
 *
 *  The maths is `src/bodybuild.h`'s: the unit equilateral triangle
 *  P0 = (0,h), P1 = (0.5,0), P2 = (1,h), h = sqrt(3)/2, which is the triangle
 *  `bodyBuildCentroidK` measures its k against -- so the handle's distance from
 *  the centre IS the k the scale formula uses, and the control cannot disagree
 *  with the arithmetic it drives.
 *
 *  Blender is the reference for the gesture (its colour picker's triangle): a
 *  press anywhere inside jumps the handle there and begins the drag, the handle
 *  is a ringed dot, and the wheel belongs to the panel, not to the control.
 *  Colours come from `wwSkinColor` only.
 */
class BodyBuildTriangle final : public QWidget
{
	Q_OBJECT

public:
	explicit BodyBuildTriangle( QWidget * parent = nullptr );

	float thin() const { return wThin; }
	float muscular() const { return wMuscular; }
	float fat() const { return wFat; }

	//! Normalised and clamped into the triangle before it is kept.
	void setWeights( float t, float m, float f );
	//! The neutral default: the centroid, (1/3, 1/3, 1/3).
	void setCentre();

	//! Re-read the skin table (a theme switch).
	void restyle() { update(); }

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

signals:
	//! Emitted whenever the three weights change, from any source.
	void weightsChanged( float thin, float muscular, float fat );

protected:
	void paintEvent( QPaintEvent * ) override;
	void mousePressEvent( QMouseEvent * ) override;
	void mouseMoveEvent( QMouseEvent * ) override;
	void mouseReleaseEvent( QMouseEvent * ) override;
	//! The wheel scrolls the panel, not the build (the number fields' rule).
	void wheelEvent( QWheelEvent * ) override;

private:
	//! The three corners in widget pixels, and the handle's point.
	void corners( QPointF & p0, QPointF & p1, QPointF & p2 ) const;
	QPointF handlePoint() const;
	//! Widget point -> barycentric weights, clamped into the triangle.
	void takePoint( const QPointF & p );

	float wThin = 1.0f / 3.0f;
	float wMuscular = 1.0f / 3.0f;
	float wFat = 1.0f / 3.0f;
	bool dragging = false;
};

/*! The Body Build dock.
 *
 *  Three bands (nifskope-ww-panel-style): the settings in a QScrollArea, the
 *  live part -- the triangle -- under them on a QSplitter whose sizes persist,
 *  and the summary-or-refusal line with the action bar pinned under both, never
 *  scrolling away. One `label | field` grid per section, one field per row, one
 *  label width for the whole panel. No blurbs: a label and a control, and the
 *  sentence is the pinned line.
 */
class BodyBuildPanel final : public QWidget
{
	Q_OBJECT

public:
	explicit BodyBuildPanel( QWidget * parent = nullptr );
	~BodyBuildPanel() override;

	void setNif( NifModel * model );
	void setGLView( GLView * view );

	// ---- the state, public so a harness can FORCE what it measures
	//! By RACE editor id ("HumanRace"), case-insensitively; unknown = refused.
	void setRaceByEditorId( const QString & editorId );
	void setGender( int gender );                  //!< 0 male, 1 female
	void setWeights( float t, float m, float f );  //!< normalised
	void setPreviewOn( bool on );

	// ---- readers for the harness
	//! The per-axis scale this panel has APPLIED to one bone, (1,1,1) if none.
	Vector3 appliedScaleFor( const QString & boneName ) const;
	//! Bones of the race's set that found a `*_skin` node and were scaled.
	int appliedCount() const { return applied.count(); }
	//! Bones of the race's set with no node in this file -- named in the tooltip.
	int refusedCount() const { return missing.count(); }
	QStringList refusedBoneNames() const { return missing; }
	//! The pinned line, and whether it is the refusal.
	QString summaryText() const;
	QString summaryDetail() const;
	bool summaryIsRefusal() const { return noteRefusal; }

	bool previewOn() const;
	int gender() const;
	QString raceEditorId() const;
	const BodyBuildTriangle * triangle() const { return tri; }
	//! Does the loaded file carry any `*_skin` node at all?
	bool hasSkinBones() const;
	//! The .esm the race list was read from ("" = none was found).
	QString esmPath() const { return esm; }
	//! Is this panel running from the environment rather than from QSettings?
	bool forcedByEnvironment() const { return forced; }

public slots:
	//! Put every node back exactly as it was and drop the record.
	void clearPreview();
	/*! The scene was rebuilt or cleared: the Node objects this panel saved
	 *  transforms for are GONE, so the record is dropped WITHOUT restoring
	 *  (restoring onto a re-used node id would write a stale transform), and
	 *  the preview is worked out again against the nodes that are there now. */
	void onSceneRebuilt();
	//! Re-read the selected race's table and the bones, then re-apply.
	void refresh();

private slots:
	void raceChosen();
	void genderChosen();
	void fieldEdited();
	void triangleMoved( float t, float m, float f );
	void previewToggled( bool on );
	void resetToCentre();
	//! Owns the action bar's enabled state and the pinned sentence.
	void refreshSummary();

private:
	void buildUi();
	void loadRaceList();
	void loadTable();
	//! Restore, then (when the preview is on) write the scales again.
	void reapply();
	//! Write every node back byte for byte and forget the record.
	void restoreNodes();
	/*! Take the census of the race's bones against the scene, and -- when
	 *  `write` -- put the weighted scales on the `*_skin` nodes. Returns the
	 *  number of bones that found a node. */
	int applyNodes( bool write );
	void say( const QString & text, bool refusal, const QString & detail = QString() );
	void pushWeightsToFields();
	Scene * scene() const;
	void redraw();
	//! Only when the panel is not forced by the environment.
	void saveSetting( const QString & key, const QVariant & value );

	QPointer<NifModel> nif;
	/*! A QPointer, not a raw one, and deliberately: the preview lives on the
	 *  scene's nodes and has to be taken back OFF them when this panel goes
	 *  away -- and at teardown the view may already have gone. A QPointer nulls
	 *  itself when its QObject is destroyed, so the destructor's restore either
	 *  reaches live nodes or does nothing, and never a dangling pointer. */
	QPointer<GLView> glView;

	// the data layer's answers
	QString esm;                                   //!< the Fallout4.esm found
	QStringList esmLookedIn;                       //!< what was tried, for the refusal
	QVector<QPair<quint32, QString>> races;        //!< formID -> EDID, with bone scale data
	BodyBuildTable table;
	QString tableError;                            //!< the loader's refusal sentence, or ""

	// the preview's record -- the pattern of HkxClipEntry::saved
	QHash<int, Transform> savedLocal;              //!< Node::id() -> the local BEFORE the preview
	QHash<QString, Vector3> applied;               //!< bone name -> the scale written
	QStringList missing;                           //!< bones of the set with no node here
	int skinNodes = 0;                             //!< `*_skin` nodes the scene offered

	bool forced = false;                           //!< WW_BODY_BUILD is set: no QSettings, either way
	bool syncing = false;
	bool noteRefusal = false;
	QString noteDetail;

	// widgets
	QSplitter * split = nullptr;
	QScrollArea * scroll = nullptr;
	QComboBox * raceBox = nullptr;
	QComboBox * genderBox = nullptr;
	QDoubleSpinBox * thinBox = nullptr;
	QDoubleSpinBox * muscBox = nullptr;
	QDoubleSpinBox * fatBox = nullptr;
	BodyBuildTriangle * tri = nullptr;
	QLabel * note = nullptr;                       //!< pinned summary-or-refusal
	QWidget * actionBar = nullptr;                 //!< pinned
	QPushButton * btnPreview = nullptr;            //!< checkable
	QPushButton * btnReset = nullptr;
};

/*! WW_BODY_BUILD: this sub-lane's gates, run inside the real application
 *  (ww-test-harness-add). Does nothing unless WW_BODY_BUILD is set, and the
 *  variable's value is also what the panel forces its state to, so the gate
 *  and the panel cannot be measuring two different builds. */
void wwBodyBuildHarness( class NifSkope * skope );

#endif // BODYBUILDPANEL_H
