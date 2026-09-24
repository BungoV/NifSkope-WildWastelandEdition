"""IMPOSTORAA1: the card bake renders each view OFFSCREEN at exactly 2x the
frame's inner size and box-filters 2:1 (bungo 2026-09-22: "2x render then
downscale"). Count==1 anchors, LF-only file."""
P = 'E:/Projects/NifskopeWildWastelandEdition/src/nifskope_ui.cpp'
s = open(P, 'rb').read().decode('utf-8')
assert '\r' not in s


def rep(old, new, count=1):
    global s
    n = s.count(old)
    assert n == count, (n, old[:120])
    s = s.replace(old, new)


# 0. includes
rep('#include <QActionGroup>\n',
    '#include <QActionGroup>\n#include <QOpenGLContext>\n#include <QOpenGLFramebufferObject>\n#include <QOpenGLFunctions>\n')

# 1. the window size is a test knob now (the proof that the sheets do not depend on it)
rep('''					skope->showNormal();
					skope->resize( 560, 560 );
''', '''					skope->showNormal();
					{
						/* WW_IMPOSTOR_WINDOW=WxH (lane IMPOSTORAA1): the bake window's
						 * size, a TEST KNOB. Under the 2x offscreen bake (the default)
						 * nothing the sheets hold depends on it, and the gate proves
						 * that by baking at two sizes and comparing the bytes. */
						int bw = 560, bh = 560;
						const QStringList ws = qEnvironmentVariable( "WW_IMPOSTOR_WINDOW" ).split( QLatin1Char( 'x' ) );
						if ( ws.size() == 2 && ws[0].toInt() >= 320 && ws[1].toInt() >= 240 ) {
							bw = ws[0].toInt();
							bh = ws[1].toInt();
						}
						skope->resize( bw, bh );
					}
''')

