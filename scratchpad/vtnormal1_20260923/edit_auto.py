"""VTNORMAL1 addition (bungo 2026-09-24): the normal-sheets folder accepts a
MOD ROOT (or a Data / Textures folder) and finds <root>/Textures/Terrain/<ws>/;
"auto" picks the last resource-stack folder holding an upscaled set (sheets
wider than vanilla's 512); the panel's empty field means auto, "none" off."""
import sys
R = 'E:/Projects/NifskopeWildWastelandEdition/src/'


def patch(fn, pairs):
	p = R + fn
	s = open(p, 'rb').read().decode('utf-8')
	assert s.count('\r') == 0, fn
	for old, new in pairs:
		n = s.count(old)
		if n != 1:
			sys.exit('%s: anchor count %d: %r' % (fn, n, old[:60]))
		s = s.replace(old, new)
	open(p, 'wb').write(s.encode('utf-8'))
	print(fn, 'ok', len(pairs))


# ---------------------------------------------------------------- lodgen.cpp
L_GET_OLD = """QString lodgenMsnCacheDir()
{
	return g_msnCacheDir;
}
"""
L_GET_NEW = """/*! The sheet's own file inside a normal-sheets folder (bungo 2026-09-24): the
 *  folder itself first (the rung's only place), then, so a MOD ROOT, a Data
 *  folder or a Textures folder can be named, `Textures/Terrain/<world>/` and
 *  `Terrain/<world>/` under it; <world> is the name up to its first dot. The
 *  first place is returned when none has it, so a miss reads as before. */
static QString lodgenMsnSheetPath( const QString & root, const QString & fileName )
{
	const QDir d( root );
	const QString first = d.filePath( fileName );
	if ( QFileInfo( first ).exists() )
		return first;
	const QString ws = fileName.section( QChar( '.' ), 0, 0 );
	if ( ws.isEmpty() )
		return first;
	for ( const QString & sub : { QStringLiteral( "Textures/Terrain/" ) + ws,
			QStringLiteral( "Terrain/" ) + ws } ) {
		const QString p = d.filePath( sub + QChar( '/' ) + fileName );
		if ( QFileInfo( p ).exists() )
			return p;
	}
	return first;
}

/*! Does this folder hold an UPSCALED dim-4 normal set for some world: a
 *  `Textures/Terrain/<world>/<world>.4.*_msn.DDS` (or the same under
 *  `Terrain/`, or loose in the folder) whose DDS width is above vanilla's 512.
 *  Vanilla's own sheets, loose in an unpacked Data, are 512 and do not count. */
static bool lodgenMsnFolderHasUpscaledSet( const QString & root )
{
	QStringList dirs;
	dirs << root;
	for ( const QString & t : { QStringLiteral( "Textures/Terrain" ), QStringLiteral( "Terrain" ) } ) {
		const QDir td( QDir( root ).filePath( t ) );
		if ( !td.exists() )
			continue;
		for ( const QString & w : td.entryList( QDir::Dirs | QDir::NoDotAndDotDot ) )
			dirs << td.filePath( w );
	}
	for ( const QString & dir : dirs ) {
		const QDir d( dir );
		const QStringList sheets = d.entryList( { QStringLiteral( "*.4.*_msn.dds" ) }, QDir::Files );
		if ( sheets.isEmpty() )
			continue;
		QFile f( d.filePath( sheets.first() ) );
		if ( !f.open( QIODevice::ReadOnly ) )
			continue;
		const QByteArray hd = f.read( 20 );
		if ( hd.size() < 20 || !hd.startsWith( "DDS " ) )
			continue;
		quint32 width = 0;
		memcpy( &width, hd.constData() + 16, 4 );
		if ( width > 512 )
			return true;
	}
	return false;
}

/*! The folder the sheets are read from. `auto` (the panel's empty row) is the
 *  LAST resource-stack folder with an upscaled set (the stack is last-wins),
 *  or none; archives in the stack are skipped. Remembered per stack. */
QString lodgenMsnCacheDir()
{
	if ( g_msnCacheDir.compare( QStringLiteral( "auto" ), Qt::CaseInsensitive ) != 0 )
		return g_msnCacheDir;
	// the chunk-sheet writer asks from its worker threads
	static QMutex memoLock;
	QMutexLocker lock( &memoLock );
	static QString memoKey, memoDir;
	const QStringList stack = lodgenResources();
	const QString key = stack.join( QChar( '\\n' ) );
	if ( key == memoKey && !memoKey.isNull() )
		return memoDir;
	QString found;
	for ( int i = stack.size() - 1; i >= 0 && found.isEmpty(); i-- ) {
		if ( QFileInfo( stack.at( i ) ).isDir() && lodgenMsnFolderHasUpscaledSet( stack.at( i ) ) )
			found = stack.at( i );
	}
	memoKey = key.isNull() ? QStringLiteral( "" ) : key;
	memoDir = found;
	return found;
}

bool lodgenMsnCacheAuto()
{
	return g_msnCacheDir.compare( QStringLiteral( "auto" ), Qt::CaseInsensitive ) == 0;
}
"""

