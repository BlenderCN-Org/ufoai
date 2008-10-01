// bumpmap fragment shader

varying vec3 eyedir;

uniform float BUMP;
uniform float PARALLAX;
uniform float SPECULAR;

/*
BumpTexcoord
*/
vec2 BumpTexcoord(in float height){

	eyedir = normalize(eyedir);

	return vec2(height * PARALLAX * 0.04 - 0.02) * eyedir.xy;
}

/*
BumpFragment
*/
void BumpFragment(in vec3 deluxemap, in vec3 normalmap){

	float diffuse = dot(deluxemap,
					vec3(normalmap.x * BUMP, normalmap.y * BUMP, normalmap.z));

	float specular = pow(max(-dot(eyedir,
					reflect(deluxemap, normalmap)), 0.0), 8.0 * SPECULAR);

	gl_FragColor.rgb *= vec3(diffuse + specular);
}
