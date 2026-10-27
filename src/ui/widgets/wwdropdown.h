/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WWDROPDOWN_H
#define WWDROPDOWN_H

#include <QHash>
#include <QList>
#include <QPointer>
#include <QToolButton>
#include <QVariant>

class QActionGroup;
class QMenu;

/* lane OPENCELLFIX2 (bungo 10-27: "take the mode selector I linked as an example", "Without the icons though, just
 * text"): THE dropdown. The toolbar's Object Mode selector is the template: a boxed tool button that pops a menu of
 * exclusive checkable rows. applyStyle() is the one place that look is set; the toolbar's mode button calls it too.
 * The API is the part of QComboBox the dock used, so a combo swaps for it in place. */
class WwDropdown final : public QToolButton
{
	Q_OBJECT

public:
	explicit WwDropdown( QWidget * parent = nullptr );

	//! The Object Mode selector's look: instant popup, boxed, not auto-raised; stamps the "wwDropdown" property.
	static void applyStyle( QToolButton * button );
	//! True when `w` carries that look (the property and the boxed sheet).
	static bool hasStyle( const QWidget * w );

	int addItem( const QString & text, const QVariant & data = QVariant() );
	void addItems( const QStringList & texts );
	void clear();
	int count() const { return int( acts.size() ); }
	int currentIndex() const { return cur; }
	void setCurrentIndex( int i );
	QString currentText() const { return itemText( cur ); }
	QVariant currentData( int role = Qt::UserRole ) const { return itemData( cur, role ); }
	QString itemText( int i ) const;
	QVariant itemData( int i, int role = Qt::UserRole ) const;
	void setItemData( int i, const QVariant & value, int role = Qt::UserRole );
	void setItemEnabled( int i, bool on );
	int findData( const QVariant & value, int role = Qt::UserRole ) const;
	int findText( const QString & text ) const;
	QMenu * itemMenu() const { return popup; }
	//! Picks item `i` as a click in its menu would (currentIndexChanged, then activated).
	void pick( int i );
	//! Sits inline on a tick row (bungo 10-27 note 10): the same look, no vertical padding, as tall as `tick`.
	void setRowBuddy( QWidget * tick );
	//! Fits a narrow panel (bungo 10-27): may shrink below its longest item; the closed box elides, the list stays whole.
	void setElide( bool on );
	//! The text the closed box shows at its width (elided when narrower than the pick).
	QString shownText() const;

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent( QPaintEvent * e ) override;

signals:
	void currentIndexChanged( int index );
	void activated( int index );   //!< a pick from the menu (not a setCurrentIndex)

private:
	QMenu * popup = nullptr;
	QActionGroup * group = nullptr;
	QList<QAction *> acts;
	QList<QHash<int, QVariant>> roles;
	int cur = -1;
	int fitWidth = 0;
	bool elide = false;
	QPointer<QWidget> buddy;

	void refit();
};

#endif // WWDROPDOWN_H