L_READ_OLD = """	if ( g_msnCacheDir.isEmpty() )
		return false;
	const QString dds = QDir( g_msnCacheDir ).filePath(
		QFileInfo( base ).fileName() + QStringLiteral( "_msn.DDS" ) );
	if ( QFileInfo( dds ).exists() && lodgenMsnFromAssembledDds( dds, out, w, h, vec ) )
		return true;
	const QString png = QDir( g_msnCacheDir ).filePath(
		QFileInfo( base ).fileName() + QStringLiteral( ".png" ) );"""
L_READ_NEW = """	const QString root = lodgenMsnCacheDir();
	if ( root.isEmpty() )
		return false;
	const QString dds = lodgenMsnSheetPath( root,
		QFileInfo( base ).fileName() + QStringLiteral( "_msn.DDS" ) );
	if ( QFileInfo( dds ).exists() && lodgenMsnFromAssembledDds( dds, out, w, h, vec ) )
		return true;
	const QString png = lodgenMsnSheetPath( root,
		QFileInfo( base ).fileName() + QStringLiteral( ".png" ) );"""

L_MISS_OLD = """	if ( !g_msnCacheDir.isEmpty() )
		g_vrMsnCacheMiss++;"""
L_MISS_NEW = """	if ( !lodgenMsnCacheDir().isEmpty() )
		g_vrMsnCacheMiss++;"""

L_CEN_OLD = """			r << QString( "msnCacheDir %1" ).arg( lodgenMsnCacheDir().isEmpty()
				? QStringLiteral( "(none)" ) : lodgenMsnCacheDir() );"""
L_CEN_NEW = """			r << QString( "msnCacheDir %1%2" ).arg( lodgenMsnCacheDir().isEmpty()
				? QStringLiteral( "(none)" ) : lodgenMsnCacheDir(),
				lodgenMsnCacheAuto() ? QStringLiteral( " (auto)" ) : QString() );"""

patch('lodgen.cpp', [(L_GET_OLD, L_GET_NEW), (L_READ_OLD, L_READ_NEW),
					 (L_MISS_OLD, L_MISS_NEW), (L_CEN_OLD, L_CEN_NEW)])

# ---------------------------------------------------------------- lodgen.h
H_OLD = """void lodgenSetMsnCacheDir( const QString & dir );  // empty = off = the rung's bytes
"""
H_NEW = """void lodgenSetMsnCacheDir( const QString & dir );  // empty = off = the rung's bytes; "auto" = the resource stack's upscaled set
bool lodgenMsnCacheAuto();  // the setting is "auto" (lodgenMsnCacheDir() then names what it found, or nothing)
"""
patch('lodgen.h', [(H_OLD, H_NEW)])

# ---------------------------------------------------------------- panel
P_SET_OLD = """		lodgenSetMsnCacheDir( xs( "msnCache" ) );"""
P_SET_NEW = """		{
			// bungo 2026-09-24: an empty row means FIND his upscaled set in the
			// resources (the generator's "auto"); "none" turns the sheets off
			const QString mc = xs( "msnCache" ).trimmed();
			lodgenSetMsnCacheDir( mc.isEmpty() ? QStringLiteral( "auto" )
				: mc.compare( QStringLiteral( "none" ), Qt::CaseInsensitive ) == 0 ? QString() : mc );
		}"""
P_TIP_OLD = """				tr( "A folder of cleaned or upscaled terrain normal sheets, one\\n"
					"<world>.4.<x>.<y>_msn.DDS per chunk. When set, they are the chunk\\n"
					"sheets' normal and the terrain pyramid's, reduced to its texel size;\\n"
					"a chunk with no sheet keeps the normal from the heights.\\n"
					"Empty means none.\\n"
					"Command line: --msn-cache" ), true );"""
P_TIP_NEW = """				tr( "A folder of cleaned or upscaled terrain normal sheets, one\\n"
					"<world>.4.<x>.<y>_msn.DDS per chunk: the sheets' own folder, or the\\n"
					"mod folder that holds them under Textures\\\\Terrain\\\\<world>. They are\\n"
					"the chunk sheets' normal and the terrain pyramid's, reduced to its\\n"
					"texel size; a chunk with no sheet keeps the normal from the heights.\\n"
					"Empty: the last resource folder holding an upscaled set is used, if\\n"
					"any. none: never.\\n"
					"Command line: --msn-cache DIR|auto" ), true );"""
patch('lodgenmanager.cpp', [(P_SET_OLD, P_SET_NEW), (P_TIP_OLD, P_TIP_NEW)])
