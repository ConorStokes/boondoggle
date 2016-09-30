cbuffer Parameters : register( b0 )
{
    float Time : packoffset( c0.x );
    float DeltaTime : packoffset( c0.y );
    float TransitionIn : packoffset( c0.z );
    float TransitionOut : packoffset( c0.w );

    float4 SoundFrequencyBuckets[ 16 ] : packoffset( c1 );

    float2 SoundRMS : packoffset( c17.x );
    float2 SoundRMSdbSPL : packoffset( c17.z );

    float SoundSampleRate : packoffset( c18.x );
    float SoundSamples : packoffset( c18.y );
    float NoiseFloorDbSPL : packoffset( c18.z );

    float2 Resolution : packoffset( c19.x );
    float2 InverseResolution : packoffset( c19.z );

    float4 EyePosition : packoffset( c20 );
    float4 RayScreenUpperLeft : packoffset( c21 );
    float4 RayScreenRight : packoffset( c22 );
    float4 RayScreenDown : packoffset( c23 );
};