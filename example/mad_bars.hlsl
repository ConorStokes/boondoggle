#include "ps_constants.hlsl"
#include "lighting_common.hlsl"
#include "sdf_common.hlsl"

static const float3 MaterialColors[ 16 ] = { float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ) };
static const float3 PBRs[ 16 ] = { float3( 0.7, 1.3, 0.7),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ),
                                   float3( 0.7, 1.3, 0.7 ) };
static const float3 Ambient[ 16 ] = { float3( 1, 0.01, 0.65 ) * 0.6,
                                      float3( 0.01, 0.82, 1 ) * 0.6,
                                      float3( 1, 0.26, 0.01 ) * 0.6,
                                      float3( 1, 0.5, 0.01 ) * 0.6,
                                      float3( 0.17, 0.95, 0.05 ) * 0.6,
                                      float3( 0.01, 0.16, 1 ) * 0.6,
                                      float3( 0.85, 0.82, 0.5 ) * 0.6,
                                      float3( 1, 0.97, 0.01 ) * 0.6,
                                      float3( 1, 0.01, 0.65 ) * 0.6,
                                      float3( 0.01, 0.82, 1 ) * 0.6,
                                      float3( 1, 0.26, 0.01 ) * 0.6,
                                      float3( 1, 0.5, 0.01 ) * 0.6,
                                      float3( 0.17, 0.95, 0.05 ) * 0.6,
                                      float3( 0.01, 0.16, 1 ) * 0.6,
                                      float3( 0.85, 0.82, 0.5 ) * 0.6,
                                      float3( 1, 0.97, 0.01 ) * 0.6 };


float2 SceneDistance( float3 from )
{
    float  scale           = 0.25f / NoiseFloorDbSPL;

    float  bucketIndex     = 15 - ( floor( abs( from.x ) / 10 ) + ( floor( abs( from.z ) / 10 ) ) ) % 16;// + abs( floor( from.x / 2 ) + floor( from.z / 2 ) ) * 39 ) % 16 );

    float2 soundBucket     = SoundFrequencyBuckets[ (uint)bucketIndex ];
    float  normalizedSound = 1 + ( ( from.x < 0 ? -soundBucket.x : -soundBucket.y ) * scale * 4 );

    float3 repeatedFrom = { abs( from.x ) % 10, from.y, abs( from.z ) % 10 };

    float lowFrequency;
    float midLowFrequency;
    float midHighFrequency;
    float highFrequency;

    float    cosY      = cos( from.y * 0.5 + bucketIndex );
    float    sinY      = sin( from.y * 0.5 + bucketIndex );
    float2x2 rotMat    = { cosY, -sinY, sinY, cosY };
    float2   rotatedXz = mul( rotMat, repeatedFrom.xz - float2( 5, 5 ) ) + float2( 5, 5 );

    float3 rotatedFrom = float3( rotatedXz.x, repeatedFrom.y, rotatedXz.y );

    if ( from.x < 0 )
    {
        lowFrequency = 1 - ( SoundFrequencyBuckets[ 0 ].x + SoundFrequencyBuckets[ 1 ].x + SoundFrequencyBuckets[ 2 ].x + SoundFrequencyBuckets[ 3 ].x ) * scale;
        midLowFrequency = 1 - ( SoundFrequencyBuckets[ 4 ].x + SoundFrequencyBuckets[ 5 ].x + SoundFrequencyBuckets[ 6 ].x + SoundFrequencyBuckets[ 7 ].x ) * scale;
        midHighFrequency = 1 - ( SoundFrequencyBuckets[ 8 ].x + SoundFrequencyBuckets[ 9 ].x + SoundFrequencyBuckets[ 10 ].x + SoundFrequencyBuckets[ 11 ].x ) * scale;
        highFrequency = 1 - ( SoundFrequencyBuckets[ 12 ].x + SoundFrequencyBuckets[ 13 ].y + SoundFrequencyBuckets[ 14 ].x + SoundFrequencyBuckets[ 15 ].x ) * scale;
    }
    else
    {
        lowFrequency = 1 - ( SoundFrequencyBuckets[ 0 ].y + SoundFrequencyBuckets[ 1 ].y + SoundFrequencyBuckets[ 2 ].y + SoundFrequencyBuckets[ 3 ].y ) * scale;
        midLowFrequency = 1 - ( SoundFrequencyBuckets[ 4 ].y + SoundFrequencyBuckets[ 5 ].y + SoundFrequencyBuckets[ 6 ].y + SoundFrequencyBuckets[ 7 ].y ) * scale;
        midHighFrequency = 1 - ( SoundFrequencyBuckets[ 8 ].y + SoundFrequencyBuckets[ 9 ].y + SoundFrequencyBuckets[ 10 ].y + SoundFrequencyBuckets[ 11 ].y ) * scale;
        highFrequency = 1 - ( SoundFrequencyBuckets[ 12 ].y + SoundFrequencyBuckets[ 13 ].y + SoundFrequencyBuckets[ 14 ].y + SoundFrequencyBuckets[ 15 ].y ) * scale;
    }

    float distortion = lowFrequency * lowFrequency * 0.8 * sin( 1.25 * from.x + Time ) * sin( 1.25 * from.y + Time * 0.9f ) * sin( 1.25 * from.z + Time * 1.1f ) +
                        midLowFrequency * midLowFrequency * midLowFrequency * 0.4 * sin( 3.0 * from.x + Time * 1.5 ) * sin( 3.0 * from.y + Time * 1.3f ) * sin( 3.0 * from.z + -Time * 1.6f ) +
                        midHighFrequency * midHighFrequency * midHighFrequency * midHighFrequency * 0.5 * sin( 5.7 * from.x + Time * 2.5 ) * sin( 5.7 * from.y + -Time * 2.3f ) * sin( 5.7 * from.z + Time * 2.6f ) +
                        highFrequency * highFrequency * highFrequency * highFrequency * highFrequency * 0.7 * sin( 9.2 * from.x + -Time * 4.5 ) * sin( 9.2 * from.y + Time * 4.3f ) * sin( 9.2 * from.z + Time * 4.6f ) * ( from.x < 0 ? -1 : 1 );
    
    float normalizedSoundCubed = normalizedSound * normalizedSound * normalizedSound;

    float2 result = float2(
        RoundBoxSDF(
            float3( 5, 0, 5 ),
            float3( 0.5, 12 * normalizedSoundCubed, 0.5 ),
            0.45,
            rotatedFrom ) + distortion,
        bucketIndex );
  
    return result;
}

