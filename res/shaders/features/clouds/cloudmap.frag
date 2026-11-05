#version 410 core

/* lane CLOUD1: a layer's coverage into the map, a = tex.a x edge fade x the layer's JNAM alpha (the drawn 2D layer's
 * alpha, lookdev_clouds.frag). The blend unions it into the band's channel: dst = 1 - (1 - src)(1 - dst).
 * show: the map as colour (R Low, G Mid, B High). */

uniform sampler2D Tex;
uniform sampler2D Tex2;	// lane WEATHERBLEND3: during a change the new weather's layer, cross-faded by texBlend
uniform float texBlend;	// 0 = Tex alone
uniform float alpha;
uniform bool cmShow;

in float vfade;
in vec2 uv;

out vec4 fragColor;

void main()
{
	vec4 t = texture( Tex, uv );
	if ( texBlend > 0.0 )
		t = mix( t, texture( Tex2, uv ), texBlend );
	if ( cmShow ) {
		fragColor = vec4( t.r, t.g, t.a, 1.0 );
		return;
	}
	fragColor = vec4( clamp( t.a * vfade * alpha, 0.0, 1.0 ) );
}
