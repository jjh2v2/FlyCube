struct VS_OUTPUT
{
    float4 pos       : SV_POSITION;
    float3 fragPos   : POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float2 texCoord  : TEXCOORD;
    float3 fragPosView : POSITIONVIEW;
    float3 normalView    : NORMALVIEW;
};

Texture2D normalMap;
Texture2D alphaMap;
Texture2D ambientMap;
Texture2D diffuseMap;
Texture2D specularMap;
Texture2D glossMap;

SamplerState g_sampler;

cbuffer Material
{
    float3 material_ambient;
    float3 material_diffuse;
    float3 material_specular;
    float material_shininess;
};

cbuffer Light
{
    float3 light_ambient;
    float3 light_diffuse;
    float3 light_specular;
};

float4 getTexture(Texture2D _texture, SamplerState _sample, float2 _tex_coord, bool _need_gamma = false)
{
    float4 _color = _texture.Sample(_sample, _tex_coord);
    if (_need_gamma)
        _color = float4(pow(abs(_color.rgb), 2.2), _color.a);
    return _color;
}

struct PS_OUT
{
    float4 gPosition : SV_Target0;
    float3 gNormal   : SV_Target1;
    float3 gAmbient  : SV_Target2;
    float3 gDiffuse  : SV_Target3;
    float4 gSpecular : SV_Target4;
    float3 gPositionView : SV_Target5;
    float3 gNormalView   : SV_Target6;
};

float3 CalcBumpedNormal(VS_OUTPUT input)
{
    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent);
    T = normalize(T - dot(T, N) * N);
    float3 B = normalize(cross(T, N));
    float3x3 tbn = float3x3(T, B, N);
    float3 normal = normalMap.Sample(g_sampler, input.texCoord).rgb;
    normal = normalize(2.0 * normal - 1.0);
    normal = normalize(mul(normal, tbn));
    return normal;
}

bool HasTexture(Texture2D _texture)
{
    uint width, height;
    _texture.GetDimensions(width, height);
    return width > 0 && height > 0;
}

PS_OUT main(VS_OUTPUT input)
{
    if (HasTexture(alphaMap))
    {
        if (getTexture(alphaMap, g_sampler, input.texCoord).r < 0.5)
            discard;
    }

    PS_OUT output;
    output.gPosition = float4(input.fragPos.xyz, input.pos.z);
    output.gPositionView = input.fragPosView;
    output.gNormalView = normalize(input.normalView);

    if (HasTexture(normalMap))
        output.gNormal = CalcBumpedNormal(input);
    else
        output.gNormal = normalize(input.normal);

    output.gAmbient = light_ambient;
    if (HasTexture(ambientMap))
        output.gAmbient *= getTexture(ambientMap, g_sampler, input.texCoord, true).rgb;
    else
        output.gAmbient *= material_ambient;

    output.gDiffuse = light_diffuse;
    if (HasTexture(diffuseMap))
        output.gDiffuse *= getTexture(diffuseMap, g_sampler, input.texCoord, true).rgb;
    else
        output.gDiffuse *= material_diffuse;

    output.gSpecular.rgb = light_specular;
    if (HasTexture(specularMap))
        output.gSpecular.rgb *= getTexture(specularMap, g_sampler, input.texCoord, true).rgb;
    else
        output.gSpecular.rgb *= material_specular;

    if (HasTexture(glossMap))
        output.gSpecular.a = getTexture(glossMap, g_sampler, input.texCoord, false).r;
    else
        output.gSpecular.a = material_shininess;

    return output;
}