float4 main(float4 position : SV_POSITION, float2 texCoord : TEXCOORD0) : SV_Target0
{
    float3 movedEyePosition = EyePosition.xyz + float3( 0, 0, Time );

    float3 rayDir = normalize( RayScreenUpperLeft.xyz + texCoord.x * RayScreenRight.xyz + texCoord.y * RayScreenDown.xyz );

    float t = 3.7f;

    float2 actualDistance = float2( 50, 0 );
    float3 pbr           = float3( 0, 0, 0 );
    float3 materialColor = float3( 0, 0, 0 );

    float3 rightRayDir = normalize( RayScreenUpperLeft.xyz + ( texCoord.x + InverseResolution.x ) * RayScreenRight.xyz + texCoord.y * RayScreenDown.xyz );
    float3 downRayDir = normalize( RayScreenUpperLeft.xyz + texCoord.x * RayScreenRight.xyz + ( texCoord.y + InverseResolution.y ) * RayScreenDown.xyz );

    float pixelRadius = min( length( rightRayDir - rayDir ), length( downRayDir - rayDir ) ) * 0.5f;

    float material = 0;

    float2 distance = float2(0,0);

    float3 glow = { 0, 0, 0 };
    
 //   float iterations = 0;
    float previousDistance = 3.7;

    for ( ;; )
    {
        float3 from = movedEyePosition.xyz + rayDir * t;

        distance = SceneDistance( from );

        glow += previousDistance * Ambient[ ( uint )distance.y ] / ( 1 + distance.x * distance.x );

//        iterations += 1;

        if (distance.x / t < pixelRadius || t > 150.0f || abs( from.y ) > 13.5 ) break;

        t += distance.x;
    }

    float3 finalPosition = movedEyePosition.xyz + rayDir * t;

    glow *= 1.5 / 150.0;
   // glow /= iterations;
  //  glow *= 2.5 * ( t > 150.0f || abs( finalPosition ) > 13.5 ? 150 : t ) / 150;

    material = distance.y;
    
    float3 endColor;

    float3 lightDir1 = normalize( float3( /*-0.5f*/0.5 * sin( Time * 0.007 ), 0.8f, -abs( 0.5 * cos( Time * 0.01 ) ) ) );

    float  sunAmount  = saturate( dot( lightDir1, rayDir ) );
    float3 soundColor = float3( 0.43 + 0.3 * cos( Time * 0.1 ), 0.45 + 0.3 * sin( 2.0 * M_PI * SoundRMS.x ), 0.47 + 0.3 * cos( 2.0 * M_PI * SoundRMS.y ) );
    float3 fogColor   = lerp( soundColor, float3( 1.0, 0.9, 0.75 ), pow( sunAmount, 15 ) );

    if ( t < 150.0f && abs( finalPosition.y ) < 13.5 )
    {
        float3 offset = float3( 0.005f, 0.0f, 0.0f );

        float3 normal;
        
        normal =
            normalize(
                float3( SceneDistance( finalPosition + offset ).x - SceneDistance( finalPosition - offset ).x,
                    SceneDistance( finalPosition + offset.yxz ).x - SceneDistance( finalPosition - offset.yxz ).x,
                    SceneDistance( finalPosition + offset.zyx ).x - SceneDistance( finalPosition - offset.zyx ).x ) );
        
        float3 materialColor = MaterialColors[ ( uint )material ];               
        float3 litColor      = Ambient[ ( uint )material ];

        {
            float3 pbr      = PBRs[ ( uint )material ];
            float3 F0       = CalcF0( pbr.x, pbr.y, materialColor );

            float3 kS;

            float3 specular = SchlickGGX( lightDir1, -rayDir, normal, pbr.z, F0, kS );
            float3 kD       = saturate( ( 1 - kS ) * ( 1 - pbr.x ) ) * materialColor;
            float  lambert  = saturate( dot( normal, lightDir1 ) );

            litColor += ( kD * lambert + specular ) * float3( 1.0, 0.9, 0.75 ) * 0.8;
        }

        float fogFactor = 1.0 - exp( -pow( t, 2.1 ) * 0.000125 );

        return float4( lerp( litColor, fogColor, fogFactor ) + glow, 1 );
    }
    else
    {        
        return float4( fogColor + glow, 1 );
    }
}