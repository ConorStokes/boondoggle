#include "audio.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include "../common/boondoggle_helpers.h"
#include <float.h>
#include <assert.h>

#define _USE_MATH_DEFINES

#include <math.h>

namespace 
{
    const float    NOISE_FLOOR                   = -120.0f;
    const uint32_t MIN_REQUIRED_FREQUENCY        = 50;
    const uint32_t MAX_REQUIRED_BUCKET_FREQUENCY = 20000;
    const float    HAMMING_ALPHA                 = 0.5f;
    const float    HAMMING_BETA                  = 1.0f - HAMMING_ALPHA; 
    const float    DBSPL_SCALE                   = 20.0f;
    const float    SMOOTHING_RATE                = 0.17f;
    const float    DEFAULT_SMOOTHING_FREQUENCY   = 50.0f;
}

AudioCapture::AudioCapture() :
    Device_( nullptr ),
    Cursor_( 0 ),
    BufferSize_( 0 ),
    SilenceClient_( nullptr ),
    CaptureClient_( nullptr ),
    SilenceRender_( nullptr ),
    Capture_( nullptr ),
    SampleRate_( 0 ),
    SilenceFrameCount_( 0 ),
    NumberChannelsCapture_( 0 ),
    NumberChannelsSilence_( 0 ),
    LastReadCursor_( 0 )
{
    Channels_[ 0 ] = nullptr;
    Channels_[ 1 ] = nullptr;
}

AudioCapture::~AudioCapture()
{
    if ( SilenceClient_ != nullptr )
    {
        SilenceClient_->Stop();
    }

    if ( CaptureClient_ != nullptr )
    {
        CaptureClient_->Stop();
    }

    COMRelease( Device_ );
    COMRelease( SilenceClient_ );
    COMRelease( CaptureClient_ );
    COMRelease( Capture_ );
    COMRelease( SilenceRender_ );

    // both channels memory is one contiguous allocation, so this deletes the buffer for both channels.
    delete[] Channels_[ 0 ]; 
    Channels_[ 0 ] = nullptr;
    Channels_[ 1 ] = nullptr;
}

