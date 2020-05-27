#version 450 core

layout (location = 0) out vec4 color;

struct PointLight{
	vec4 position;
	vec4 color;
	vec4 falloff;
};

struct DirectionalLight{
	vec4 direction;
	vec4 color;
};

struct SpotLight{
	vec4 position;
	vec4 direction;
	vec4 color;
	vec4 falloff;
};

uniform vec4 flatColor;
uniform vec4 material;
uniform sampler2DShadow shadowMap;

layout (binding = 0 ) uniform sampler2D albedoTex;
layout (binding = 1 ) uniform sampler2D materialTex;
layout (binding = 2 ) uniform sampler2D normalTex;
layout (binding = 3 ) uniform samplerCube cubemapTex;

layout (binding = 1, std140) uniform LightData{
	vec4 ambientColor;
	uvec4 lightCounts;

	PointLight[5] pointLights;

	DirectionalLight[5] directionalLights;

	SpotLight[3] spotLights;
} lights;

in VertexData
{
	vec3 eyeDir;
	vec3 worldPos;
	vec2 texCoord;
	mat3 TBN;
    vec4 shadow_Position;
} vertex;

vec3 reflectDir;
vec3 lightDiffuse;
vec3 lightSpecular;
vec3 eyeDir;
vec3 worldNormal;

//Declarations
void doPointLight(PointLight light);
void doDirectionalLight(DirectionalLight light);
void doSpotLight(SpotLight light);

void main()
{
	eyeDir = normalize(vertex.eyeDir);
	worldNormal = normalize(vertex.TBN[2]);

	vec3 textureNormal = normalize(texture( normalTex, vertex.texCoord ).rgb * 2.0 - 1.0);
	worldNormal = vertex.TBN * textureNormal;

	reflectDir = reflect(eyeDir,worldNormal);
	
	lightDiffuse = vec3(0);
	lightSpecular = vec3(0);

	for(int i=0;i<lights.lightCounts.x;i++){ //do PointLights
		doPointLight(lights.pointLights[i]);
	}

	for(int i=0;i<lights.lightCounts.y;i++){ //do DirectionalLights
		doDirectionalLight(lights.directionalLights[i]);
	}

	for(int i=0;i<lights.lightCounts.z;i++){ //do SpotLight
		doSpotLight(lights.spotLights[i]);
	}

	float shadow = texture(shadowMap, (vertex.shadow_Position.xy/vertex.shadow_Position.w)).z  <  (vertex.shadow_Position.z-0.5/vertex.shadow_Position.w) ? 1.0 : 0.0;
	color = vec4(flatColor.xyz * texture(albedoTex, vertex.texCoord).xyz * (lights.ambientColor.xyz + (1.0 - shadow)  * material.x + lightDiffuse * material.y) + (lightSpecular + texture(cubemapTex, -reflectDir).xyz * flatColor.w) * material.z * texture(materialTex,vertex.texCoord).x,1);
}


void doPointLight(PointLight light){
	vec3 lightDir = light.position.xyz - vertex.worldPos;
	float dist = length(lightDir);
	if(dist > light.falloff.w) return;
	lightDir /= dist;

	float falloff = 1/(light.falloff.x + light.falloff.y * dist + light.falloff.z * dist * dist);

	float facDiff = max(dot(worldNormal,lightDir),0);
	float facSpec = facDiff>0 ? max(-dot(reflectDir,lightDir),0) : 0;

	lightDiffuse += facDiff * light.color.xyz * falloff; //Diffuse
	lightSpecular += facDiff * pow(facSpec,material.w) * light.color.xyz * falloff; //Specular
}

void doDirectionalLight(DirectionalLight light){
	float facDiff = max(dot(worldNormal,-light.direction.xyz),0);
	float facSpec = facDiff>0 ? max(-dot(reflectDir,-light.direction.xyz),0) : 0;

	lightDiffuse += facDiff * light.color.xyz; //Diffuse
	lightSpecular += facDiff * pow(facSpec,material.w) * light.color.xyz; //Specular
}

void doSpotLight(SpotLight light){
	vec3 lightDir = light.position.xyz - vertex.worldPos;
	float dist = length(lightDir);
	if(dist > light.falloff.w) return;
	lightDir /= dist;

	float radial = dot(-light.direction.xyz,lightDir);

	float falloff = 1/(light.falloff.x + light.falloff.y * dist + light.falloff.z * dist * dist);
	falloff *= smoothstep(light.direction.w,light.position.w,radial);

	float facDiff = max(dot(worldNormal,lightDir),0);
	float facSpec = facDiff>0 ? max(-dot(reflectDir,lightDir),0) : 0;

	lightDiffuse += facDiff * light.color.xyz * falloff; //Diffuse
	lightSpecular += facDiff * pow(facSpec,material.w) * light.color.xyz * falloff; //Specular
}