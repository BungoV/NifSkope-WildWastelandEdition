"""The heredoc ate a backslash (lodgen skill, "editing traps"): the result
label's QStringLiteral got a real newline instead of an escaped one. Rebuild
the escape from chr(92) so no backslash ever travels through a shell."""
BS = chr(92)
P = 'src/lodgenmanager.cpp'
s = open(P, encoding='utf-8', newline='').read()
bad = 'QStringLiteral( "\n" ) + lodgenBakeCensusLine()'
good = 'QStringLiteral( "' + BS + 'n" ) + lodgenBakeCensusLine()'
assert s.count(bad) == 1, s.count(bad)
s = s.replace(bad, good)
open(P, 'w', encoding='utf-8', newline='').write(s)
print('fixed')