bool AudioCapture::Initialize( uint32_t requiredFrequency )
{
    COMAutoPtr< IMMDeviceEnumerator > deviceEnumerator;

    HRESULT deviceEnumeratorResult =
        CoCreateInstance( __uuidof( MMDeviceEnumerator ), 
                          nullptr,
                          CLSCTX_ALL,
                          __uuidof( IMMDeviceEnumerator ),
                          reinterpret_cast< void** >( &deviceEnumerator.raw ) );

    if ( FAILED( deviceEnumeratorResult ) )
    {
        return false;
    }

    HRESULT defaultEndpointResult =
        deviceEnumerator->GetDefaultAudioEndpoint( eRender, eConsole, &Device_ );

    if ( FAILED( defaultEndpointResult ) )
    {
        return false;
    }

    HRESULT activateSilenceResult =
        Device_->Activate( __uuidof( IAudioClient ), CLSCTX_ALL, nullptr, reinterpret_cast<void**>( &SilenceClient_ ) );

    if ( FAILED( activateSilenceResult ) )
    {
        return false;
    }

    HRESULT activateCaptureResult =
        Device_->Activate( __uuidof( IAudioClient ), CLSCTX_ALL, nullptr, reinterpret_cast<void**>( &CaptureClient_ ) );

    if ( FAILED( activateCaptureResult ) )
    {
        return false;
    }

    WAVEFORMATEX* silenceWaveFormat;

    HRESULT silentMixFormatResult =
        SilenceClient_->GetMixFormat( &silenceWaveFormat );

    if ( FAILED( silentMixFormatResult ) )
    {
        return false;
    }

    NumberChannelsSilence_ = silenceWaveFormat->nChannels;
    
    HRESULT silenceClientInitResult =
        SilenceClient_->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            0,
            0,
            0,
            silenceWaveFormat,
            nullptr );

    CoTaskMemFree( silenceWaveFormat );

    if ( FAILED( silenceClientInitResult ) )
    {
        return false;
    }
    
    HRESULT getRenderClientResult =
        SilenceClient_->GetService(
            __uuidof( IAudioRenderClient ),
            reinterpret_cast< void** >( &SilenceRender_ ) );

    if ( FAILED( getRenderClientResult ) )
    {
        return false;
    }

    WAVEFORMATEX* captureFormat;
    
    HRESULT captureMixFormatResult =
        CaptureClient_->GetMixFormat( &captureFormat );

    if ( FAILED( captureMixFormatResult ) )
    {
        return false;
    }

    NumberChannelsCapture_ = captureFormat->nChannels;
    
    // try and force float output on capture.
    switch ( captureFormat->wFormatTag )
    {
    case WAVE_FORMAT_PCM:

        captureFormat->wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
        captureFormat->wBitsPerSample  = 32;
        captureFormat->nBlockAlign     = captureFormat->nChannels * sizeof( float );
        captureFormat->nAvgBytesPerSec = captureFormat->nBlockAlign * captureFormat->nBlockAlign;
        break;

    case WAVE_FORMAT_EXTENSIBLE:
        {
            WAVEFORMATEXTENSIBLE* extensible = reinterpret_cast<WAVEFORMATEXTENSIBLE*>( captureFormat );

            if ( !IsEqualGUID( KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, extensible->SubFormat ) )
            {
                extensible->SubFormat                   = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
                extensible->Samples.wValidBitsPerSample = 32;
                captureFormat->wBitsPerSample           = 32;
                captureFormat->nBlockAlign              = captureFormat->nChannels * sizeof( float );
                captureFormat->nAvgBytesPerSec          = captureFormat->nBlockAlign * captureFormat->nBlockAlign;
            }
        }
    }

    SampleRate_ = captureFormat->nSamplesPerSec;

    uint32_t captureSamples = 1024;

    while ( captureFormat->nSamplesPerSec / captureSamples > requiredFrequency )
    {
        captureSamples *= 2;
    }

    BufferSize_    = captureSamples * 2;

    Channels_[ 0 ] = new float[ BufferSize_ * 2 ];
    Channels_[ 1 ] = Channels_[ 0 ] + BufferSize_;

    HRESULT captureInitResult =
        CaptureClient_->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK,
            0,
            0,
            captureFormat,
            nullptr );

    CoTaskMemFree( captureFormat );

    if ( FAILED( captureInitResult ) )
    {
        return false;
    }

    HRESULT getCaptureClientResult =
        CaptureClient_->GetService(
            __uuidof( IAudioCaptureClient ),
            reinterpret_cast< void** >( &Capture_ ) );

    if ( FAILED( getCaptureClientResult ) )
    {
        return false;
    }

    HRESULT silenceGetBufferSizeResult =
        SilenceClient_->GetBufferSize( &SilenceFrameCount_ );

    if ( FAILED( silenceGetBufferSizeResult ) )
    {
        return false;
    }

    BYTE* silenceData = nullptr;

    HRESULT silenceGetBufferResult = 
        SilenceRender_->GetBuffer( SilenceFrameCount_, &silenceData );

    if ( FAILED( silenceGetBufferResult ) )
    {
        return false;
    }

    HRESULT silenceFillResult = 
        SilenceRender_->ReleaseBuffer( SilenceFrameCount_, AUDCLNT_BUFFERFLAGS_SILENT );

    if ( FAILED( silenceFillResult ) )
    {
        return false;
    }

    HRESULT silenceStartResult = SilenceClient_->Start();

    if ( FAILED( silenceStartResult ) )
    {
        return false;
    }

    HRESULT captureStartResult = CaptureClient_->Start();

    if ( FAILED( captureStartResult ) )
    {
        return false;
    }

    return true;
}

