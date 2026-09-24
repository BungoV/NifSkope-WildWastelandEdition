/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_SCENEWINDOW_H
#define WW_SCENEWINDOW_H

/* The Scene window (lane PBRR2A; bungo's UI ruling 2026-09-24 01:2x/01:3x).
 *
 * ONE non-modal tool window for everything scene-wide: separate from the main
 * window, movable to any monitor, above NifSkope only (Qt::Tool with the main
 * window as parent), opened and closed from View > Scene and the viewport
 * toolbar's Scene button, remembering its size and position.
 *
 * A flat Name|Value tree in the skinVars palette, label + control only. The
 * sections are Mode / Weather / Sky / Ground / Fog / Effects; R2a makes the Mode
 * rows real (Lighting, Exposure, View Transform, PBR Route View) and leaves the
 * others as disabled rows for the stages that fill them. Every enabled row
 * applies live.
 *
 * No Q_OBJECT: it declares no signals or slots. Find it by object name
 * ("SceneWindow"), never by qobject_cast. */

#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QTimer;
class QTreeWidget;

class SceneWindow final : public QWidget
{
public:
	//! repaint: redraw the viewport; visibility: the window was shown (true) or hidden
	SceneWindow( QWidget * mainWindow, std::function<void()> repaint, std::function<void( bool )> visibility );

	QTreeWidget * tree = nullptr;
	QComboBox * modeBox = nullptr;
	QDoubleSpinBox * exposureBox = nullptr;
	QComboBox * viewBox = nullptr;
	QCheckBox * routeViewBox = nullptr;
	// Weather (W1) and Ground rows (lane PBRR2B)
	QComboBox * pluginBox = nullptr;
	QComboBox * weatherBox = nullptr;
	QDoubleSpinBox * hourBox = nullptr;
	QCheckBox * groundBox = nullptr;
	QLabel * statusLabel = nullptr;
	// the weather preview rows (lane PBRWX1)
	QCheckBox * skyBox = nullptr;
	QCheckBox * cloudsBox = nullptr;
	QCheckBox * sunBox = nullptr;
	QCheckBox * moonBox = nullptr;
	QDoubleSpinBox * gameDayBox = nullptr;
	// the weather fog row (lane FOG1)
	QCheckBox * fogBox = nullptr;
	// the cascaded sun shadows row (lane CSM1)
	QCheckBox * shadowsBox = nullptr;

	//! re-read the Data folder's plugins / the loaded weathers / the status line
	void refreshPlugins();
	void refreshLookdev();
	void refreshStatus();

	//! write the geometry to the settings now (also done on hide and after a move)
	void saveGeometryNow();
	static const char * geometryKey();

protected:
	void showEvent( QShowEvent * e ) override;
	void hideEvent( QHideEvent * e ) override;
	void moveEvent( QMoveEvent * e ) override;
	void resizeEvent( QResizeEvent * e ) override;

private:
	void syncEnabled();
	//! the cloud scroll: repaint at 10 Hz while Lookdev and Clouds are on (never in a harness run)
	void syncCloudTimer();

	std::function<void()> m_repaint;
	std::function<void( bool )> m_visibility;
	QTimer * m_saveTimer = nullptr;
	QTimer * m_cloudTimer = nullptr;
	bool m_placed = false;
};

#endif
