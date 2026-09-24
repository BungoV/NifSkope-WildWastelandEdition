P = 'E:/Projects/NifskopeWWE-loadorder1/src/nifskope_ui.cpp'
s = open(P, encoding='utf-8', newline='').read()
cr0 = s.count('\r')
old = '''						check( "a source selector offers Specified and Mod Organizer 2",
							source && source->count() == 2
							&& source->itemText( 1 ).contains( QLatin1String( "Mod Organizer" ) ) );
'''
new = '''						// three since lane LOADORDER1 (2026-09-24): the profile read off disk
						check( "a source selector offers Specified, Mod Organizer 2 and a Mod Organizer 2 profile",
							source && source->count() == 3
							&& source->itemText( 1 ).contains( QLatin1String( "Mod Organizer" ) )
							&& source->itemData( 2 ).toInt() == 2
							&& source->itemText( 2 ).contains( QLatin1String( "profile" ) ) );
'''
assert s.count(old) == 1
s = s.replace(old, new)
assert s.count('\r') == cr0
with open(P, 'w', encoding='utf-8', newline='') as f:
    f.write(s)
print('ok')
