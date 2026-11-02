#ifndef CELLEDITPANEL_H
#define CELLEDITPANEL_H

/* lane ESPWRITE2 (2026-11-02): the Edits section of the Cell workspace's References tab (src/cellworkspace.cpp).
 * Base (XMarkerHeading first, or any EditorID), yaw, align, ground snap, persistent, Place (Shift+A in the view);
 * the edit list with Remove; the plugin file, new or Add to existing, the ESL flag; the action bar says what Save
 * will do or the one reason it cannot (skill nifskope-ww-panel-style). ESPWRITE3B: the picked reference (a click in
 * the view) gets Move (x y z), Turn 15 deg either way, Scale and Disable; its change joins the list. The session lives in src/celledit.h: nothing
 * here touches the cell until Save, which writes the plugin and re-opens the cell on the new load order. */

#include <QWidget>

class GLView;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class CellEditPanel final : public QWidget
{
	Q_OBJECT

public:
	explicit CellEditPanel( QWidget * parent = nullptr );
	void setGLView( GLView * v ) { view = v; }
	//! re-read the session into the widgets
	void refresh();

	// ---- readers for a harness
	QString summaryText() const;
	bool summaryIsRefusal() const { return refusal; }

private:
	void refreshSummary();
	void applyBase();
	//! ESPWRITE3B: the picked reference's row (its name, the move fields prefilled)
	void refreshPicked();
	quint32 pickedRef() const;

	GLView * view = nullptr;
	QComboBox * baseBox = nullptr;
	QLineEdit * edidEdit = nullptr;
	QDoubleSpinBox * yawSpin = nullptr;
	QComboBox * alignBox = nullptr;
	QCheckBox * snapBox = nullptr;
	QCheckBox * persistBox = nullptr;
	QPushButton * placeBtn = nullptr;
	QLabel * pickedLabel = nullptr;
	QDoubleSpinBox * moveSpin[3] = { nullptr, nullptr, nullptr };
	QPushButton * moveBtn = nullptr;
	QPushButton * turnLeftBtn = nullptr;
	QPushButton * turnRightBtn = nullptr;
	QDoubleSpinBox * scaleSpin = nullptr;
	QPushButton * scaleBtn = nullptr;
	QPushButton * disableBtn = nullptr;
	QListWidget * list = nullptr;
	QPushButton * removeBtn = nullptr;
	QLineEdit * pathEdit = nullptr;
	QComboBox * modeBox = nullptr;
	QCheckBox * eslBox = nullptr;
	QLabel * status = nullptr;
	QLabel * summary = nullptr;
	QPushButton * saveBtn = nullptr;
	bool refusal = true;
	bool syncing = false;
};

#endif // CELLEDITPANEL_H
