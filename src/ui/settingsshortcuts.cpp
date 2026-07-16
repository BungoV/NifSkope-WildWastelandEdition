#include "settingspane.h"

#include "ui/settingsdialog.h"
#include "nifskope.h"
#include "shortcutregistry.h"

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

//! @file settingsshortcuts.cpp SettingsShortcuts — rebindable shortcuts pane

// row data roles on column 0
enum {
	ScKindRole = Qt::UserRole,        // 0 = registry entry, 1 = QAction
	ScIdRole = Qt::UserRole + 1,      // registry id / action objectName
	ScDefaultRole = Qt::UserRole + 2  // default sequence (PortableText)
};

static QString scCleanText( QString s )
{
	s.remove( QLatin1Char( '&' ) );
	// menu items often carry embedded tab-shortcut hints
	const int tab = s.indexOf( QLatin1Char( '\t' ) );
	if ( tab >= 0 )
		s.truncate( tab );
	return s.trimmed();
}

SettingsShortcuts::SettingsShortcuts( QWidget * parent ) :
	SettingsPane( parent )
{
	QVBoxLayout * lay = new QVBoxLayout( this );

	QLabel * title = new QLabel( tr( "Shortcuts" ), this );
	QFont tf = title->font();
	tf.setPointSize( tf.pointSize() + 4 );
	title->setFont( tf );
	lay->addWidget( title );

	// Blender-style mouse mapping: which button selects, the other places
	// the gizmo / 3D cursor. Drags (orbit / zoom) keep their buttons.
	QHBoxLayout * mouseRow = new QHBoxLayout();
	mouseRow->addWidget( new QLabel( tr( "Select with mouse button:" ), this ) );
	mouseSelect = new QComboBox( this );
	mouseSelect->addItem( tr( "Left  (Right places the gizmo)" ) );
	mouseSelect->addItem( tr( "Right  (Left places the gizmo)" ) );
	mouseRow->addWidget( mouseSelect, 1 );
	lay->addLayout( mouseRow );
	connect( mouseSelect, &QComboBox::currentIndexChanged, this, [this]( int ) { modifyPane(); } );

	search = new QLineEdit( this );
	search->setPlaceholderText( tr( "Search shortcuts...  (name, category or key, e.g. \"extrude\" or \"Ctrl+R\")" ) );
	search->setClearButtonEnabled( true );
	lay->addWidget( search );

	tree = new QTreeWidget( this );
	tree->setColumnCount( 3 );
	tree->setHeaderLabels( { tr( "Action" ), tr( "Shortcut" ), QString() } );
	tree->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	tree->header()->setSectionResizeMode( 1, QHeaderView::Fixed );
	tree->header()->resizeSection( 1, 190 );
	tree->header()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
	tree->setRootIsDecorated( true );
	tree->setUniformRowHeights( true );
	lay->addWidget( tree, 1 );

	QLabel * hint = new QLabel(
		tr( "Click a shortcut field and press the new keys; duplicates within the same group turn red. "
		    "The reset button restores a row's default. Empty = unbound. Saved bindings apply immediately." ), this );
	hint->setWordWrap( true );
	lay->addWidget( hint );

	connect( search, &QLineEdit::textChanged, this, [this]( const QString & t ) { applyFilter( t ); } );

	SettingsDialog::registerPage( parent, tr( "Shortcuts" ) );
}

SettingsShortcuts::~SettingsShortcuts()
{
}

void SettingsShortcuts::showEvent( QShowEvent * e )
{
	QWidget::showEvent( e );
	// the pane is built before the main window's actions exist, so populate
	// lazily; keep unsaved edits when the user just switches pages
	if ( !populated && !isModified() )
		read();
}

