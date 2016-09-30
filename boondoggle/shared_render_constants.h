#ifndef BOONDOGGLE_SHARED_RENDER_CONSTANTS_H__
#define BOONDOGGLE_SHARED_RENDER_CONSTANTS_H__

#pragma once

#define FREQUENCY_BUCKETS 16

struct PerFrameConstants
{
    float Time;
    float DeltaTime;
    // Eventually we're going to add transitions for the effects.
    float TransitionIn; 
    float TransitionOut;

    // Each frequency bin has max dbSPL of the bin for channel 0 and channel 1 (with a cut off noise floor of -60), 
    // as well as the min and max frequency of the bin.
    float SoundFrequencyBuckets[ FREQUENCY_BUCKETS ][ 4 ]; 

    // Has the non-logarithmic RMS for each channel
    float SoundRMS[ 2 ];

    // Has the logarithmic dbSPL for each channel
    float SoundRMSdbSPL[ 2 ];

    float SoundSampleRate;

    // sound samples in the sound texture.
    float SoundSamples; 

    float NoiseFloorDbSPL;

    float Padding;
};

struct PerViewConstants
{
    float                   EyePosition[ 4 ];
    float                   RayScreenUpperLeft[ 4 ]; // top left ray direction
    float                   RayScreenRight[ 4 ];  // pre-scaled right addition to 
    float                   RayScreenDown[ 4 ];
};

#endif // --BOONDOGGLE_SHARED_RENDER_CONSTANTS_H__