struct VS_OUTPUT
{
    float4 pos      : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

Texture2DMS<float4> gPosition;
Texture2DMS<float4> gNormal;
Texture2DMS<float4> gAmbient;
Texture2DMS<float4> gDiffuse;
Texture2DMS<float4> gSpecular;
TextureCube<float> LightCubeShadowMap;

cbuffer Params
{
    float4x4 View[6];
    float4x4 Projection;
};

cbuffer ShadowParams
{
    float s_near;
    float s_far;
    float s_size;
};

SamplerState g_sampler : register(s0);
SamplerComparisonState LightCubeShadowComparsionSampler : register(s1);

float _vectorToDepth(float3 vec, float n, float f)
{
    float3 AbsVec = abs(vec);
    float LocalZcomp = max(AbsVec.x, max(AbsVec.y, AbsVec.z));

    float NormZComp = (f + n) / (f - n) - (2 * f * n) / (f - n) / LocalZcomp;
    return NormZComp;
}

float _sampleCubeShadowHPCF(float3 L, float3 vL)
{
    float sD = _vectorToDepth(vL, s_near, s_far);
    return LightCubeShadowMap.SampleCmpLevelZero(LightCubeShadowComparsionSampler, float3(L.xy, -L.z), sD).r;
}

float _sampleCubeShadowPCFSwizzle3x3(float3 L, float3 vL)
{
    float sD = _vectorToDepth(vL, s_near, s_far);

    float3 forward = float3(L.xy, -L.z);
    float3 right = float3(forward.z, -forward.x, forward.y);
    right -= forward * dot(right, forward);
    right = normalize(right);
    float3 up = cross(right, forward);

    float tapoffset = (1.0f / s_size);

    right *= tapoffset;
    up *= tapoffset;

    float3 v0;
    v0.x = LightCubeShadowMap.SampleCmpLevelZero(LightCubeShadowComparsionSampler, forward - right - up, sD).r;
    v0.y = LightCubeShadowMap.SampleCmpLevelZero(LightCubeShadowComparsionSampler, forward - up, sD).r;
    v0.z = LightCubeShadowMap.SampleCmpLevelZero(LightCubeShadowComparsionSampler, forward + right - up, sD).r;
	
    float3 v1;
    v1.x = LightCubeShadowMap.SampleCmpLevelZero(LightCubeShadowComparsionSampler, forward - right, sD).r;
    v1.y = LightCubeShadowMap.SampleCmpLevelZero(LightCubeShadowComparsionSampler, forward, sD).r;
    v1.z = LightCubeShadowMap.SampleCmpLevelZero(LightCubeShadowComparsionSampler, forward + right, sD).r;

    float3 v2;
    v2.x = LightCubeShadowMap.SampleCmpLevelZero(LightCubeShadowComparsionSampler, forward - right + up, sD).r;
    v2.y = LightCubeShadowMap.SampleCmpLevelZero(LightCubeShadowComparsionSampler, forward + up, sD).r;
    v2.z = LightCubeShadowMap.SampleCmpLevelZero(LightCubeShadowComparsionSampler, forward + right + up, sD).r;
	
	
    return dot(v0 + v1 + v2, .1111111f);
}


// UE4: https://github.com/EpicGames/UnrealEngine/blob/release/Engine/Shaders/ShadowProjectionCommon.usf
static const float2 DiscSamples5[] =
{ // 5 random points in disc with radius 2.500000
    float2(0.000000, 2.500000),
	float2(2.377641, 0.772542),
	float2(1.469463, -2.022543),
	float2(-1.469463, -2.022542),
	float2(-2.377641, 0.772543),
};

float _sampleCubeShadowPCFDisc5(float3 L, float3 vL)
{
    float3 SideVector = normalize(cross(L, float3(0, 0, 1)));
    float3 UpVector = cross(SideVector, L);

    SideVector *= 1.0 / s_size;
    UpVector *= 1.0 / s_size;
	
    float sD = _vectorToDepth(vL, s_near, s_far);

    float3 nlV = float3(L.xy, -L.z);

    float totalShadow = 0;

	[UNROLL]
    for (int i = 0; i < 5; ++i)
    {
        float3 SamplePos = nlV + SideVector * DiscSamples5[i].x + UpVector * DiscSamples5[i].y;
        totalShadow += LightCubeShadowMap.SampleCmpLevelZero(
				LightCubeShadowComparsionSampler,
				SamplePos,
				sD);
    }
    totalShadow /= 5;

    return totalShadow;

}

#define USE_CAMMA_RT
#define USE_CAMMA_TEX

float4 getTexture(Texture2DMS<float4> _texture, float2 _tex_coord, int ss_index, bool _need_gamma = false)
{
    float3 gbufferDim;
    _texture.GetDimensions(gbufferDim.x, gbufferDim.y, gbufferDim.z);
    float2 texcoord = _tex_coord * float2(gbufferDim.xy);
    float4 _color = _texture.Load(texcoord, ss_index);
#ifdef USE_CAMMA_TEX
    if (_need_gamma)
        _color = float4(pow(abs(_color.rgb), 2.2), _color.a);
#endif
    return _color;
}

cbuffer ConstantBuffer
{
    float3 lightPos;
    float3 viewPos;
};

float3 CalcLighting(float3 fragPos, float3 normal, float3 ambient,  float3 diffuse, float3 specular_base, float shininess)
{
    float3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    diffuse *= diff;

    float3 viewDir = normalize(viewPos - fragPos);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(saturate(dot(viewDir, reflectDir)), shininess);
    float3 specular = specular_base * spec;

    float3 vL = fragPos - lightPos;
    float3 L = normalize(vL);

    float shadow = _sampleCubeShadowPCFDisc5(L, vL);
    float3 hdrColor = float3(ambient + diffuse * shadow + specular * shadow);
    return hdrColor;
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
    float4 gamma4 = float4(1.0/2.2, 1.0 / 2.2, 1.0 / 2.2, 1);

    float3 lighting = 0;
    for (int i = 0; i < 8; ++i)
    {
        float3 fragPos = getTexture(gPosition, input.texCoord, i).rgb;
        float3 normal = getTexture(gNormal, input.texCoord, i).rgb;
        float3 ambient = getTexture(gAmbient, input.texCoord, i).rgb;
        float3 diffuse = getTexture(gDiffuse, input.texCoord, i).rgb;
        float3 specular_base = getTexture(gSpecular, input.texCoord, i).rgb;
        float shininess = getTexture(gSpecular, input.texCoord, i).a;
        lighting += CalcLighting(fragPos, normal, ambient, diffuse, specular_base, shininess);
    }
    lighting /= 8;
 
    return pow(float4(lighting, 1.0), gamma4);
}