# 2. the offscreen renderer, after the channel lambda
rep('''						auto channel = [&]( int which ) {
							wwLodChannelView = which;
							const QImage img = grabOnce();
							wwLodChannelView = 0;
							return img;
						};
''', '''						auto channel = [&]( int which ) {
							wwLodChannelView = which;
							const QImage img = grabOnce();
							wwLodChannelView = 0;
							return img;
						};

						/* THE 2x OFFSCREEN BAKE (lane IMPOSTORAA1, 2026-09-23; bungo
						 * 2026-09-22: "Preferable solution would be a 2x render then
						 * downscale to achieve AA").
						 *
						 * Until this lane every view was a photograph of the LIVE
						 * WINDOW: `grabFramebuffer()` at whatever size the window came
						 * out (its width floored by the main window's minimum size),
						 * with whatever MSAA the user's "Msaa Samples" setting gave the
						 * window surface, cropped and then resized to the frame with
						 * `QImage::scaled( SmoothTransformation )`. So two machines
						 * baked two different sheets, and an MSAA resolve averaged the
						 * DATA channels (normal, depth, material) across silhouette
						 * edges behind the bake's back.
						 *
						 * Now each view renders into its own FBO -- GL_RGBA8, NO MSAA,
						 * no sRGB -- at EXACTLY 2 x the frame's inner size, framed on
						 * that view's own silhouette centre by panning the camera in
						 * its own view plane, and is box-filtered 2:1:
						 *   coverage       = the mean of the four samples' matte alpha;
						 *   colour, height, material, mask, emissive
						 *                  = sum(value x a) / sum(a) over the four,
						 *                    i.e. premultiply, box, un-premultiply;
						 *   normal         = the same weighted sum of the DECODED
						 *                    3-vectors, renormalised.
						 * The frames come out UN-premultiplied, so the per-texel
						 * `unp` below is the identity on this arm. Pass one reads its
						 * silhouette boxes from an offscreen matte of fixed size too.
						 *
						 * MODULE AND FALLBACK (CONSTITUTION 10): `WW_IMPOSTOR_AA=0`
						 * restores the window photograph exactly, and the sidecar's
						 * `aa` line names the arm that served. */
						const bool aaOn = !( qEnvironmentVariableIsSet( "WW_IMPOSTOR_AA" )
							&& qEnvironmentVariableIntValue( "WW_IMPOSTOR_AA" ) == 0 );
						const int p1Size = qMax( 1024, 2 * tile );		// pass one's offscreen matte, square
						float aaCentreErr = 0.0f;						// worst silhouette-centre miss, output texels
						auto renderOff = [&]( int w, int h, float half, float ox, float oy ) {
							GLView * gl = skope->ogl;
							const float oldDist = gl->Dist;
							const Vector3 oldPos = gl->Pos;
							const QSize oldSize = gl->getSizeInPixels();
							QImage out;
							auto prv = gl->pushGLContext();
							gl->resizeGL( w, h );
							// ortho half-height = Dist / Zoom; depth is |centre| +- 1.5 bound, so
							// neither this nor the pan moves the window z the height sheet stores
							gl->Dist = float( double( half ) * gl->Zoom );
							/* The pan: the view-space point ( ox, -oy ) -- `oy` is pixel-down --
							 * to the centre. viewTransform() translates by R * Pos with R the
							 * UNshuffled Euler matrix, so Pos moves by R^-1 * ( -ox, +oy, 0 ). */
							Matrix R;
							R.fromEuler( deg2rad( gl->Rot[0] ), deg2rad( gl->Rot[1] ), deg2rad( gl->Rot[2] ) );
							gl->Pos = oldPos + R.inverted() * Vector3( -ox, oy, 0.0f );
							try {
								QOpenGLFramebufferObjectFormat ff;
								ff.setTextureTarget( GL_TEXTURE_2D );
								ff.setInternalTextureFormat( GL_RGBA8 );
								ff.setMipmap( false );
								ff.setSamples( 0 );
								ff.setAttachment( QOpenGLFramebufferObject::CombinedDepthStencil );
								QOpenGLFramebufferObject fbo( w, h, ff );
								if ( fbo.isValid() ) {
									fbo.bind();
									gl->paintGL();
									fbo.bind();
									std::vector<uchar> buf( size_t( w ) * size_t( h ) * 4 );
									QOpenGLFunctions * f = QOpenGLContext::currentContext()->functions();
									f->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
									f->glReadPixels( 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf.data() );
									fbo.release();
									out = QImage( w, h, QImage::Format_ARGB32 );
									for ( int y = 0; y < h; y++ ) {
										const uchar * src = buf.data() + size_t( h - 1 - y ) * size_t( w ) * 4;
										QRgb * dst = reinterpret_cast<QRgb *>( out.scanLine( y ) );
										for ( int x = 0; x < w; x++ )
											dst[x] = qRgb( src[4 * x], src[4 * x + 1], src[4 * x + 2] );
									}
								}
							} catch ( std::exception & ) {
								out = QImage();
							}
							gl->Dist = oldDist;
							gl->Pos = oldPos;
							gl->resizeGL( oldSize.width(), oldSize.height() );
							gl->popGLContext( prv );
							return out;
						};
						/* The matte, offscreen: rgb = the BLACK pass as rendered (colour x
						 * coverage, still premultiplied), alpha = this sample's coverage. */
						auto matteOff = [&]( int w, int h, float half, float ox, float oy ) {
							// settle the view once through the window path's own event
							// round, so the offscreen passes see exactly what a grab would
							skope->ogl->update();
							qApp->processEvents();
							QImage pass[2];
							if ( bakeScene )
								bakeScene->options = Scene::SceneOptions( litOptions & ~Scene::DoLighting );
							wwLodChannelView = 12;
							for ( int b = 0; b < 2; b++ ) {
								skope->ogl->setBackground( bgs[b] );
								pass[b] = renderOff( w, h, half, ox, oy );
							}
							wwLodChannelView = 0;
							if ( bakeScene )
								bakeScene->options = litOptions;
							skope->ogl->setBackground( bgs[0] );
							QImage cov( w, h, QImage::Format_ARGB32 );
							cov.fill( 0 );
							if ( pass[0].isNull() || pass[1].isNull() )
								return cov;
							for ( int y = 0; y < h; y++ )
								for ( int x = 0; x < w; x++ ) {
									const QRgb pb = pass[0].pixel( x, y ), pw = pass[1].pixel( x, y );
									const int d = ( ( qRed( pw ) - qRed( pb ) ) + ( qGreen( pw ) - qGreen( pb ) )
										+ ( qBlue( pw ) - qBlue( pb ) ) ) / 3;
									cov.setPixel( x, y, qRgba( qRed( pb ), qGreen( pb ), qBlue( pb ), qBound( 0, 255 - d, 255 ) ) );
								}
							return cov;
						};
						auto channelOff = [&]( int which, int w, int h, float half, float ox, float oy ) {
							wwLodChannelView = which;
							const QImage img = renderOff( w, h, half, ox, oy );
							wwLodChannelView = 0;
							return img;
						};
''')

