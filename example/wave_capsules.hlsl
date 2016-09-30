#include "ps_constants.hlsl"
#include "lighting_common.hlsl"
#include "sdf_common.hlsl"

Texture1D SoundTexture : register( t0 );
SamplerState SoundSampler : register( s0 );

static const float3 MaterialColors[ 2 ] = { float3( 0.8, 0.8, 0.0 ),
                                            float3( 0.5, 0.05, 0.6 ) };

static const float3 PBRs[ 2 ] = { float3( 0.4, 1.5, 0.7 ),
                                  float3( 0.4, 1.460, 0.8 ) };

#define CAPSULE_LENGTH 800
#define CAPSULE_RADIUS 5

float2 SceneDistance( float3 from )
{
    float4 soundSample = SoundTexture.SampleLevel( SoundSampler, ( from.z + CAPSULE_RADIUS + 0.5 * CAPSULE_LENGTH ) / ( CAPSULE_LENGTH + 2 * CAPSULE_RADIUS ), 0 );

    float    cosZ        = cos( ( from.z ) / 8 + ( Time % ( 2 * M_PI ) ) );
    float    sinZ        = sin( ( from.z ) / 8 + ( Time % ( 2 * M_PI ) ) );
    float2x2 rotMat      = { cosZ, -sinZ, sinZ, cosZ };
    float2   rotatedXY   = mul( rotMat, from.xy );
    float3   rotatedFrom = { rotatedXY.x, rotatedXY.y, from.z };

    float2 result = 
        float2( 
            CapsuleSDF( CAPSULE_LENGTH,
                CAPSULE_RADIUS,
                ( rotatedFrom - float3( -9, 1, -0.5  * CAPSULE_LENGTH ) ).zyx ) + soundSample.x * 0.8,
            0 );

    UnionSDF(
        result,
        float2(
            CapsuleSDF( CAPSULE_LENGTH,
                CAPSULE_RADIUS,
                ( rotatedFrom - float3( 9, 1, -0.5  * CAPSULE_LENGTH ) ).zyx ) + soundSample.y * 0.8,
            1 ) );

    return result;
}

float4 main(float4 position : SV_POSITION, float2 texCoord : TEXCOORD0) : SV_Target0
{
    float3 rayDir      = normalize( RayScreenUpperLeft.xyz + texCoord.x * RayScreenRight.xyz + texCoord.y * RayScreenDown.xyz );
    float3 rightRayDir = normalize( RayScreenUpperLeft.xyz + ( texCoord.x + InverseResolution.x ) * RayScreenRight.xyz + texCoord.y * RayScreenDown.xyz );
    float3 downRayDir  = normalize( RayScreenUpperLeft.xyz + texCoord.x * RayScreenRight.xyz + ( texCoord.y + InverseResolution.y ) * RayScreenDown.xyz );

    float pixelRadius = min( length( rightRayDir - rayDir ), length( downRayDir - rayDir ) ) * 0.5f;

    float material = 0;

    float2 distance = float2( 0,0 );

    float t = 2;

    for ( ;; /*int i = 0; i < 65; ++i*/ )
    {
        distance = SceneDistance( EyePosition.xyz + rayDir * t );

        if ( distance.x / t < pixelRadius || t > 425.0f ) break;

        t += distance.x;
    }

    material = distance.y;

    float3 lightDir1 = normalize( float3( /*-0.5f*/0.5 * sin( Time * 0.007 ), 0.8f, -abs( 0.5 * cos( Time * 0.01 ) ) ) );

    if ( t < 425 )
    {
        float3 offset        = float3( 0.002f, 0.0f, 0.0f );
        float3 materialColor = MaterialColors[ ( uint )material ];
        float3 finalPosition = EyePosition.xyz + rayDir * t;

        float3 normal =
            normalize(
                float3( SceneDistance( finalPosition + offset ).x - SceneDistance( finalPosition - offset ).x,
                    SceneDistance( finalPosition + offset.yxz ).x - SceneDistance( finalPosition - offset.yxz ).x,
                    SceneDistance( finalPosition + offset.zyx ).x - SceneDistance( finalPosition - offset.zyx ).x ) );

        float3 litColor = materialColor * 0.04 * ( 0.5 + 0.5 * normal.y );

        float3 pbr      = PBRs[ ( uint )material ];
        float3 F0       = CalcF0( pbr.x, pbr.y, materialColor );

        float3 kS;

        float3 specular = SchlickGGX( lightDir1, -rayDir, normal, pbr.z, F0, kS );
        float3 kD       = saturate( ( 1 - kS ) * ( 1 - pbr.x ) ) * materialColor;
        float  lambert  = saturate( dot( normal, lightDir1 ) );

        litColor += ( kD * lambert + specular );

        return float4( litColor.x, litColor.y, litColor.z, 1 );
    }
    else
    {
        return float4( 0.5, 0.5, 0.9, 1 );
    }
}