AudioUpdateResult AudioCapture::PullAudio()
{
    uint32_t silencePadding   = 0;

    HRESULT  getPaddingResult;
    
    for ( getPaddingResult = SilenceClient_->GetCurrentPadding( &silencePadding );
          SUCCEEDED( getPaddingResult ) && SilenceFrameCount_ != silencePadding;
          getPaddingResult = SilenceClient_->GetCurrentPadding( &silencePadding ) )
    {
        BYTE*   renderBuffer          = nullptr;
        HRESULT getRenderBufferResult =
            SilenceRender_->GetBuffer( SilenceFrameCount_ - silencePadding, &renderBuffer );

        if ( FAILED( getRenderBufferResult ) )
        {
            return AudioUpdateResult::AUDIO_ERROR;
        }

        HRESULT releaseBufferRenderResult =
            SilenceRender_->ReleaseBuffer( SilenceFrameCount_ - silencePadding, AUDCLNT_BUFFERFLAGS_SILENT );

        if ( FAILED( releaseBufferRenderResult ) )
        {
            return AudioUpdateResult::AUDIO_ERROR;
        }
    }

    if ( FAILED( getPaddingResult ) )
    {
        return AudioUpdateResult::AUDIO_ERROR;
    }

    HRESULT  getNextPacketResult;
    uint32_t nextPacketSize = 0;

    for ( getNextPacketResult = Capture_->GetNextPacketSize( &nextPacketSize );
          SUCCEEDED( getNextPacketResult ) && nextPacketSize > 0;
          getNextPacketResult = Capture_->GetNextPacketSize( &nextPacketSize ) )
    {
        BYTE* readBuffer;
        uint32_t framesRead;
        DWORD readFlags;

        HRESULT getBufferResult = 
            Capture_->GetBuffer(
                &readBuffer,
                &framesRead,
                &readFlags,
                nullptr,
                nullptr );

        if ( FAILED( getBufferResult ) )
        {
            return AudioUpdateResult::AUDIO_ERROR;
        }

        if ( AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == readFlags )
        {
            Cursor_         = 0;
            LastReadCursor_ = 0;
        }

        const float* floatBuffer = reinterpret_cast< const float* >( readBuffer );

        if ( NumberChannelsCapture_ == 1 )
        {
            for ( const float* currentFrame = floatBuffer, *endFrame = floatBuffer + framesRead; 
                  currentFrame < endFrame; 
                  ++currentFrame, ++Cursor_ )
            {
                Channels_[ 0 ][ Cursor_ & ( BufferSize_ - 1 ) ] = 
                Channels_[ 1 ][ Cursor_ & ( BufferSize_ - 1 ) ] = *currentFrame;
            }
        }
        else
        {
            assert( NumberChannelsCapture_ < 5 );

            for ( const float* currentFrame = floatBuffer, *endFrame = floatBuffer + ( framesRead * NumberChannelsCapture_ ); 
                  currentFrame < endFrame; 
                  currentFrame += NumberChannelsCapture_, ++Cursor_ )
            {
                Channels_[ 0 ][ Cursor_ & ( BufferSize_ - 1 ) ] = currentFrame[ 0 ];
                Channels_[ 1 ][ Cursor_ & ( BufferSize_ - 1 ) ] = currentFrame[ 1 ];
            }
        }

        HRESULT bufferReleaseResult = Capture_->ReleaseBuffer( framesRead );
    
        if ( FAILED( bufferReleaseResult ) )
        {
            return AudioUpdateResult::AUDIO_ERROR;
        }
    }

    if ( FAILED( getNextPacketResult ) )
    {
        return AudioUpdateResult::AUDIO_ERROR;
    }

    AudioUpdateResult result =
        ( Cursor_ - LastReadCursor_ ) >= uint64_t( SamplesPerPeriod() ) ? 
            AudioUpdateResult::UPDATED : 
            AudioUpdateResult::NOT_UPDATED;

    if ( result == AudioUpdateResult::UPDATED )
    {
        LastReadCursor_ = Cursor_ & ~( ( BufferSize_ >> 1 ) - 1 );
    }

    return result;
}

AudioProcessing::AudioProcessing() :
    Kiss_( nullptr ),
    Window_( nullptr ),
    Intermediate_( nullptr ),
    Frequency_( nullptr ),
    AudioTextureData_( nullptr )
{
    for ( uint32_t where = 0; where < FREQUENCY_BUCKETS; ++where )
    {
        BucketRange_[ where ][ 0 ] = FLT_MAX;
        BucketRange_[ where ][ 1]  = -FLT_MAX;
    }
}

AudioProcessing::~AudioProcessing()
{
    ::_aligned_free( AudioTextureData_ );
}

