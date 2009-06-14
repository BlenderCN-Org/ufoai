// default fragment shader

#include "light_fs.glsl"
#include "bump_fs.glsl"
#include "fog_fs.glsl"

uniform int BUMPMAP;

uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER1;
uniform sampler2D SAMPLER2;
uniform sampler2D SAMPLER3;

const vec3 two = vec3(2.0);
const vec3 negHalf = vec3(-0.5);


/*
main
*/
void main(void){

	vec2 offset = vec2(0.0);
	vec3 bump = vec3(1.0);
	vec3 deluxemap;

	// sample the lightmap
	vec3 lightmap = texture2D(SAMPLER1, gl_TexCoord[1].st).rgb;

#if r_bumpmap
	if(BUMPMAP > 0){  // sample deluxemap and normalmap
		deluxemap = texture2D(SAMPLER2, gl_TexCoord[1].st).rgb;
		deluxemap = normalize(two * (deluxemap + negHalf));

		vec4 normalmap = texture2D(SAMPLER3, gl_TexCoord[0].st);
		normalmap.rgb = normalize(two * (normalmap.rgb + negHalf));

		// resolve parallax offset and bump mapping
		offset = BumpTexcoord(normalmap.a);
		bump = BumpFragment(deluxemap, normalmap.rgb);
	}
#endif

	// sample the diffuse texture, honoring the parallax offset
	vec4 diffuse = texture2D(SAMPLER0, gl_TexCoord[0].st + offset);

	// factor in bump mapping
	diffuse.rgb *= bump;

	// add any dynamic lighting and yield a base fragment color
	LightFragment(diffuse, lightmap);

#if r_fog
	FogFragment();  // add fog
#endif

// developer tools
#if r_lightmap
	gl_FragColor.rgb = lightmap;
	gl_FragColor.a = 1.0;
#endif

#if r_deluxemap
	if(BUMPMAP > 0){
		gl_FragColor.rgb = deluxemap;
		gl_FragColor.a = 1.0;
	}
#endif
}
