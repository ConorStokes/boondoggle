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

class AudioCapture
{
public:
    
    AudioCapture();

    ~AudioCapture();

    bool Initialize( uint32_t requiredFrequency );

    // Pull audio 
    AudioUpdateResult PullAudio();

    uint32_t SampleRate() const { return SampleRate_; }

    size_t SamplesPerPeriod() const { return BufferSize_ >> 1; }

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

class AudioProcessing
{
public:

    AudioProcessing();

    ~AudioProcessing();

    size_t SamplesPerPeriod() const { return Capture_.SamplesPerPeriod(); }

    const float* AudioTextureData() const { return AudioTextureData_; }

    bool Initialize( PerFrameConstants& toUpdate );

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