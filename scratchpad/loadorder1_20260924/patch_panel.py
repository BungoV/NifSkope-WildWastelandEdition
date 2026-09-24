"""LOADORDER1 panel leg: the Source row's third choice, Mod Organizer 2 profile (off disk).
Asserts each anchor once and the CR count unchanged (lodgenmanager.cpp is LF-only)."""
import sys
P = 'E:/Projects/NifskopeWWE-loadorder1/src/lodgenmanager.cpp'
s = open(P, encoding='utf-8', newline='').read()
cr0 = s.count('\r')
H = []

H.append(('#include "lodgenlayout.h"\n', '#include "lodgenlayout.h"\n#include "lodgenloadorder.h"\n'))

H.append((r'''		sourceBox->addItem( tr( "Mod Organizer 2" ), 1 );
		sourceBox->setCurrentIndex( settings.value( QStringLiteral( "LodGeneration/source" ), 0 ).toInt() == 1 ? 1 : 0 );
''', r'''		sourceBox->addItem( tr( "Mod Organizer 2" ), 1 );
		sourceBox->addItem( tr( "Mod Organizer 2 profile" ), 2 );
		{
			const int saved = settings.value( QStringLiteral( "LodGeneration/source" ), 0 ).toInt();
			sourceBox->setCurrentIndex( ( saved == 1 || saved == 2 ) ? saved : 0 );
		}
'''))

H.append((r'''			"NifSkope to Mod Organizer's executable list, the way FO4Edit is added." ) );
		wwMatchFieldStyle( sourceBox );
		src.add( page, tr( "Source" ), sourceBox );
''', r'''			"NifSkope to Mod Organizer's executable list, the way FO4Edit is added.\n"
			"Mod Organizer 2 profile reads a profile's modlist.txt and plugins.txt off\n"
			"disk, with no Mod Organizer running: each plugin from its own mod folder, the\n"
			"enabled mods stacked in its priority order, disabled mods left out." ) );
		wwMatchFieldStyle( sourceBox );
		src.add( page, tr( "Source" ), sourceBox );

		/* THE PROFILE, OFF DISK (lane LOADORDER1, 2026-09-24). The same reader
		 * as the command line's --mo2-profile / --mo2-mods
		 * (lodgenLoadOrderFromMo2), so the panel and a scripted bake resolve one
		 * load order the same way. Shown only under that source. */
		mo2ProfileEdit = new QLineEdit( page );
		mo2ProfileEdit->setObjectName( QStringLiteral( "LodgenMo2ProfileEdit" ) );
		{
			QString p = settings.value( QStringLiteral( "LodGeneration/mo2Profile" ) ).toString();
			if ( p.isEmpty() && QFileInfo( QStringLiteral( "E:/Projects/Fallout 4 Mods/profiles/Default/modlist.txt" ) ).isFile() )
				p = QStringLiteral( "E:/Projects/Fallout 4 Mods/profiles/Default" );
			mo2ProfileEdit->setText( p );
		}
		mo2ProfileEdit->setPlaceholderText( tr( "the profile folder that holds modlist.txt" ) );
		mo2ProfileEdit->setToolTip( tr( "A Mod Organizer 2 profile folder (profiles\\<name>). Its modlist.txt orders\n"
			"the mods, the top line winning, and its plugins.txt lists the enabled plugins.\n"
			"Command line: --mo2-profile" ) );
		mo2ProfileHost = browseHost( mo2ProfileEdit, tr( "Mod Organizer 2 profile folder" ) );
		mo2ProfileLabel = src.add( page, tr( "Profile" ), mo2ProfileHost );
		mo2ModsEdit = new QLineEdit( settings.value( QStringLiteral( "LodGeneration/mo2Mods" ) ).toString(), page );
		mo2ModsEdit->setObjectName( QStringLiteral( "LodgenMo2ModsEdit" ) );
		mo2ModsEdit->setPlaceholderText( tr( "the instance's mods folder" ) );
		mo2ModsEdit->setToolTip( tr( "The Mod Organizer 2 instance's mods folder. Empty = the mods folder two\n"
			"levels above the profile, where Mod Organizer keeps it.\n"
			"Command line: --mo2-mods" ) );
		mo2ModsHost = browseHost( mo2ModsEdit, tr( "Mod Organizer 2 mods folder" ) );
		mo2ModsLabel = src.add( page, tr( "Mods folder" ), mo2ModsHost );
		for ( QLineEdit * e : { mo2ProfileEdit, mo2ModsEdit } )
			connect( e, &QLineEdit::textChanged, this, [this]( const QString & ) {
				if ( mo2DiskMode() )
					applySource( false );
			} );
'''))

