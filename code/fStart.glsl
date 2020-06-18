
varying vec3 fN;
varying vec3 fV;
varying vec3 fL;

varying vec2 texCoord;

uniform sampler2D texture;

uniform float TexScale;
uniform mat4 ModelView;
uniform mat4 Projection;
uniform vec4 LightPosition;
uniform vec4 Light2Position;

uniform vec3 AmbientProduct, DiffuseProduct, SpecularProduct;
uniform vec3 AmbientProduct2, DiffuseProduct2, SpecularProduct2;
uniform float Shininess;
uniform float LightDropOff;
uniform float SpotlightConeAngle;
uniform vec4 SpotlightDirection;
uniform bool SpotlightEnabled;

void main()
{
    // Unit direction vectors for Blinn-Phong shading calculation
    vec3 L = normalize(fL); // Direction to light 1
    vec3 V = normalize(fV); // Direction to the viewer
    vec3 N = normalize(fN); // Normal vector
    vec3 H = normalize(L + V);  // Halfway vector

    vec3 L2 = normalize(Light2Position.xyz); // Light 2 direction same for every fragment
    vec3 H2 = normalize(L2 + V);			 // Light 2 halfway vector

    // Compute terms in the illumination equation
    vec3 ambient = AmbientProduct;
    vec3 ambient2 = AmbientProduct2;

    float Kd = max(dot(L, N), 0.0);
    float Kd2 = max(dot(L2, N), 0.0);

    vec3 diffuse = Kd * DiffuseProduct;
    vec3 diffuse2 = Kd2 * DiffuseProduct2;

    float Ks = pow(max(dot(N, H), 0.0), Shininess);
    float Ks2 = pow(max(dot(N, H2), 0.0), Shininess);
    
    vec3 specular = Ks * SpecularProduct;
    vec3 specular2 = Ks2 * SpecularProduct2;
    
	// Light is coming in at 90 degrees or more to the face of the fragment, so can't have a specular reflection
    if (dot(L, N) < 0.0 ) {
	    specular = vec3(0.0, 0.0, 0.0);
    }
    if (dot(L2, N) < 0.0 ) {
	    specular2 = vec3(0.0, 0.0, 0.0);
    }

	if (SpotlightEnabled) {
		// If the fragment is not within the light's cone it is not illuminated. 
		vec3 SpotlightDirectionNormalised = normalize(-SpotlightDirection.xyz);
		if (dot(L, SpotlightDirectionNormalised) < cos(SpotlightConeAngle)) {
	 	   specular = vec3(0.0, 0.0, 0.0);
		    diffuse = vec3(0.0, 0.0, 0.0);
	    	ambient = vec3(0.0, 0.0, 0.0);
		}
	}

	// Light attenuation increases with distance and the LightDropOff value
	float distance = length(fL);
	float a = 1.0;
	float b = 0.0;
	float c = LightDropOff/2;
	float lightAttenuation = a + (b*distance) + (c*pow(distance, 2.0));
	
    // globalAmbient is independent of distance from the light source
    vec3 GlobalAmbient = vec3(0.05, 0.05, 0.05);

	// Only light 1 is affected by light attenuation, and its ambient term is not affected
    vec4 colour;
	colour.rgb = GlobalAmbient + ambient + diffuse/lightAttenuation + ambient2 + diffuse2; 
    colour.a = 1.0;

	 // Add in white specular light after combining the ambient and diffuse light with the texture colour
    gl_FragColor = colour * texture2D( texture, texCoord * 2.0 * TexScale) + 
				   vec4(specular/lightAttenuation + specular2, 1.0f);
}