# 3. pass one reads its boxes from the offscreen matte
rep('''								skope->ogl->setRotation( rx, 0.0f, rz );
								const QImage cov = matte();
								const int W = cov.width(), H = cov.height();
								const float upp = 2.0f * halfH0 / float( H );		// units per pixel, both axes
''', '''								skope->ogl->setRotation( rx, 0.0f, rz );
								// the 2x arm: a square offscreen matte at a fixed size, half-extent
								// halfH0 on both axes, so the boxes do not depend on the window
								const QImage cov = aaOn ? matteOff( p1Size, p1Size, halfH0, 0.0f, 0.0f ) : matte();
								const int W = cov.width(), H = cov.height();
								const float upp = 2.0f * halfH0 / float( H );		// units per pixel, both axes
''')

# 4. pass two: the six frames
rep('''								const QImage tA = frameOf( matte(), ox, oy );
								const QImage tN = frameOf( channel( 8 ), ox, oy );		// normal, view space
								const QImage tD = frameOf( channel( 9 ), ox, oy );		// window depth
								const QImage tS = frameOf( channel( 10 ), ox, oy );		// the material channel: legacy pair, or a .lodm's third texture raw
								const QImage tM = frameOf( channel( 11 ), ox, oy );		// alpha-tested: the leaf cards
								const QImage tE = frameOf( channel( 13 ), ox, oy );		// the emissive: a .lodm's texture raw, or the vanilla glow rule
''', '''								QImage tA, tN, tD, tS, tM, tE;
								if ( !aaOn ) {
									tA = frameOf( matte(), ox, oy );
									tN = frameOf( channel( 8 ), ox, oy );		// normal, view space
									tD = frameOf( channel( 9 ), ox, oy );		// window depth
									tS = frameOf( channel( 10 ), ox, oy );		// the material channel: legacy pair, or a .lodm's third texture raw
									tM = frameOf( channel( 11 ), ox, oy );		// alpha-tested: the leaf cards
									tE = frameOf( channel( 13 ), ox, oy );		// the emissive: a .lodm's texture raw, or the vanilla glow rule
								} else {
									/* THE 2x ARM: the frame's inner rect, exactly, at 2 x iw by
									 * 2 x ih samples -- halfW / halfH == iw / ih, so the samples
									 * are square -- centred on this view's silhouette, then 2:1. */
									const int RW = 2 * iw, RH = 2 * ih;
									const QImage sA = matteOff( RW, RH, halfH, ox, oy );
									const QImage s8 = channelOff( 8, RW, RH, halfH, ox, oy );
									const QImage s9 = channelOff( 9, RW, RH, halfH, ox, oy );
									const QImage s10 = channelOff( 10, RW, RH, halfH, ox, oy );
									const QImage s11 = channelOff( 11, RW, RH, halfH, ox, oy );
									const QImage s13 = channelOff( 13, RW, RH, halfH, ox, oy );
									QImage * outs[6] = { &tA, &tN, &tD, &tS, &tM, &tE };
									for ( QImage * o : outs ) {
										*o = QImage( tw, th, QImage::Format_ARGB32 );
										o->fill( 0 );
									}
									const QImage * chans[4] = { &s9, &s10, &s11, &s13 };
									QImage * chOut[4] = { &tD, &tS, &tM, &tE };
									// the self-check on the pan: the covered samples' box must sit
									// on the render's centre, as pass one measured it
									int bx0 = RW, bx1 = -1, by0 = RH, by1 = -1;
									for ( int y = 0; y < ih; y++ ) {
										for ( int x = 0; x < iw; x++ ) {
											int sa = 0, cr = 0, cg = 0, cb = 0;
											int ch[4][3] = {};
											float nx = 0.0f, ny = 0.0f, nz = 0.0f;
											for ( int q = 0; q < 4; q++ ) {
												const int sx = 2 * x + ( q & 1 ), sy = 2 * y + ( q >> 1 );
												const QRgb pa = sA.pixel( sx, sy );
												const int a = qAlpha( pa );
												if ( a >= 16 ) {
													bx0 = qMin( bx0, sx ); bx1 = qMax( bx1, sx );
													by0 = qMin( by0, sy ); by1 = qMax( by1, sy );
												}
												if ( a == 0 )
													continue;
												sa += a;
												cr += qRed( pa ); cg += qGreen( pa ); cb += qBlue( pa );
												for ( int c = 0; c < 4; c++ ) {
													const QRgb pc = chans[c]->pixel( sx, sy );
													ch[c][0] += qRed( pc ); ch[c][1] += qGreen( pc ); ch[c][2] += qBlue( pc );
												}
												// the normal: this sample's value un-premultiplied, decoded,
												// weighted by its coverage -- i.e. 2c - a per component
												const QRgb pn = s8.pixel( sx, sy );
												nx += float( 2 * qRed( pn ) - a );
												ny += float( 2 * qGreen( pn ) - a );
												nz += float( 2 * qBlue( pn ) - a );
											}
											const int X = padX + x, Y = padY + y;
											const int cov = ( sa + 2 ) / 4;
											if ( sa == 0 )
												continue;
											auto un = [sa]( int c ) { return qBound( 0, ( c * 255 + sa / 2 ) / sa, 255 ); };
											tA.setPixel( X, Y, qRgba( un( cr ), un( cg ), un( cb ), cov ) );
											for ( int c = 0; c < 4; c++ )
												chOut[c]->setPixel( X, Y, qRgba( un( ch[c][0] ), un( ch[c][1] ), un( ch[c][2] ), 255 ) );
											const float len = std::sqrt( nx * nx + ny * ny + nz * nz );
											if ( len > 1.0e-6f ) {
												auto enc = [len]( float v ) { return qBound( 0, int( ( v / len * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 ); };
												tN.setPixel( X, Y, qRgba( enc( nx ), enc( ny ), enc( nz ), 255 ) );
											} else {
												tN.setPixel( X, Y, qRgba( 128, 128, 255, 255 ) );
											}
										}
									}
									if ( bx1 >= 0 ) {
										const float ex = std::fabs( 0.5f * float( bx0 + bx1 + 1 ) - 0.5f * float( RW ) );
										const float ey = std::fabs( 0.5f * float( by0 + by1 + 1 ) - 0.5f * float( RH ) );
										aaCentreErr = qMax( aaCentreErr, 0.5f * qMax( ex, ey ) );	// samples -> texels
									}
								}
''')