bool AudioProcessing::Initialize( PerFrameConstants& toUpdate )
{
    bool result = Capture_.Initialize( MIN_REQUIRED_FREQUENCY );

    if ( result )
    {
        size_t kissMemLength    = 0;
        size_t samplesPerPeriod = Capture_.SamplesPerPeriod();
        size_t realFFTSamples   = ( samplesPerPeriod / 2 ) + 1;

        float minBinFrequency = static_cast<float>( Capture_.SampleRate() ) / static_cast<float>( samplesPerPeriod );

        // Calculate the top bin above our frequency ceiling for audio cut-off.
        RelevantBins_ = static_cast< uint32_t >( ceilf( MAX_REQUIRED_BUCKET_FREQUENCY / minBinFrequency ) ) + 1;

        // Clamp the relevant bin to the actual number of bins we actually have.
        if ( RelevantBins_ > realFFTSamples )
        {
            RelevantBins_ = static_cast< uint32_t >( realFFTSamples );
        }

        kiss_fftr_alloc( static_cast< int >( samplesPerPeriod ), 0, nullptr, &kissMemLength );

        // Make it one big 16 byte aligned allocation
        void* blockAllocation = 
            ::_aligned_recalloc( nullptr, 
                                 sizeof( kiss_fft_cpx ) * realFFTSamples + 
                                 sizeof( float ) * samplesPerPeriod * 6 +
                                 sizeof( uint32_t ) * realFFTSamples +
                                 kissMemLength, 
                                 1, 
                                 16 );

        AudioTextureData_ = reinterpret_cast< float* >( blockAllocation );
        Window_           = AudioTextureData_ + ( 4 * samplesPerPeriod );
        Intermediate_     = Window_ + samplesPerPeriod;
        BucketMapping_    = reinterpret_cast< uint32_t* >( Intermediate_ + samplesPerPeriod );
        Frequency_        = reinterpret_cast< kiss_fft_cpx* >( BucketMapping_ + RelevantBins_ - 1 );

        Kiss_ = kiss_fftr_alloc( static_cast< int >( samplesPerPeriod ), 
                                 0, 
                                 reinterpret_cast< void* >( Frequency_ + realFFTSamples ), 
                                 &kissMemLength );

        float angleScale = static_cast< float >( M_PI * 2.0f ) / ( samplesPerPeriod - 1 );

        // pre-calculate hamming window weights.
        for ( uint32_t windowPosition = 0; windowPosition < samplesPerPeriod; ++windowPosition )
        {
            Window_[ windowPosition ] = HAMMING_ALPHA - HAMMING_BETA * cosf( angleScale * static_cast< float >( windowPosition ) );
        }

        float inverseMaxRelevantFrequency = 1.0f / ( minBinFrequency * ( RelevantBins_ - 2 ) );

        // skip over DC
        for ( uint32_t bin = 1; bin < RelevantBins_; ++bin )
        {
            float binFrequency        = bin * minBinFrequency;

            // uses a gamma like bucket allocation.
            uint32_t bucket = 
                static_cast< uint32_t >( roundf( sqrtf( ( binFrequency - minBinFrequency ) * inverseMaxRelevantFrequency ) * ( FREQUENCY_BUCKETS - 1 ) ) );

            BucketMapping_[ bin - 1 ]   = bucket;
            BucketRange_[ bucket ][ 0 ] = binFrequency < BucketRange_[ bucket ][ 0 ] ? binFrequency : BucketRange_[ bucket ][ 0 ];
            BucketRange_[ bucket ][ 1 ] = binFrequency > BucketRange_[ bucket ][ 1 ] ? binFrequency : BucketRange_[ bucket ][ 1 ];
        }

        Smoothing_ = powf( SMOOTHING_RATE, minBinFrequency / DEFAULT_SMOOTHING_FREQUENCY );
    }

    toUpdate.NoiseFloorDbSPL = NOISE_FLOOR;
    toUpdate.SoundSampleRate = static_cast<float>( Capture_.SampleRate() );
    toUpdate.SoundSamples    = static_cast<float>( Capture_.SamplesPerPeriod() );

    for ( uint32_t bucket = 0; bucket < FREQUENCY_BUCKETS; ++bucket )
    {
        toUpdate.SoundFrequencyBuckets[ bucket ][ 0 ] = NOISE_FLOOR;
        toUpdate.SoundFrequencyBuckets[ bucket ][ 1 ] = NOISE_FLOOR;
        toUpdate.SoundFrequencyBuckets[ bucket ][ 2 ] = BucketRange_[ bucket ][ 0 ];
        toUpdate.SoundFrequencyBuckets[ bucket ][ 3 ] = BucketRange_[ bucket ][ 1 ];
    }

    return result;
}

