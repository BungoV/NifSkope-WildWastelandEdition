p = 'src/lodifile.cpp'
s = open(p, encoding='utf-8', newline='').read()


def rep(old, new, n=1):
    global s
    assert s.count(old) == n, (s.count(old), old[:90])
    s = s.replace(old, new)


rep("""		<< QString( "vertexAoBytes %1" ).arg( h.vertexAoBytes )""",
    """		<< QString( "vertexAoBytes %1" ).arg( h.vertexAoBytes )
		<< QString( "offGroup %1" ).arg( h.offGroup )
		<< QString( "groupCount %1" ).arg( h.groupCount )
		<< QString( "groupStride %1" ).arg( h.groupStride )
		<< QString( "offVertexSky %1" ).arg( h.offVertexSky )
		<< QString( "vertexSkyBytes %1" ).arg( h.vertexSkyBytes )""")

rep("""			out << QString( "vertexAoInstances %1" ).arg( withRange )
				<< QString( "vertexAoBytesTotal %1" ).arg( table->vertexAo.size() )
				<< QString( "vertexAoMean %1" ).arg( table->vertexAo.empty() ? 0.0 : vSum / double( table->vertexAo.size() ), 0, 'f', 2 )
				<< QString( "vertexAoDark %1" ).arg( dark );
		}
	}""",
    """			out << QString( "vertexAoInstances %1" ).arg( withRange )
				<< QString( "vertexAoBytesTotal %1" ).arg( table->vertexAo.size() )
				<< QString( "vertexAoMean %1" ).arg( table->vertexAo.empty() ? 0.0 : vSum / double( table->vertexAo.size() ), 0, 'f', 2 )
				<< QString( "vertexAoDark %1" ).arg( dark );
		}
		/* v7 (a): the group table. `groups` is the header's own word; the other
		 * three are counted here from the ids, so a table that says one thing
		 * and holds another shows as a disagreement rather than as silence. */
		if ( !table->group.empty() ) {
			quint32 grouped = 0, largest = 0, singletons = 0;
			for ( const LodiChunk & c : table->chunks ) {
				if ( c.instanceCount == 0 || quint64( c.instanceFirst ) + c.instanceCount > table->group.size() )
					continue;
				std::vector<quint32> members;
				for ( quint32 i = 0; i < c.instanceCount; i++ ) {
					const quint16 g = table->group[c.instanceFirst + i];
					if ( g >= members.size() )
						members.resize( size_t( g ) + 1, 0 );
					members[g]++;
				}
				for ( quint32 m : members ) {
					largest = std::max( largest, m );
					if ( m == 1 )
						singletons++;
					else
						grouped += m;
				}
			}
			out << QString( "groups %1" ).arg( h.groupCount )
				<< QString( "groupedPlacements %1" ).arg( grouped )
				<< QString( "largestGroup %1" ).arg( largest )
				<< QString( "singletonGroups %1" ).arg( singletons );
		}
		/* v7 (b): the vertex-sky stream, the same four numbers s4.8's stream
		 * reports, plus the mean the 0x11 byte is compared against. */
		if ( !table->vertexSkyFirst.empty() ) {
			quint32 withRange = 0, open = 0;
			double sSum = 0.0;
			for ( size_t i = 0; i + 1 < table->vertexSkyFirst.size(); i++ )
				if ( table->vertexSkyFirst[i + 1] > table->vertexSkyFirst[i] )
					withRange++;
			for ( quint8 v : table->vertexSky ) {
				sSum += v;
				if ( v >= 128 )
					open++;
			}
			out << QString( "vertexSkyPlacements %1" ).arg( withRange )
				<< QString( "vertexSkyBytesTotal %1" ).arg( table->vertexSky.size() )
				<< QString( "vertexSkyMean %1" ).arg( table->vertexSky.empty() ? 0.0 : sSum / double( table->vertexSky.size() ), 0, 'f', 2 )
				<< QString( "vertexSkyOpen %1" ).arg( open );
		}
	}""")

open(p, 'w', encoding='utf-8', newline='').write(s)
print('census spliced, %d lines' % s.count('\n'))