H.append((r'''		resourceLabel = src.add( page, tr( "Resources" ), resourceHost, Qt::AlignTop );
''', r'''		resourceLabel = src.add( page, tr( "Resources" ), resourceHost, Qt::AlignTop );

		//! Mod Organizer 2 profile: the resolved stack, read only, lowest first
		mo2StackList = new QListWidget( page );
		mo2StackList->setObjectName( QStringLiteral( "LodgenMo2StackList" ) );
		mo2StackList->setMaximumHeight( 84 );
		mo2StackList->setSelectionMode( QAbstractItemView::NoSelection );
		mo2StackList->setToolTip( tr( "Where the meshes, textures and materials come from, resolved from the\n"
			"profile: the game's Data folder first, then each enabled mod from the bottom\n"
			"of the mod list to the top, then overwrite. The LAST row overrides the ones\n"
			"above it and a loose file beats an archive. Disabled mods are not in it." ) );
		mo2StackLabel = src.add( page, tr( "Mod order" ), mo2StackList, Qt::AlignTop );
'''))

H.append(('''			if ( mo2Mode() )
				applySource( false );
''', '''			if ( mo2Mode() || mo2DiskMode() )
				applySource( false );
'''))

H.append(('''	bool mo2Mode() const { return sourceBox->currentData().toInt() == 1; }
''', '''	bool mo2Mode() const { return sourceBox->currentData().toInt() == 1; }
	//! Mod Organizer 2 profile: the profile read off disk (lane LOADORDER1)
	bool mo2DiskMode() const { return sourceBox->currentData().toInt() == 2; }
'''))

H.append(('''		if ( mo2Mode() )
			return lodgenMo2Stack( gameDataDir(), mo2Plugins );
''', '''		if ( mo2DiskMode() )
			return mo2DiskStack;
		if ( mo2Mode() )
			return lodgenMo2Stack( gameDataDir(), mo2Plugins );
'''))

H.append(('''	void applySource( bool userChose )
	{
		const bool mo2 = mo2Mode();
		resourceHost->setVisible( !mo2 );
		resourceLabel->setVisible( !mo2 );
''', '''	void applySource( bool userChose )
	{
		const bool disk = mo2DiskMode();
		const bool mo2 = mo2Mode() || disk;
		resourceHost->setVisible( !mo2 );
		resourceLabel->setVisible( !mo2 );
		for ( QWidget * w : { static_cast<QWidget *>( mo2ProfileHost ), static_cast<QWidget *>( mo2ProfileLabel ),
				static_cast<QWidget *>( mo2ModsHost ), static_cast<QWidget *>( mo2ModsLabel ),
				static_cast<QWidget *>( mo2StackList ), static_cast<QWidget *>( mo2StackLabel ) } )
			w->setVisible( disk );
		if ( !disk ) {
			mo2DiskStack.clear();
			mo2DiskError.clear();
			mo2StackList->clear();
		}
'''))

H.append(('''		sourceStatus->setVisible( true );
		if ( !lodgenUnderMo2() ) {
''', '''		sourceStatus->setVisible( true );
		if ( disk ) {
			applyMo2Disk();
			return;
		}
		if ( !lodgenUnderMo2() ) {
'''))

