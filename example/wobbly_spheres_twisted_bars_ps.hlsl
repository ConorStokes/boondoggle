#include "ps_constants.hlsl"
#include "lighting_common.hlsl"
#include "sdf_common.hlsl"

Texture2D GroundTexture : register( t0 );
SamplerState GroundSampler : register( s0 );

static const float3 MaterialColors[ 19 ] = { float3( 0.45, 0.01, 0.01 ), 
                                            float3( 0.02, 0.02, 0.5 ), 
                                            float3( 0.025, 0.025, 0.025 ),
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
                                            float3( 0.2, 0.2, 0.2 ),
                                            float3( 0.2, 0.2, 0.2 ) };
static const float3 PBRs[ 19 ] = { float3( 0.25, 1.3, 0.45 ), 
                                  float3( 0.1, 1.460, 0.55 ), 
                                  float3( 0, 1.460, 0.4 ),
                                  float3( 0.7, 1.3, 0.7),
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
static const float3 Ambient[ 19 ] = { float3( 0.025, 0.025, 0.025 ),
                                     float3( 0.025, 0.025, 0.025 ),
                                     float3( 0.005, 0.005, 0.005 ),
                                     float3( 1, 0.01, 0.65 ) * 0.6,
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

#define GROUND_PLANE -8

float2 SceneDistance( float3 from, bool testSpheres, bool withATwist )
{
    float scale = 0.25f / NoiseFloorDbSPL;

    uint bucketIndex = 15 - ( uint )( floor( abs( from.x ) / 10 ) + ( floor( abs( from.z ) / 10 ) ) ) % 16;// + abs( floor( from.x / 2 ) + floor( from.z / 2 ) ) * 39 ) % 16 );

    float normalizedSound = 1 + ( ( from.x < 0 ? -SoundFrequencyBuckets[ bucketIndex ].x : -SoundFrequencyBuckets[ bucketIndex ].y ) * scale * 4 );

    float3 rotatedFrom;
    float3 repeatedFrom = { abs( from.x ) % 10, from.y, abs( from.z ) % 10 };

    if ( withATwist )
    {
        float    cosY         = cos( from.y * ( (float)bucketIndex + 4 ) / 8 );
        float    sinY         = sin( from.y * ( (float)bucketIndex + 4 ) / 8 );
        float2x2 rotMat       = { cosY, -sinY, sinY, cosY };
        float2   rotatedXz    = mul( rotMat, repeatedFrom.xz - float2( 5, 5 ) ) + float2( 5, 5 );
        
        rotatedFrom  = float3( rotatedXz.x, repeatedFrom.y, rotatedXz.y );
    }
    else
    {
        rotatedFrom = repeatedFrom;
    }

    float2 result = float2(
        RoundBoxSDF(
            float3( 5, GROUND_PLANE, 5 ),
            float3( 0.4, 9 * normalizedSound * normalizedSound * normalizedSound, 0.4 ),
            0.35,
            rotatedFrom ),
        3 + bucketIndex );
 
    if ( testSpheres )
    {
        float2 lowFrequency = 1 - ( SoundFrequencyBuckets[ 0 ].xy + SoundFrequencyBuckets[ 1 ].xy + SoundFrequencyBuckets[ 2 ].xy + SoundFrequencyBuckets[ 3 ].xy ) * scale;
        float2 midLowFrequency = 1 - ( SoundFrequencyBuckets[ 4 ].xy + SoundFrequencyBuckets[ 5 ].xy + SoundFrequencyBuckets[ 6 ].xy + SoundFrequencyBuckets[ 7 ].xy ) * scale;
        float2 midHighFrequency = 1 - ( SoundFrequencyBuckets[ 8 ].xy + SoundFrequencyBuckets[ 9 ].xy + SoundFrequencyBuckets[ 10 ].xy + SoundFrequencyBuckets[ 11 ].xy ) * scale;
        float2 highFrequency = 1 - ( SoundFrequencyBuckets[ 12 ].xy + SoundFrequencyBuckets[ 13 ].xy + SoundFrequencyBuckets[ 14 ].xy + SoundFrequencyBuckets[ 15 ].xy ) * scale;

        float2 distortion = lowFrequency * lowFrequency * 0.8 * sin( 1.25 * from.x + Time ) * sin( 1.25 * from.y + Time * 0.9f ) * sin( 1.25 * from.z + Time * 1.1f ) +
            midLowFrequency * midLowFrequency * midLowFrequency * 0.4 * sin( 3.0 * from.x + Time * 1.5 ) * sin( 3.0 * from.y + Time * 1.3f ) * sin( 3.0 * from.z + -Time * 1.6f ) +
            midHighFrequency * midHighFrequency * midHighFrequency * midHighFrequency * 0.5 * sin( 5.7 * from.x + Time * 2.5 ) * sin( 5.7 * from.y + -Time * 2.3f ) * sin( 5.7 * from.z + Time * 2.6f ) +
            highFrequency * highFrequency * highFrequency * highFrequency * highFrequency * 0.7 * sin( 9.2 * from.x + -Time * 4.5 ) * sin( 9.2 * from.y + Time * 4.3f ) * sin( 9.2 * from.z + Time * 4.6f ) * float2( -1, 1 );

        UnionSDF( result, float2( SphereSDF( float3( 3, -5, 8 ), 2, from ) + distortion.y, 1 ) );

        UnionSDF( result, float2( SphereSDF( float3( -3, -5, 8 ), 2, from ) + distortion.x, 0 ) );
    }

    return result;
}

//float2 CastRayRelaxed( float3 origin, float3 rayDir, float pixelRadius, float minT, float maxT )
//{
//    float functionSign   = SceneDistance( EyePosition.xyz ).x < 0 ? -1 : 1;
//    float omega          = 1.2;
//    float previousRadius = 0;
//    float candidateError = maxT;
//    float candidateT     = minT;
//    float stepLength     = 0;
//    float t              = minT;
//    float candidateMat   = 0;
//    float signedRadius   = 0;
//
//    for ( int i = 0; i < 70; ++i )
//    {
//        float2 distance     = SceneDistance( origin + rayDir * t );
//        signedRadius = distance.x * functionSign;
//        float  radius       = abs( signedRadius );
//        bool   sorFail      = omega > 1 && ( radius + previousRadius ) < stepLength;
//
//        if ( sorFail )
//        {
//            stepLength -= omega * stepLength;
//            omega       = 1;
//        }
//        else
//        {
//            stepLength = signedRadius * omega;
//        }
//
//        previousRadius = radius;
//
//        float error = radius / t;
//
//        if ( !sorFail && error < candidateError )
//        {
//            candidateT     = t;
//            candidateError = error;
//            candidateMat   = distance.y;
//        }
//
//        if ( !sorFail && ( error < pixelRadius || signedRadius < 0 || t > maxT || ( origin.y + rayDir.y * t ) < GROUND_PLANE ) )
//            break;
//
//        t += stepLength;
//    }
//    
//    if ( t >= maxT || ( candidateError > pixelRadius && candidateMat >= 2 ) || ( origin.y + rayDir.y * t ) < GROUND_PLANE )
//    {
//        return float2( ( origin.y + rayDir.y * t ) < GROUND_PLANE ? t : maxT, 2 );
//    } 
//    else
//    {
//        return float2( candidateT, candidateMat );
//    }
//}

float CastShadowRay( float3 origin, float3 rayDir, float maxT, float k, bool testSpheres )
{
    float t   = 0.05;
    float res = 1.0;

    while ( t < maxT )
    {
        float distance = SceneDistance( origin + rayDir * t, testSpheres, true ).x;
    
        if ( distance < 0.01 ) 
            return 0;

//        if ( distance < 0.04 ) break;

        res = min( res, k * distance / t );
        t  += distance;
        //    iterations = (float)( i + 1 ) / 55.0f;
    }

    return res;
}

float4 main(float4 position : SV_POSITION, float2 texCoord : TEXCOORD0) : SV_Target0
{
    //return float4( texCoord.x, texCoord.y, abs( sin( Time / 10.0f ) ), 1.0f );
    float3 rayDir = normalize( RayScreenUpperLeft.xyz + texCoord.x * RayScreenRight.xyz + texCoord.y * RayScreenDown.xyz );

    float t = 3.0f;

    float2 actualDistance = float2( 50, 0 );
 //   float iterations = 0.0f;

   // float sinTimeX = abs( sin(Time / 10) ) * 1.0f + 2.0f;
    float3 pbr           = float3( 0, 0, 0 );
    float3 materialColor = float3( 0, 0, 0 );

    float3 movedEyePosition = float3( EyePosition.x, EyePosition.y - 8, EyePosition.z );

    float3 rightRayDir = normalize( RayScreenUpperLeft.xyz + ( texCoord.x + InverseResolution.x ) * RayScreenRight.xyz + texCoord.y * RayScreenDown.xyz );
    float3 downRayDir = normalize( RayScreenUpperLeft.xyz + texCoord.x * RayScreenRight.xyz + ( texCoord.y + InverseResolution.y ) * RayScreenDown.xyz );

    float pixelRadius = min( length( rightRayDir - rayDir ), length( downRayDir - rayDir ) ) * 0.5f;

    float material = 0;

    float2 distance = float2(0,0);

    for ( ;; /*int i = 0; i < 65; ++i*/)
    {
        distance = SceneDistance( movedEyePosition.xyz + rayDir * t, true, true );

        //actualDistance = distance.x < actualDistance.x ? distance : actualDistance;
        
        if (distance.x / t < pixelRadius || t > 200.0f || ( movedEyePosition.y + rayDir.y * t ) < GROUND_PLANE ) break;

        t += distance.x;
        //    iterations = (float)( i + 1 ) / 55.0f;
    }


    material = distance.y;

   // float2 rayResult = CastRayRelaxed( EyePosition.xyz, rayDir, pixelRadius, 3, 110 );

//    float t = rayResult.x;
//    float material = actualDistance.y;
    
    float3 endColor;

    float3 lightDir1 = normalize( float3( /*-0.5f*/0.5 * sin( Time * 0.007 ), 0.8f, -abs( 0.5 * cos( Time * 0.01 ) ) ) );

    float sunAmount = saturate( dot( lightDir1, rayDir ) );
    float3 fogSound = float3( 0.53 + 0.3 * cos( Time * 0.1 ), 0.55 + 0.3 * sin( 2.0 * M_PI * SoundRMS.x ), 0.57 + 0.3 * cos( 2.0 * M_PI * SoundRMS.y ) );
    float3 fogColor = lerp( fogSound, float3( 1.0, 0.9, 0.75 ), pow( sunAmount, 15 ) );

    if ( t < 200.0f )
    {
        float3 offset = float3( 0.002f, 0.0f, 0.0f );

        float3 normal;

        float3 finalPosition = movedEyePosition.xyz + rayDir * t;

        if ( finalPosition.y < GROUND_PLANE )
        {
            t             = -( movedEyePosition.y - GROUND_PLANE ) / rayDir.y;
            finalPosition = movedEyePosition.xyz + rayDir * t;
            normal        = float3( 0, 1, 0 );
            material      = 2;
        }
        else
        {
            normal =
                normalize(
                    float3( SceneDistance( finalPosition + offset, true, true ).x - SceneDistance( finalPosition - offset, true, true ).x,
                        SceneDistance( finalPosition + offset.yxz, true, true ).x - SceneDistance( finalPosition - offset.yxz, true, true ).x,
                        SceneDistance( finalPosition + offset.zyx, true, true ).x - SceneDistance( finalPosition - offset.zyx, true, true ).x ) );
        }
        
        float3 materialColor = material != 2 ? MaterialColors[ ( uint )material ] : GroundTexture.Sample(GroundSampler, finalPosition.xz / 4).xyz;

        float3 litColor = Ambient[ ( uint )material ] + ( ( material <= 2 ) ? materialColor * 0.1 * ( 0.5 + 0.5 * normal.y ) : float3( 0, 0, 0 ) );

        float shadow = 1;

        if ( material <= 2 )
        {
            shadow = CastShadowRay( finalPosition, lightDir1, 10, 24, material == 2 );
        }

        if ( shadow > 0 )
        {
            float3 pbr      = PBRs[ ( uint )material ];
            float3 F0       = CalcF0( pbr.x, pbr.y, materialColor );

            float3 kS;

            float3 specular = SchlickGGX( lightDir1, -rayDir, normal, pbr.z, F0, kS );
            float3 kD       = saturate( ( 1 - kS ) * ( 1 - pbr.x ) ) * materialColor;
            float  lambert  = saturate( dot( normal, lightDir1 ) );

            litColor += ( kD * lambert + specular ) * float3( 1.0, 0.9, 0.75 ) * 0.8 * shadow;
        }
        
        if ( material == 2 )
        {
            float2 blockDistance = SceneDistance( finalPosition, false, true );

            litColor += 0.7 * materialColor * Ambient[ ( uint )blockDistance.y ] * saturate( ( 1.05 / ( 1 + blockDistance.x * blockDistance.x ) - 0.05 ) );
        }
        else if ( material < 2 )
        {
            float2 blockDistance = SceneDistance( finalPosition, false, true );

            float3 lightDir2 =
                normalize(
                    float3( SceneDistance( finalPosition - offset, false, false ).x - SceneDistance( finalPosition + offset, false, false ).x,
                        SceneDistance( finalPosition - offset.yxz, false, false ).x - SceneDistance( finalPosition + offset.yxz, false, false ).x,
                        SceneDistance( finalPosition - offset.zyx, false, false ).x - SceneDistance( finalPosition + offset.zyx, false, false ).x ) );

            float3 pbr      = PBRs[ ( uint )material ];
            float3 F0       = CalcF0( pbr.x, pbr.y, materialColor );

            float3 kS;

            float3 specular = SchlickGGX( lightDir2, -rayDir, normal, 0.5, F0, kS );
            float3 kD       = saturate( ( 1 - kS ) * ( 1 - pbr.x ) ) * materialColor;

            float  lambert  = saturate( dot( normal, lightDir2 ) );

            litColor += ( kD * lambert + specular ) * Ambient[ ( uint )blockDistance.y ] * saturate( ( 1.05 / ( 1 + blockDistance.x * blockDistance.x ) - 0.05 ) ) / M_PI;
        }
       
        float fogFactor = 1.0 - exp( -pow( t, 1.8 ) * 0.000125 );

        return float4( lerp( litColor, fogColor, fogFactor ), 1 );
    }
    else
    {        
        return float4( fogColor, 1 );
    }

}