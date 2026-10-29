#ifndef AIRZONEPANEL_H
#define AIRZONEPANEL_H

/* lane AIRZONE1 (bungo 10-29): the Air zones band of the Cell workspace's Display tab (src/cellworkspace.cpp, under
 * PRTP). Pick a cell of the open view, its hand-placed volumes in a list, the selected one's rows (label | field, one
 * setting a row, the house style: skill nifskope-ww-panel-style): shape, location, size, rotation, edge, priority, and
 * the gi, fog and particles blocks. Every change is live in the fog and the lit particles (the panel's working copy,
 * src/airzone.h wwAirZoneEdit) before it is saved; Save writes the cell's .recx (its own file when it came from one,
 * else Save to/Plugins/<Provider>/Cells/<stem>.recx); Revert drops the working copy. Export GI shapes writes today's
 * GI volumes of every cell into its file (the converter). To 3D cursor puts the volume's centre on the viewport's 3D
 * cursor (a right click on a surface).
 *
 * Not done here (rows owed, drop/HANDOFF.md): a drag gizmo in the viewport (GLView's G/R/S gizmo works on NIF blocks;
 * a volume is not one), and an Add that drops the new volume where the camera looks. */

#include "airzonemath.h"

#include <QWidget>

#include <functional>
#include <vector>

class GLView;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class WwNumberField;

class AirZonePanel final : public QWidget
{
	Q_OBJECT

public:
	explicit AirZonePanel( QWidget * parent = nullptr );

	//! the open cell document (null: none); a new one drops the working copy
	void setNif( const void * nif );
	void setGLView( GLView * view );
	//! re-read the cells and their volumes (a cell opened, a file changed, the row ticked)
	void refresh();

	// ---- readers for a harness (object names cover the widgets)
	int cellRows() const;
	int volumeRows() const;
	QString noteText() const;
	bool noteIsRefusal() const { return refusal; }

protected:
	//! the overlay draws while the band shows (the Display tab), and stops when it hides
	void showEvent( QShowEvent * e ) override;
	void hideEvent( QHideEvent * e ) override;

private:
	struct Num
	{
		WwNumberField * field = nullptr;
		QLabel * label = nullptr;
		int block = 0;	// 0 the volume, 1 gi, 2 fog, 3 particles
		std::function<double( const az::Volume & )> get;
		std::function<void( az::Volume &, double )> set;
	};

	void buildUi();
	void loadCell( int index );
	void showVolume( int index );
	void rebuildList();
	void fromFields();
	void pushLive();
	void greyBlocks();
	void say( const QString & text, bool isRefusal );

	const void * nif = nullptr;
	GLView * glView = nullptr;
	bool syncing = false;
	bool refusal = false;
	bool dirty = false;
	int cell = -1, vol = -1;
	quint64 seenVersion = 0;	// the files' version the list shows (a file changed outside: the list again)
	std::vector<az::Volume> work;
	QString giMode;

	QCheckBox * showBox = nullptr;
	QComboBox * cellBox = nullptr;
	QTreeWidget * list = nullptr;
	QPushButton * addBtn = nullptr, * dupBtn = nullptr, * delBtn = nullptr;
	QLineEdit * nameEdit = nullptr;
	QComboBox * shapeBox = nullptr;
	QComboBox * giModeBox = nullptr;
	QCheckBox * giOn = nullptr, * fogOn = nullptr, * partOn = nullptr;
	QComboBox * giKind = nullptr;
	std::vector<Num> nums;
	QLineEdit * saveRoot = nullptr, * provider = nullptr;
	QPushButton * cursorBtn = nullptr, * revertBtn = nullptr, * saveBtn = nullptr, * exportBtn = nullptr;
	QLabel * note = nullptr;
};

#endif // AIRZONEPANEL_H