void SettingsShortcuts::addRow( QTreeWidgetItem * parent, int kind, const QString & id,
	const QString & label, const QKeySequence & cur, const QKeySequence & def )
{
	QTreeWidgetItem * item = new QTreeWidgetItem( parent );
	item->setText( 0, label );
	item->setData( 0, ScKindRole, kind );
	item->setData( 0, ScIdRole, id );
	item->setData( 0, ScDefaultRole, def.toString( QKeySequence::PortableText ) );
	item->setToolTip( 0, tr( "%1\nDefault: %2" ).arg( id,
		def.isEmpty() ? tr( "(none)" ) : def.toString( QKeySequence::NativeText ) ) );

	QKeySequenceEdit * edit = new QKeySequenceEdit( cur, tree );
#if QT_VERSION >= QT_VERSION_CHECK( 6, 4, 0 )
	edit->setClearButtonEnabled( true );
#endif
#if QT_VERSION >= QT_VERSION_CHECK( 6, 5, 0 )
	edit->setMaximumSequenceLength( 1 );
#endif
	tree->setItemWidget( item, 1, edit );
	connect( edit, &QKeySequenceEdit::keySequenceChanged, this, [this]( const QKeySequence & ) {
		modifyPane();
		updateConflicts();
	} );

	QToolButton * reset = new QToolButton( tree );
	reset->setText( QString( QChar( 0x21BA ) ) );	// anticlockwise open circle arrow
	reset->setAutoRaise( true );
	reset->setToolTip( tr( "Restore the default shortcut" ) );
	tree->setItemWidget( item, 2, reset );
	connect( reset, &QToolButton::clicked, this, [edit, def]() {
		edit->setKeySequence( def );
	} );
}

void SettingsShortcuts::read()
{
	{
		QSettings settings;
		QSignalBlocker blocker( mouseSelect );
		mouseSelect->setCurrentIndex(
			settings.value( QStringLiteral( "Shortcuts/MouseSelect" ) ).toString()
				== QLatin1String( "right" ) ? 1 : 0 );
	}

	tree->clear();
	auto & reg = ShortcutRegistry::get();

	// built-in viewport bindings, grouped by their registry category
	QMap<QString, QTreeWidgetItem *> groups;
	auto groupItem = [this, &groups]( const QString & cat ) {
		auto it = groups.find( cat );
		if ( it != groups.end() )
			return it.value();
		QTreeWidgetItem * g = new QTreeWidgetItem( tree );
		g->setText( 0, cat );
		g->setFlags( g->flags() & ~Qt::ItemIsSelectable );
		QFont f = g->font( 0 );
		f.setBold( true );
		g->setFont( 0, f );
		g->setExpanded( true );
		groups.insert( cat, g );
		return g;
	};

	const auto & entries = reg.entries();
	for ( auto it = entries.constBegin(); it != entries.constEnd(); ++it )
		addRow( groupItem( it->category ), 0, it->id, it->label, it->cur, it->def );

	// every QAction of the main window that is currently bound (or was, before
	// an override unbound it), grouped by the menu it lives in
	NifSkope * win = nullptr;
	for ( QWidget * w : QApplication::topLevelWidgets() ) {
		if ( ( win = qobject_cast<NifSkope *>( w ) ) )
			break;
	}
	if ( win ) {
		// map every menu action to its top-level menu title
		QMap<QAction *, QString> menuOf;
		if ( QMenuBar * mb = win->menuBar() ) {
			const auto tops = mb->actions();
			for ( QAction * top : tops ) {
				if ( !top->menu() )
					continue;
				const QString title = scCleanText( top->menu()->title() );
				QList<QMenu *> stack{ top->menu() };
				while ( !stack.isEmpty() ) {
					QMenu * m = stack.takeLast();
					const auto acts = m->actions();
					for ( QAction * a : acts ) {
						menuOf.insert( a, title );
						if ( a->menu() )
							stack.append( a->menu() );
					}
				}
			}
		}
		QSet<QString> seen;
		const auto acts = win->findChildren<QAction *>();
		for ( QAction * a : acts ) {
			const QString name = a->objectName();
			if ( name.isEmpty() || seen.contains( name ) )
				continue;
			const QKeySequence def = reg.actionDefault( name ).isEmpty()
				? a->shortcut() : reg.actionDefault( name );
			if ( a->shortcut().isEmpty() && def.isEmpty() )
				continue;	// never bound: nothing to rebind
			QString label = scCleanText( a->text() );
			if ( label.isEmpty() )
				continue;
			seen.insert( name );
			const QString cat = menuOf.contains( a )
				? tr( "Menu: %1" ).arg( menuOf.value( a ) ) : tr( "Application" );
			addRow( groupItem( cat ), 1, name, label, a->shortcut(), def );
		}
	}

	populated = true;
	updateConflicts();
	applyFilter( search->text() );
	setModified( false );
}