AudioUpdateResult AudioProcessing::Update( PerFrameConstants& toUpdate )
{
    AudioUpdateResult result = Capture_.PullAudio();

    if ( result == AudioUpdateResult::UPDATED )
    {
        ProcessChannel( toUpdate, 0 );
        ProcessChannel( toUpdate, 1 );
    }

    toUpdate.SoundSampleRate = static_cast<float>( Capture_.SampleRate() );
    toUpdate.SoundSamples    = static_cast<float>( Capture_.SamplesPerPeriod() );

    for ( uint32_t bucket = 0; bucket < FREQUENCY_BUCKETS; ++bucket )
    {
        toUpdate.SoundFrequencyBuckets[ bucket ][ 2 ] = BucketRange_[ bucket ][ 0 ];
        toUpdate.SoundFrequencyBuckets[ bucket ][ 3 ] = BucketRange_[ bucket ][ 1 ];
    }


    return result;
}

void AudioProcessing::ProcessChannel( PerFrameConstants& toUpdate, uint32_t channel ) 
{
    const float* channelData = Capture_.GetChannel( channel );
    size_t samplesPerPeriod  = Capture_.SamplesPerPeriod();

    float channelMS = 0;

    for ( uint32_t frame = 0, numberSamples = static_cast< uint32_t >( samplesPerPeriod );
          frame < numberSamples;
          ++frame )
    {
        float channelFrameValue = channelData[ frame ];

        channelMS                                   += channelFrameValue * channelFrameValue;
        Intermediate_[ frame ]                       = channelFrameValue * Window_[ frame ];
        AudioTextureData_[ frame * 4 + channel ]     = channelFrameValue;
        AudioTextureData_[ frame * 4 + channel + 2 ] = 0;
    }

    float inverseSamples = 1.0f / static_cast< float >( Capture_.SamplesPerPeriod() );

    channelMS *= inverseSamples;

    toUpdate.SoundRMS[ channel ] += ( sqrtf( channelMS ) - toUpdate.SoundRMS[ channel ] ) * Smoothing_;

    float soundDbSPL = DBSPL_SCALE * log10f( toUpdate.SoundRMS[ channel ] );

    toUpdate.SoundRMSdbSPL[ channel ] += soundDbSPL > NOISE_FLOOR ? soundDbSPL : NOISE_FLOOR;

    kiss_fftr( Kiss_, Intermediate_, Frequency_ );

    uint32_t binCount = ( static_cast< uint32_t >( Capture_.SamplesPerPeriod() ) / 2 ) + 1;

    float normalizationFactor = 2.0f / binCount;

    for ( uint32_t bin = 0; bin < binCount; ++bin )
    {
        Frequency_[ bin ].r                        *= normalizationFactor;
        Frequency_[ bin ].i                        *= normalizationFactor;
        AudioTextureData_[ bin * 4 + channel + 2 ]  = sqrtf( Frequency_[ bin ].r * Frequency_[ bin ].r * + Frequency_[ bin ].i * Frequency_[ bin ].i );
    }

    float buckets[ FREQUENCY_BUCKETS ];

    for ( uint32_t bucket = 0; bucket < FREQUENCY_BUCKETS; ++bucket )
    {
        buckets[ bucket ] = -FLT_MAX;
    }

    for ( uint32_t bin = 1; bin < RelevantBins_; ++bin )
    {
        uint32_t bucketMapping    = BucketMapping_[ bin - 1 ];
        float    amplitudeSquared = Frequency_[ bin ].r * Frequency_[ bin ].r + Frequency_[ bin ].i * Frequency_[ bin ].i;
        float    bucketValue      = buckets[ bucketMapping ];

        buckets[ bucketMapping ] = amplitudeSquared > bucketValue ? amplitudeSquared : bucketValue;
    }

    for ( uint32_t bucket = 0; bucket < FREQUENCY_BUCKETS; ++bucket )
    {
        float bucketValue = buckets[ bucket ];
        
        bucketValue = 0.5f * DBSPL_SCALE * log10f( bucketValue );
        bucketValue = ( bucketValue > NOISE_FLOOR ? bucketValue : NOISE_FLOOR );

        toUpdate.SoundFrequencyBuckets[ bucket ][ channel ] += ( bucketValue - toUpdate.SoundFrequencyBuckets[ bucket ][ channel ] ) * Smoothing_;
    }
}