# 5. unp is the identity on the 2x arm (the frames are already un-premultiplied)
rep('''										auto unp = [a]( int c ) { return qMin( 255, ( c * 255 + a / 2 ) / a ); };''',
    '''										auto unp = [a, aaOn]( int c ) { return aaOn ? c : qMin( 255, ( c * 255 + a / 2 ) / a ); };''')
rep('''												if ( qMin( 255, ( qRed( tD.pixel( sx, sy ) ) * 255 + na / 2 ) / na ) + 2 < z )''',
    '''												if ( ( aaOn ? qRed( tD.pixel( sx, sy ) )
														: qMin( 255, ( qRed( tD.pixel( sx, sy ) ) * 255 + na / 2 ) / na ) ) + 2 < z )''')

# 6. the sidecar names the arm
rep('''						ms << "frameclamped " << clamped << "\\n";
''', '''						ms << "frameclamped " << clamped << "\\n";
						/* THE PHOTOGRAPH'S ARM (lane IMPOSTORAA1): `aa 2 <pass-one size>
						 * <worst centre miss in texels>` for the 2x offscreen bake, `aa 0
						 * <window WxH>` for the window photograph (WW_IMPOSTOR_AA=0).
						 * Unknown to every older reader, which skips lines it does not
						 * name; the sheet bytes keep their meaning either way. */
						if ( aaOn )
							ms << "aa 2 " << p1Size << " " << aaCentreErr << "\\n";
						else
							ms << "aa 0 " << skope->ogl->getSizeInPixels().width() << "x"
							   << skope->ogl->getSizeInPixels().height() << " crop "
							   << int( float( skope->ogl->getSizeInPixels().height() ) * halfH / fitH + 0.5f )
							   << " rows into " << ih << "\\n";
''')

open(P, 'wb').write(s.encode('utf-8'))
print('patched OK')
