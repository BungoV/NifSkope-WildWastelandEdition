"""FARRING1 step 10: re-fetch Vertex Data and Segment AFTER setState(Processing)
in the far-ring writer, the way lodgenMergeChunkShapes does -- a model index
held across a resize is exactly the kind of thing that works until it does not."""

P = 'src/lodgen.cpp'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')


def once(hay, needle):
    n = hay.count(needle)
    assert n == 1, 'anchor matched %d times, want 1:\n%s' % (n, needle[:240])


A = ("\t\t\tnif.setState( BaseModel::Processing );\n"
     "\t\t\tnif.updateArraySize( iVD );\n"
     "\t\t\tfloat mnx = 3.4e38f, mny = 3.4e38f, mnz = 3.4e38f, mxx = -3.4e38f, mxy = -3.4e38f, mxz = -3.4e38f;\n"
     "\t\t\tfor ( int v = 0; v < newVerts.size(); v++ ) {\n"
     "\t\t\t\tconst QModelIndex row = nif.index( v, 0, iVD );\n")
once(s, A)
s = s.replace(A,
    "\t\t\tnif.setState( BaseModel::Processing );\n"
    "\t\t\t/* Re-fetched after the state change and before the resize, as the\n"
    "\t\t\t * merge does: an index held across updateArraySize is the kind of\n"
    "\t\t\t * thing that works until the array shrinks under it. */\n"
    "\t\t\tconst QModelIndex iVDw = nif.getIndex( iShape, \"Vertex Data\" );\n"
    "\t\t\tnif.updateArraySize( iVDw );\n"
    "\t\t\tfloat mnx = 3.4e38f, mny = 3.4e38f, mnz = 3.4e38f, mxx = -3.4e38f, mxy = -3.4e38f, mxz = -3.4e38f;\n"
    "\t\t\tfor ( int v = 0; v < newVerts.size(); v++ ) {\n"
    "\t\t\t\tconst QModelIndex row = nif.index( v, 0, iVDw );\n")

B = ("\t\t\tnif.set<quint32>( iShape, \"Num Primitives\", numTris );\n"
     "\t\t\tif ( iSegs.isValid() ) {\n"
     "\t\t\t\tnif.set<quint32>( iShape, \"Num Segments\", quint32( segRuns.size() ) );\n"
     "\t\t\t\tnif.set<quint32>( iShape, \"Total Segments\", quint32( segRuns.size() ) );\n"
     "\t\t\t\tnif.updateArraySize( iSegs );\n"
     "\t\t\t\tfor ( int s = 0; s < segRuns.size(); s++ ) {\n"
     "\t\t\t\t\tconst QModelIndex seg = nif.index( s, 0, iSegs );\n")
once(s, B)
s = s.replace(B,
    "\t\t\tnif.set<quint32>( iShape, \"Num Primitives\", numTris );\n"
    "\t\t\tconst QModelIndex iSegsW = nif.getIndex( iShape, \"Segment\" );\n"
    "\t\t\tif ( iSegsW.isValid() ) {\n"
    "\t\t\t\tnif.set<quint32>( iShape, \"Num Segments\", quint32( segRuns.size() ) );\n"
    "\t\t\t\tnif.set<quint32>( iShape, \"Total Segments\", quint32( segRuns.size() ) );\n"
    "\t\t\t\tnif.updateArraySize( iSegsW );\n"
    "\t\t\t\tfor ( int s = 0; s < segRuns.size(); s++ ) {\n"
    "\t\t\t\t\tconst QModelIndex seg = nif.index( s, 0, iSegsW );\n")

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('src/lodgen.cpp: %d -> %d bytes' % (len(b), len(out)))
