attribute vec3 vPosition;
attribute vec3 vNormal;
attribute vec2 vTexCoord;

varying vec3 fN;
varying vec3 fV;
varying vec3 fL;

varying vec2 texCoord;

uniform mat4 ModelView;
uniform mat4 Projection;
uniform vec4 LightPosition;

void main()
{
    vec4 vpos = vec4(vPosition, 1.0);

	// Pass the fragment shader the vertex normal vector, light vector and viewer vector,
	// which the fragment shader will get interpolated
	fN = (ModelView * vec4(vNormal, 0)).xyz;
	fL = LightPosition.xyz - (ModelView * vpos).xyz; 
	fV = -(ModelView * vpos).xyz;
		
    gl_Position = Projection * ModelView * vpos;
    texCoord = vTexCoord;
}
