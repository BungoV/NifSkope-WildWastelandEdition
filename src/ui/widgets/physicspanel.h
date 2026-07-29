/*! The Physics Sim controls, as one widget with two sizes.
 *
 * There are two places these belong and they want different amounts of it. The
 * toolbar dropdown wants what you touch every few seconds -- run, tool, playback
 * -- and has to fit without scrolling, because a dropdown that scrolls is a dock
 * with extra steps. The Collision Manager wants the lot: the world, the solver,
 * the body list, recording and capture, all staying open while you work.
 *
 * One class rather than two, because the alternative is two implementations of
 * the same controls that disagree the first time one of them changes. The mode
 * selects which SECTIONS are laid out; every widget is built either way, so the
 * one sync path can refresh all of them without asking which host it is in.
 */

#ifndef PHYSICSPANEL_H
#define PHYSICSPANEL_H

#include <QWidget>

#include <functional>

class GLView;
class NifModel;
class QAction;

class PhysicsSimPanel final : public QWidget
{
	Q_OBJECT

public:
	enum class Mode
	{
		//! Run/stop, tool, tool parameters, playback, presets. Fits a dropdown.
		Essentials,
		//! Everything, for a dock that stays open beside the viewport.
		Full
	};

	/*! `showAction` is the existing View > Show Collision action, driven rather
	 * than duplicated -- a second switch of its own would let the two disagree
	 * about whether collision is being drawn. May be null.
	 */
	PhysicsSimPanel( GLView * ogl, NifModel * nif, Mode mode,
		QAction * showAction = nullptr, QWidget * parent = nullptr );

	//! Push the preview's state into the widgets. The keyboard shortcuts change
	//! the same state, so this is called whenever the panel becomes visible
	//! rather than trusting what it last wrote.
	void sync();

	Mode mode() const { return m_mode; }

signals:
	//! Essentials mode only: the user asked for the full set of controls.
	void openManagerRequested();

private:
	GLView * m_ogl = nullptr;
	NifModel * m_nif = nullptr;
	Mode m_mode = Mode::Essentials;
	QAction * m_showAction = nullptr;
	/*! The refresh, as a closure over the widgets it refreshes.
	 *
	 * Fifty-odd controls would otherwise all have to become members for one
	 * function to reach them, which is fifty declarations that say nothing and
	 * fifty chances for one to go stale. The widgets are owned by Qt parenting
	 * either way; this only decides who can name them.
	 */
	std::function<void()> m_sync;
};

#endif // PHYSICSPANEL_H
