/* The options dialog shown by File > Export > .glTF (skeleton, skin, animation).
   Lane GLTFEXPORT1, 2026-09-19. bungo, 04:xx: "when we export gltf, we need a
   few toggles in there before the export."

   It is a flat Name | Value grid -- one field per row, label on the left,
   control on the right, NOTHING else. No descriptions, no blurbs, no help text
   (his standing ruling on settings menus). Every colour comes from
   wwSkinColor() and therefore from the PBR Material Editor's skinVars[] table;
   nothing here names a grey.

   It WRITES a GltfExportOptions and reads one back. It does not export: the
   caller does, through gltfExportCharacter(), which is the same call the CLI
   makes, so a dialog option and a command-line flag cannot drift. */

#ifndef GLTFEXPORTDIALOG_H
#define GLTFEXPORTDIALOG_H

#include "gltfexportopts.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QGridLayout;

class GltfExportDialog final : public QDialog
{
	Q_OBJECT

public:
	/*! @param nifPath   the open file, for the "auto" skeleton search and for
	 *                   the part list's starting folder.
	 *  @param haveClip  whether a clip is playing in the viewer; when false the
	 *                   clip row says so instead of offering a dead switch.
	 *  @param clipName  the clip's name, for the row's value.
	 */
	GltfExportDialog( const QString & nifPath, bool haveClip, const QString & clipName,
					  QWidget * parent = nullptr );

	//! The options the rows currently state.
	GltfExportOptions options() const;
	//! Force every row to a given state -- the harness path. NEVER reads
	//! QSettings, so a gate measures the code and not the machine
	//! (CONSTITUTION rule 6).
	void setOptions( const GltfExportOptions & o );

	// ---- readers for the harness
	int rowCount() const;            //!< labelled rows in the grid
	int controlCount() const;        //!< controls in the grid
	QStringList rowLabels() const;
	QString summaryText() const;     //!< the one summary line at the foot

private slots:
	void browseSkeleton();
	void addPart();
	void removePart();
	void refreshSummary();

private:
	void addRow( const QString & label, QWidget * control );

	QGridLayout * grid = nullptr;
	int nextRow = 0;
	QStringList labels;

	QComboBox * cbSkeleton = nullptr;
	QLineEdit * edSkeleton = nullptr;
	QComboBox * cbJoints = nullptr;
	QComboBox * cbUnits = nullptr;
	QListWidget * lwParts = nullptr;
	QComboBox * cbBones = nullptr;
	QComboBox * cbTextures = nullptr;
	QComboBox * cbRootMotion = nullptr;
	QCheckBox * ckClip = nullptr;
	QCheckBox * ckBuild = nullptr;
	QCheckBox * ckBuildCycle = nullptr;
	QLabel * lbSummary = nullptr;

	QString nifPath;
	bool haveClip = false;
	QString clipName;
};

//! WW_GLTF_EXPORT_DIALOG=<...> -- opens the dialog with a forced state and
//! writes release/ww_gltf_export_dialog_test.log. Declared here the way the
//! other panels declare theirs (src/animworkspace.h:413).
void wwGltfExportDialogHarness( class NifSkope * skope );

#endif // GLTFEXPORTDIALOG_H