H.append(('''	//! the one sentence the status line and the refusal both say
	static QString mo2Refusal()
''', r'''	/*! Mod Organizer 2 profile: read the profile off disk and show what it
	 *  resolved -- every plugin as a full path in load order, the mod order as
	 *  the stack, and one line saying what was read. A refusal names its cause
	 *  on the status line and Generate carries the same words. */
	void applyMo2Disk()
	{
		mo2Plugins.clear();
		mo2DiskStack.clear();
		mo2DiskError.clear();
		mo2StackList->clear();
		const QString profile = mo2ProfileEdit->text().trimmed();
		const QString mods = mo2ModsEdit->text().trimmed();
		LodgenLoadOrder lo;
		QString err;
		bool ok = false;
		if ( profile.isEmpty() ) {
			err = tr( "choose a profile folder" );
		} else {
			/* The game's Data folder: the one Mod Organizer's own ini names for
			 * this instance, else the game folder Settings > Resources knows. */
			const QString instance = mods.isEmpty()
				? QDir::cleanPath( profile + QStringLiteral( "/../.." ) )
				: QDir::cleanPath( mods + QStringLiteral( "/.." ) );
			QString from;
			QString data = lodgenMo2GameData( instance, &from );
			if ( data.isEmpty() )
				data = gameDataDir();
			ok = lodgenLoadOrderFromMo2( profile, mods, data, &lo, &err );
		}
		pluginList->clear();
		if ( !ok ) {
			mo2DiskError = tr( "Mod Organizer 2 profile: %1" ).arg( err );
			sourceStatus->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "danger" ) ) );
			sourceStatus->setText( mo2DiskError );
			scheduleWorldspaceRefresh();
			refreshSummary();
			return;
		}
		mo2DiskStack = lo.stack;
		int fromMods = 0;
		for ( int i = 0; i < lo.plugins.size(); i++ ) {
			pluginList->addItem( lo.plugins.at( i ) );
			pluginList->item( i )->setToolTip( lo.pluginFrom.value( i ) );
			if ( i >= lo.masters && lo.pluginFrom.value( i ) != QLatin1String( "data" ) )
				fromMods++;
		}
		for ( int i = 0; i < lo.stack.size(); i++ ) {
			const QString from = lo.stackFrom.value( i );
			QString text = from;
			if ( from == QLatin1String( "data" ) )
				text = tr( "game Data" );
			else if ( from.startsWith( QLatin1String( "mod " ) ) )
				text = from.mid( 4 );
			auto * it = new QListWidgetItem( text, mo2StackList );
			it->setToolTip( lo.stack.at( i ) );
		}
		sourceStatus->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
		QString line = tr( "Mod Organizer 2 profile: %1 plugins (%2 from mod folders), %3 mods enabled, "
			"%4 disabled, files from %5" )
			.arg( lo.plugins.size() ).arg( fromMods ).arg( lo.modsEnabled ).arg( lo.modsDisabled ).arg( lo.dataDir );
		if ( !lo.modsMissing.isEmpty() )
			line += tr( "; %1 enabled mod folder(s) missing" ).arg( lo.modsMissing.size() );
		sourceStatus->setText( line );
		scheduleWorldspaceRefresh();
		refreshSummary();
	}

	//! the one sentence the status line and the refusal both say
	static QString mo2Refusal()
'''))

H.append(('''			why = mo2Refusal();
		else if ( !wsBox->currentData().toUInt() )
''', '''			why = mo2Refusal();
		else if ( mo2DiskMode() && !mo2DiskError.isEmpty() )
			why = mo2DiskError;
		else if ( !wsBox->currentData().toUInt() )
'''))

H.append(('''		if ( mo2Mode() && !lodgenUnderMo2() ) {
			progress->setFormat( mo2Refusal() );
			return;
		}
		saveSettings();
''', '''		if ( mo2Mode() && !lodgenUnderMo2() ) {
			progress->setFormat( mo2Refusal() );
			return;
		}
		if ( mo2DiskMode() && !mo2DiskError.isEmpty() ) {
			progress->setFormat( mo2DiskError );
			return;
		}
		saveSettings();
'''))

H.append(('''		if ( !mo2Mode() ) {
			QStringList plugins;
''', '''		if ( sourceBox->currentData().toInt() == 0 ) {
			QStringList plugins;
'''))

H.append(('''		s.setValue( QStringLiteral( "LodGeneration/source" ), sourceBox->currentData().toInt() );
''', '''		s.setValue( QStringLiteral( "LodGeneration/source" ), sourceBox->currentData().toInt() );
		s.setValue( QStringLiteral( "LodGeneration/mo2Profile" ), mo2ProfileEdit->text().trimmed() );
		s.setValue( QStringLiteral( "LodGeneration/mo2Mods" ), mo2ModsEdit->text().trimmed() );
'''))

H.append(('''	QStringList mo2Plugins;
''', '''	QStringList mo2Plugins;
	//! Mod Organizer 2 profile (off disk): its rows, its resolved stack, its refusal
	QLineEdit * mo2ProfileEdit = nullptr, * mo2ModsEdit = nullptr;
	QWidget * mo2ProfileHost = nullptr, * mo2ModsHost = nullptr;
	QLabel * mo2ProfileLabel = nullptr, * mo2ModsLabel = nullptr, * mo2StackLabel = nullptr;
	QListWidget * mo2StackList = nullptr;
	QStringList mo2DiskStack;
	QString mo2DiskError;
'''))

for i, (a, b) in enumerate(H):
    n = s.count(a)
    assert n == 1, 'hunk %d anchor count %d: %r' % (i, n, a[:80])
    s = s.replace(a, b)
assert s.count('\r') == cr0
if '--check' in sys.argv:
    print('check ok, %d hunks' % len(H))
else:
    with open(P, 'w', encoding='utf-8', newline='') as f:
        f.write(s)
    print('applied %d hunks' % len(H))
