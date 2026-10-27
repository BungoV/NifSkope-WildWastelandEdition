/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef OPENCELLDIALOG_H
#define OPENCELLDIALOG_H

#include "opencellcatalog.h"

#include <QWidget>

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

/*! The Cell dock's first two tabs (lane OPENCELLFIX1; before it, the File > Open Cell... dialog).
 *  Load order: the Data folder, the load-order file (an MO2 profile's plugins.txt or the game's), the language and the
 *  plugin list. Cells: worldspace, search, the CK-style cell list, X, Y, size and Open.
 *  lane CELLSPAWN1: the list reads as the CK's (Editor ID, name, grid, FormID, plugins), one search box matches
 *  Editor ID, name or FormID, and a Filters menu in the block list's design narrows it.
 *  Every choice is saved as it is made and restored on the next start; a load order whose saved folders are gone
 *  falls back to the LOD panel's MO2 profile (openCellFallback). */
class OpenCellPicker final : public QWidget
{
	Q_OBJECT

public:
	explicit OpenCellPicker( QWidget * parent = nullptr );

	//! The two pages, laid out one above the other until a host (the Cell dock) takes them into its tabs.
	QWidget * loadOrderPage() const { return loPage; }
	QWidget * cellsPage() const { return cellPage; }

	//! The written cell file after Open; open it through the ordinary file path.
	QString specPath() const { return written; }

	// Driven by the unit test as well as by the widgets.
	void setDataDir( const QString & dir );
	void setLanguage( const QString & lang );         //!< the string-table language of localized plugins
	void setPluginsTicked( const QStringList & names );
	bool importLoadOrder( const QString & pluginsTxt );
	void readNow();                                   //!< read the catalog now instead of after the debounce
	QStringList loadOrderPaths() const;               //!< ticked plugins plus masters, as full paths
	QStringList tickedPlugins() const;                //!< the ticked rows, in list order
	QStringList missingMasters() const;               //!< masters the ticked plugins need that no folder holds
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
	QString refusal() const;                          //!< why specLine() is empty, in words
	bool writeSpec( const QString & dir );            //!< write the cell file into `dir`; specPath() names it
	QString statusText() const;
	const OpenCellSources & loadOrderSources() const { return sources; }
	QString loadOrderFile() const { return orderFile; }

	static constexpr const char * interiorItem = "Interior";
	static constexpr const char * allItem = "All Cells";

	// Column numbers of the cell list (hidden where they mean nothing).
	enum Column { ColEditorId, ColName, ColX, ColY, ColForm, ColWorld, ColOwner, ColLastEdit, ColCount };
	// Search In, as bits.
	enum SearchField { SearchEditorId = 1, SearchName = 2, SearchFormId = 4 };

signals:
	//! Open was pressed (or a cell double-clicked) and the cell file is written.
	void openRequested( const QString & specPath );

protected:
	bool eventFilter( QObject * o, QEvent * e ) override;

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
	void setStatus( const QString & text );
	void refreshCount();
	bool interiorMode() const;
	bool allMode() const;
	QString exteriorWorld() const;
	void refreshFilterButton();

	OpenCellSources sources;
	OpenCellCatalog catalog;
	QString written;
	QString orderFile;          //!< the imported load-order file, empty when the list was ticked by hand
	QString pendingWorld, pendingCell;  //!< restored before the catalog is read; applied once it is
	bool restoring = false;
	bool readOwed = false;      //!< the list changed since the last read

	// filter state (Filters menu)
	int searchFields = SearchEditorId | SearchName | SearchFormId;
	int cellsKind = 0;      //!< 0 all, 1 interior, 2 exterior
	int nameKind = 0;       //!< 0 any, 1 named, 2 unnamed
	bool pluginLastEdit = false;
	int pluginIndex = -1;   //!< -1 any, else an index into catalog.plugins
	int sortColumn = -1;
	bool sortDescending = false;
	int totalRows = 0;

	QWidget * loPage = nullptr;
	QWidget * cellPage = nullptr;
	QLineEdit * dataEdit = nullptr;
	QLineEdit * orderEdit = nullptr;
	QComboBox * langCombo = nullptr;
	QListWidget * pluginList = nullptr;
	QLabel * pluginCount = nullptr;
	QLabel * loStatus = nullptr;
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
	QTimer * dataTimer = nullptr;
};

/*! Where a load order comes from when the saved one is gone (or none was ever saved): the LOD panel's MO2 profile,
 *  else an MO2 instance on this machine, else the game's Data folder and its plugins.txt (cellModSource()).
 *  Defined in src/ui/opencellfallback.cpp; the unit test supplies its own. */
struct OpenCellFallback
{
	QString dataDir;
	QString pluginsTxt;
};
OpenCellFallback openCellFallback();

//! lane CELLSPAWN1: WW_OPENCELL_SHOT=<png> harness: the two pages (and <png>.filters.png, the Filters menu) grabbed
//! with WW_OPENCELL_SHOT_DATA / _PLUGINS (comma list) / _WORLD / _SEARCH applied. Returns what it wrote, in words.
QString openCellShot( QWidget * parent, const QString & png );

#endif // OPENCELLDIALOG_H
