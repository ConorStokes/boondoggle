#ifndef BOONDOGGLE_AUDIO_CAPTURE_H__
#define BOONDOGGLE_AUDIO_CAPTURE_H__

#pragma once

#include <stdint.h>
#include "shared_render_constants.h"
#include "../external/kissfft/kiss_fftr.h"

struct IAudioClient;
struct IMMDevice;
struct IAudioRenderClient;
struct IAudioCaptureClient;

enum class AudioUpdateResult
{
    UPDATED = 0,
    NOT_UPDATED = 1,
    AUDIO_ERROR = 2
};

// Captures audio from the default device.
class AudioCapture
{
public:
    
    AudioCapture();

    ~AudioCapture();

    // Initialize audio capture with a required "base frequency" that is the 
    // minimum frequency we need to capture in an audio update.
    bool Initialize( uint32_t requiredFrequency );

    // We've filled enough buffer for another period.
    AudioUpdateResult PullAudio();

    // The sample rate of the audio.
    uint32_t SampleRate() const { return SampleRate_; }

    // Number of samples in an individual period.
    size_t SamplesPerPeriod() const { return BufferSize_ >> 1; }

    // Get left=0 or right=1 channel (always stereo)
    const float* GetChannel( uint32_t channel ) const 
    {
        uint64_t innerCursor    = Cursor_ & (BufferSize_ - 1);
        size_t   halfBufferSize = BufferSize_ >> 1;
        size_t   offset         = (innerCursor < halfBufferSize) ? halfBufferSize : 0;

        return Channels_[ channel ] + offset;
    }

private:

    uint64_t             Cursor_;
    uint64_t             LastReadCursor_;
    IMMDevice*           Device_;
    IAudioClient*        SilenceClient_;
    IAudioClient*        CaptureClient_;
    IAudioRenderClient*  SilenceRender_;
    IAudioCaptureClient* Capture_;
    float*               Channels_[ 2 ];
    size_t               BufferSize_; // must be power of two
    uint32_t             SampleRate_;
    uint32_t             SilenceFrameCount_;
    uint32_t             NumberChannelsSilence_;
    uint32_t             NumberChannelsCapture_;

};

// Audio processing - uses AudioCapture to capture audio, then runs it through
// our processing pipeline to get values for visualization.
class AudioProcessing
{
public:

    AudioProcessing();

    ~AudioProcessing();

    // Initialize this - and populate the appropriate visualization constants.
    bool Initialize( PerFrameConstants& toUpdate );

    // Number of samples in a period for processing.
    size_t SamplesPerPeriod() const { return Capture_.SamplesPerPeriod(); }

    // The data to be put in an audio texture
    // left and right channels in [0] and [1], left and right FFT amplitudes in [2] and [3].
    // Note, we don't care about phases really here.
    const float* AudioTextureData() const { return AudioTextureData_; }

    // Attempt to update the per-frame constants from the audio processing.
    // Will indicate if any processing occured of if there was an error in the return value.
    AudioUpdateResult Update( PerFrameConstants& toUpdate );

private:

    void ProcessChannel( PerFrameConstants& toUpdate, uint32_t channel );

    AudioCapture  Capture_;
    kiss_fftr_cfg Kiss_;
    float*        Window_;
    float*        Intermediate_;
    kiss_fft_cpx* Frequency_;
    uint32_t*     BucketMapping_;
    float*        AudioTextureData_;
    uint32_t      RelevantBins_; // the number of relevant frequency bins for bucketing.
    float         BucketRange_[ FREQUENCY_BUCKETS ][ 2 ];
    float         Smoothing_;
};

#endif // -- BOONDOGGLE_AUDIO_CAPTURE_H__