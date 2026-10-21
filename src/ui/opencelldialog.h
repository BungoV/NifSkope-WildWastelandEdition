/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef OPENCELLDIALOG_H
#define OPENCELLDIALOG_H

#include "opencellcatalog.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTimer;
class QTreeWidget;

//! File > Open Cell...: pick plugins, a worldspace (or Interior) and a cell; writes a `.wwcell`.
class OpenCellDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit OpenCellDialog( QWidget * parent = nullptr );

	//! The written `.wwcell` after accept(); open it through the ordinary file path.
	QString specPath() const { return written; }

	// Driven by the unit test as well as by the widgets.
	void setDataDir( const QString & dir );
	void setPluginsTicked( const QStringList & names );
	bool importLoadOrder( const QString & pluginsTxt );
	void readNow();                                   //!< read the catalog now instead of after the debounce
	QStringList loadOrderPaths() const;               //!< ticked plugins plus masters, as full paths
	QStringList worldItems() const;                   //!< the worldspace combo's entries, as shown
	QStringList cellItems() const;                    //!< the cell list's visible rows, columns tab-joined
	void selectWorld( const QString & edid );         //!< an EDID, or "Interior"
	void selectCell( int x, int y, int n );
	void selectInterior( const QString & edid );
	QString specLine() const;                         //!< what Open writes, empty when nothing can be opened
	bool writeSpec( const QString & dir );            //!< write the spec into `dir`; specPath() names it

	static constexpr const char * interiorItem = "Interior";

private:
	void build();
	void browseData();
	void browseImport();
	void pluginsChanged();
	void fillWorlds();
	void fillCells();
	void cellPicked();
	void openClicked();
	void saveSettings() const;
	void loadSettings();
	bool interiorMode() const;

	OpenCellSources sources;
	OpenCellCatalog catalog;
	QString written;

	QLineEdit * dataEdit = nullptr;
	QListWidget * pluginList = nullptr;
	QComboBox * worldCombo = nullptr;
	QLineEdit * filterEdit = nullptr;
	QTreeWidget * cellTree = nullptr;
	QLineEdit * xyEdit = nullptr;
	QComboBox * sizeCombo = nullptr;
	QLabel * status = nullptr;
	QPushButton * openButton = nullptr;
	QTimer * readTimer = nullptr;
};

#endif // OPENCELLDIALOG_H
