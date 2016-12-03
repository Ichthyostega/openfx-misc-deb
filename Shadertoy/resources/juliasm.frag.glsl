// See: https://www.shadertoy.com/view/XssXDr
// max_iterations was 14 in the screensaver version
uniform int max_iterations = 255; // Max. Iterations (Maximum number of iterations), min=0, max=500

vec2 complex_square( vec2 v ) {
	return vec2(
		v.x * v.x - v.y * v.y,
		v.x * v.y * 2.0
	);
}


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 uv = fragCoord.xy - iResolution.xy * 0.5;
	uv *= 2.5 / min( iResolution.x, iResolution.y );
	
	vec2 c = vec2( 0.37+cos(iGlobalTime*1.23462673423)*0.04, sin(iGlobalTime*1.43472384234)*0.10+0.50);
	vec2 v = uv;
	float scale = 0.01;
	
	float smoothcolor = exp(-length(v));

	for ( int i = 0 ; i < max_iterations; i++ ) {
		v = c + complex_square( v );
		smoothcolor += exp(-length(v));
		if ( dot( v, v ) > 4.0 ) {
			break;
		}
	}
	/*
fragColor = 0.4*(1.0+
						vec4( sin(smoothcolor*0.05+iGlobalTime*0.63), 
							 sin(smoothcolor*0.04+iGlobalTime*0.32), 
							 sin(smoothcolor*0.03+iGlobalTime*0.7),1.0));
*/
	float r = 0.6+0.4*(sin(smoothcolor*0.1+iGlobalTime*0.63));
	float g = r * r;
	float b = r * g;
	fragColor = vec4(r,g,b,0);
}