void SettingsShortcuts::write()
{
	if ( !populated )
		return;
	auto & reg = ShortcutRegistry::get();
	QSettings settings;
	settings.setValue( QStringLiteral( "Shortcuts/MouseSelect" ),
		mouseSelect->currentIndex() == 1 ? QLatin1String( "right" ) : QLatin1String( "left" ) );
	for ( int g = 0; g < tree->topLevelItemCount(); g++ ) {
		QTreeWidgetItem * group = tree->topLevelItem( g );
		for ( int i = 0; i < group->childCount(); i++ ) {
			QTreeWidgetItem * item = group->child( i );
			auto edit = qobject_cast<QKeySequenceEdit *>( tree->itemWidget( item, 1 ) );
			if ( !edit )
				continue;
			QKeySequence ks = edit->keySequence();
			if ( ks.count() > 1 )
				ks = QKeySequence( ks[0] );	// one chord per binding
			const QString id = item->data( 0, ScIdRole ).toString();
			if ( item->data( 0, ScKindRole ).toInt() == 0 ) {
				reg.setSeq( id, ks );
			} else {
				const QKeySequence def = QKeySequence::fromString(
					item->data( 0, ScDefaultRole ).toString(), QKeySequence::PortableText );
				const QString key = ShortcutRegistry::actionSettingsKey( id );
				if ( ks == def )
					settings.remove( key );
				else
					settings.setValue( key, ks.toString( QKeySequence::PortableText ) );
			}
		}
	}
	// NifSkope::applyShortcutOverrides() runs on the same saveSettings signal
	// (connected after the panes), so QAction changes land right away
	setModified( false );
}

void SettingsShortcuts::setDefault()
{
	if ( !populated )
		read();
	mouseSelect->setCurrentIndex( 0 );
	for ( int g = 0; g < tree->topLevelItemCount(); g++ ) {
		QTreeWidgetItem * group = tree->topLevelItem( g );
		for ( int i = 0; i < group->childCount(); i++ ) {
			QTreeWidgetItem * item = group->child( i );
			auto edit = qobject_cast<QKeySequenceEdit *>( tree->itemWidget( item, 1 ) );
			if ( !edit )
				continue;
			edit->setKeySequence( QKeySequence::fromString(
				item->data( 0, ScDefaultRole ).toString(), QKeySequence::PortableText ) );
		}
	}
}

void SettingsShortcuts::updateConflicts()
{
	// duplicates within the same top-level group are conflicts (a viewport key
	// and a menu action can intentionally share a key: different scope)
	for ( int g = 0; g < tree->topLevelItemCount(); g++ ) {
		QTreeWidgetItem * group = tree->topLevelItem( g );
		QHash<QString, int> counts;
		for ( int i = 0; i < group->childCount(); i++ ) {
			auto edit = qobject_cast<QKeySequenceEdit *>( tree->itemWidget( group->child( i ), 1 ) );
			if ( !edit || edit->keySequence().isEmpty() )
				continue;
			counts[edit->keySequence().toString( QKeySequence::PortableText )]++;
		}
		for ( int i = 0; i < group->childCount(); i++ ) {
			QTreeWidgetItem * item = group->child( i );
			auto edit = qobject_cast<QKeySequenceEdit *>( tree->itemWidget( item, 1 ) );
			const bool dup = edit && !edit->keySequence().isEmpty()
				&& counts.value( edit->keySequence().toString( QKeySequence::PortableText ) ) > 1;
			item->setForeground( 0, dup ? QBrush( QColor( 220, 80, 80 ) ) : QBrush() );
			item->setToolTip( 1, dup ? tr( "This key is bound more than once in this group" ) : QString() );
		}
	}
}

void SettingsShortcuts::applyFilter( const QString & text )
{
	const QString t = text.trimmed();
	for ( int g = 0; g < tree->topLevelItemCount(); g++ ) {
		QTreeWidgetItem * group = tree->topLevelItem( g );
		int visible = 0;
		for ( int i = 0; i < group->childCount(); i++ ) {
			QTreeWidgetItem * item = group->child( i );
			bool show = t.isEmpty();
			if ( !show ) {
				auto edit = qobject_cast<QKeySequenceEdit *>( tree->itemWidget( item, 1 ) );
				const QString seq = edit ? edit->keySequence().toString( QKeySequence::NativeText ) : QString();
				show = item->text( 0 ).contains( t, Qt::CaseInsensitive )
					|| group->text( 0 ).contains( t, Qt::CaseInsensitive )
					|| seq.contains( t, Qt::CaseInsensitive )
					|| item->data( 0, ScIdRole ).toString().contains( t, Qt::CaseInsensitive );
			}
			item->setHidden( !show );
			if ( show )
				visible++;
		}
		group->setHidden( visible == 0 );
		if ( !t.isEmpty() && visible > 0 )
			group->setExpanded( true );
	}
}
