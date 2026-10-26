/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef OPENCELLDIALOG_H
#define OPENCELLDIALOG_H

#include "opencellcatalog.h"

#include <QDialog>

class QActionGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QPushButton;
class QTimer;
class QToolButton;
class QTreeWidget;

//! File > Open Cell...: pick plugins, a worldspace (or Interior) and a cell; writes a `.wwcell`.
//! lane CELLSPAWN1: the list reads as the CK's (Editor ID, name, grid, FormID, plugins), one search box matches
//! Editor ID, name or FormID, and a Filters menu in the block list's design narrows it.
class OpenCellDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit OpenCellDialog( QWidget * parent = nullptr );

	//! The written `.wwcell` after accept(); open it through the ordinary file path.
	QString specPath() const { return written; }

	// Driven by the unit test as well as by the widgets.
	void setDataDir( const QString & dir );
	void setLanguage( const QString & lang );         //!< the string-table language of localized plugins
	void setPluginsTicked( const QStringList & names );
	bool importLoadOrder( const QString & pluginsTxt );
	void readNow();                                   //!< read the catalog now instead of after the debounce
	QStringList loadOrderPaths() const;               //!< ticked plugins plus masters, as full paths
	QStringList worldItems() const;                   //!< the worldspace combo's entries, as shown
	QStringList cellItems() const;                    //!< the cell list's visible rows, visible columns tab-joined
	QStringList cellHeaders() const;                  //!< the visible column titles
	int cellCount() const;                            //!< rows listed after the search and the filters
	int cellTotal() const;                            //!< rows the selected worldspace holds before them
	void setSearch( const QString & text );           //!< the search box, applied at once (no debounce)
	void sortCells( int column, bool descending );    //!< as a header click; column -1 = the catalog's order
	void selectWorld( const QString & edid );         //!< an EDID, "Interior" or "All Cells"
	void selectCell( int x, int y, int n );
	void selectInterior( const QString & edid );
	QString specLine() const;                         //!< what Open writes, empty when nothing can be opened
	bool writeSpec( const QString & dir );            //!< write the spec into `dir`; specPath() names it

	static constexpr const char * interiorItem = "Interior";
	static constexpr const char * allItem = "All Cells";

	// Column numbers of the cell list (hidden where they mean nothing).
	enum Column { ColEditorId, ColName, ColX, ColY, ColForm, ColWorld, ColOwner, ColLastEdit, ColCount };
	// Search In, as bits.
	enum SearchField { SearchEditorId = 1, SearchName = 2, SearchFormId = 4 };

private:
	void build();
	void buildFilters();
	void browseData();
	void browseImport();
	void pluginsChanged();
	void fillWorlds();
	void fillPluginFilter();
	void fillCells();
	void fillCellsOld();
	void cellPicked();
	void openClicked();
	void saveSettings() const;
	void loadSettings();
	bool interiorMode() const;
	bool allMode() const;
	QString exteriorWorld() const;
	void refreshFilterButton();

	OpenCellSources sources;
	OpenCellCatalog catalog;
	QString written;

	// filter state (Filters menu)
	int searchFields = SearchEditorId | SearchName | SearchFormId;
	int cellsKind = 0;      //!< 0 all, 1 interior, 2 exterior
	int nameKind = 0;       //!< 0 any, 1 named, 2 unnamed
	bool pluginLastEdit = false;
	int pluginIndex = -1;   //!< -1 any, else an index into catalog.plugins
	int sortColumn = -1;
	bool sortDescending = false;
	int totalRows = 0;

	QLineEdit * dataEdit = nullptr;
	QComboBox * langCombo = nullptr;
	QListWidget * pluginList = nullptr;
	QComboBox * worldCombo = nullptr;
	QLineEdit * filterEdit = nullptr;
	QToolButton * filterButton = nullptr;
	QMenu * filterMenu = nullptr;
	QMenu * pluginMenu = nullptr;
	QActionGroup * pluginGroup = nullptr;
	QTreeWidget * cellTree = nullptr;
	QLabel * countLabel = nullptr;
	QLineEdit * xyEdit = nullptr;
	QComboBox * sizeCombo = nullptr;
	QLabel * status = nullptr;
	QPushButton * openButton = nullptr;
	QTimer * readTimer = nullptr;
	QTimer * searchTimer = nullptr;
};

//! lane CELLSPAWN1: WW_OPENCELL_SHOT=<png> harness: the dialog (and <png>.filters.png, its Filters menu) grabbed
//! with WW_OPENCELL_SHOT_DATA / _PLUGINS (comma list) / _WORLD / _SEARCH applied. Returns what it wrote, in words.
QString openCellShot( QWidget * parent, const QString & png );

#endif // OPENCELLDIALOG_H
