#define M_PI 3.1415926

float3 SchlickGGX( float3 lightDir, float3 viewDir, float3 normal, float roughness, float3 F0, out float3 kS )
{
    float  alpha          = roughness * roughness;
    float  alpha2         = alpha * alpha;
    float3 halfDir        = normalize( lightDir + viewDir );
    float  NdotH          = saturate( dot( normal, halfDir ) );
    float3 Fschlick       = F0 + ( 1 - F0 ) * pow( 1 - dot( halfDir, viewDir ), 5.0 );
    float  ggxDenomFactor = NdotH * NdotH * ( alpha2 - 1 ) + 1;
    float  Dggx           = alpha2 / ( ( /*M_PI **/ ggxDenomFactor * ggxDenomFactor ) );
    float  NdotV          = saturate( dot( normal, viewDir ) );
    float  NdotL          = saturate( dot( normal, lightDir ) );
    // Use trick shown http://graphicrants.blogspot.com.au/2013/08/specular-brdf-reference.html to combine Smith GGX shadowing term with Cook-Torrence BRDF denominator
    float  GggxL          = NdotV + sqrt( ( NdotV - NdotV * alpha2 ) * NdotV + alpha2 );
    float  GggxV          = NdotL + sqrt( ( NdotV - NdotL * alpha2 ) * NdotV + alpha2 );

    kS = Fschlick;

    return rcp( GggxL * GggxV ) * Fschlick * Dggx;
}

float3 CalcF0( float metallic, float indexOfRefraction, float3 material )
{
    float f0Root = abs( ( 1.0 - indexOfRefraction ) / ( 1.0 + indexOfRefraction ) );
    float f0     = f0Root * f0Root;

    return lerp( float3( f0, f0, f0 ), material, metallic );
}