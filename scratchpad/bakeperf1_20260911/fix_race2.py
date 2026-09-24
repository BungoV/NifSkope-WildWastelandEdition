"""BAKEPERF1, fix 2: warm a NifModel on the calling thread.

`lodgenWarmSharedIndices()` already builds the three archive indices before the
fan-out. A NifModel's own first construction is lazy too -- the array-pseudonym
tables, the QSettings read, the entry in GameManager's resource map -- and in a
`-no-gui lodgen` run the FIRST NifModel of the process is the one a worker
builds. Building one here, and throwing it away, moves every one of those first
times onto the calling thread where nothing races.
"""
P = 'src/lodgen.cpp'
s = open(P, encoding='utf-8', newline='').read()

old = """	Game::GameManager::find_file( Game::FALLOUT_4,
		QStringLiteral( "ww_lodgen_warm_up" ), "textures", ".dds" );
}
"""
new = """	Game::GameManager::find_file( Game::FALLOUT_4,
		QStringLiteral( "ww_lodgen_warm_up" ), "textures", ".dds" );
	/* And ONE NifModel, built and thrown away. Its constructor fills the
	 * array-pseudonym tables, reads QSettings and takes an entry in the game
	 * manager's resource map -- all "first time only" work, and in a -no-gui
	 * run the first NifModel of the process would otherwise be one a worker
	 * builds, sixteen of them at once. */
	{
		NifModel warmModel;
		(void) warmModel.getBlockCount();
	}
}
"""
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
open(P, 'w', encoding='utf-8', newline='').write(s)
print('warm-up model added')
