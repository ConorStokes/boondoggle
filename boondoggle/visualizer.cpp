#include <windows.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <stdint.h>
#include "visualizer.h"
#include "oculus_helpers.h"
#include "../common/boondoggle_helpers.h"
#include "OVR_CAPI_D3D.h"
#include "visual_effects.h"
#include <DirectXMath.h>
#include "audio.h"

#define _USE_MATH_DEFINES

#include <math.h>

namespace
{
    const WCHAR* const DISPLAY_CLASS_NAME = L"Boondoggle";
    const WCHAR* const DISPLAY_TITLE      = L"Boondoggle";
    const size_t       BufferSize         = 384;

    struct VisualizerResources
    {
        HINSTANCE                  ModuleInstance;
        HWND                       WindowHandle;
        
        ID3D11Device*              Device;
        ID3D11DeviceContext*       Context;
        IDXGISwapChain*            SwapChain;
        ID3D11Texture2D*           BackBuffer;
        ID3D11RenderTargetView*    BackBufferTarget;
        ID3D11Buffer*              ConstantBuffer;
        ID3D11Texture1D*           SoundTexture;
        ID3D11ShaderResourceView*  SoundTextureSRV;

        LONG                       Width;
        LONG                       Height;

        BoondoggleEffectsPackage*  Effects;
        uint8_t*                   BufferMemory;
        
        int32_t                    LeftDown;
        int32_t                    RightDown;

        VisualizerResources();

        // Open the display window, returns false on failure.
        bool OpenWindow( uint32_t width, uint32_t height );

        // Open the D3D device (luid is for the adapter to use to open said device, if null will get the default adapter).
        // Returns false on failure.
        bool CreateD3D( /*optional*/ const LUID* luid = nullptr );
        
        bool CreateSoundTexture( const AudioProcessing& from );

        // Close the D3D device
        void CloseDevice();

        // Show the window
        void ShowWindow();

        // Handles messages to this window
        LRESULT WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );

        // Show an error message box for this window.
        void ShowError( LPCWSTR message, LPCWSTR caption );

        // Resize this window
        void Resize( uint32_t width, uint32_t height );

        // Load package.
        bool LoadPackage( const wchar_t* packageFile );

        ~VisualizerResources();

        VisualizerResources( const VisualizerResources& ) = delete;

        VisualizerResources& operator=( const VisualizerResources& ) = delete;
    };

    class Clock
    {
    public:

        Clock();

        void Reset();

        double GetElapsedTime() const;

    private:

        int64_t m_frequency;
        int64_t m_initial;

    };

    Clock::Clock()
    {
        LARGE_INTEGER frequency;
        LARGE_INTEGER initial;

        ::QueryPerformanceFrequency( &frequency );

        m_frequency = frequency.QuadPart;

        ::QueryPerformanceCounter( &initial );

        m_initial = initial.QuadPart;
    }

    void Clock::Reset()
    {
        LARGE_INTEGER initial;

        ::QueryPerformanceCounter( &initial );

        m_initial = initial.QuadPart;
    }

    double Clock::GetElapsedTime() const
    {
        LARGE_INTEGER current;

        ::QueryPerformanceCounter( &current );
        
        int64_t delta         = current.QuadPart - m_initial;
        int64_t positiveDelta = delta > 0 ? delta : 0;

        return ( (double)positiveDelta / (double)m_frequency );
    }

    VisualizerResources::VisualizerResources() 
        : WindowHandle( nullptr ),
          Device( nullptr ),
          Context( nullptr ),
          BackBuffer( nullptr ),
          BackBufferTarget( nullptr ),
          SwapChain( nullptr ),
          Effects( nullptr ),
          ConstantBuffer( nullptr ),
          LeftDown( 0 ),
          RightDown( 0 ),
          SoundTexture( nullptr ),
          SoundTextureSRV( nullptr ),
          BufferMemory( reinterpret_cast< uint8_t* >( _aligned_malloc( BufferSize, 16 ) ) )
    {
    }
    
    void VisualizerResources::CloseDevice()
    {
        COMRelease( SoundTexture );
        COMRelease( SoundTextureSRV );
        COMRelease( ConstantBuffer );
        COMRelease( BackBufferTarget );
        COMRelease( BackBuffer );
        COMRelease( SwapChain );
        COMRelease( Context );
        COMRelease( Device );
    }
    
    VisualizerResources::~VisualizerResources()
    {
        delete Effects;
        Effects = nullptr;

        ::_aligned_free( BufferMemory );
        BufferMemory = nullptr;

        CloseDevice();

        if ( WindowHandle != nullptr )
        {
            ::DestroyWindow( WindowHandle );
            
            WindowHandle = nullptr;

            ::UnregisterClassW( DISPLAY_CLASS_NAME, ModuleInstance );
        }
    }

    void VisualizerResources::ShowWindow()
    {
        if ( WindowHandle != nullptr )
        {
            ::ShowWindow( WindowHandle, SW_SHOW );
        }
    }

    void VisualizerResources::Resize( uint32_t width, uint32_t height )
    {
        if ( WindowHandle != nullptr )
        {
            Width  = width;
            Height = height;

            ::RECT windowRectangle = { 0, 0, static_cast< LONG >( width ), static_cast< LONG >( height ) };

            ::AdjustWindowRect( &windowRectangle, WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME, FALSE );
            ::SetWindowPos( WindowHandle, 
                            nullptr, 
                            0,
                            0, 
                            windowRectangle.right - windowRectangle.left, 
                            windowRectangle.bottom - windowRectangle.top, 
                            SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW );
        }
    }


    // Load package.
    bool VisualizerResources::LoadPackage( const wchar_t* packageFile )
    {
        Effects = new BoondoggleEffectsPackage();
    
        bool result = Effects->CreateResources( Device, Context, WindowHandle, D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION, packageFile );

        if ( !result )
        {
            delete Effects;
            Effects = nullptr;
        }

        return result;
    }


    LRESULT CALLBACK DisplayWindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
    {
        VisualizerResources* window = reinterpret_cast< VisualizerResources* >( ::GetWindowLongPtr( hWnd, 0 ) );

        if ( window != nullptr )
        {
            return window->WindowProc( uMsg, wParam, lParam );
        }
        else
        {
            return ::DefWindowProcW( hWnd, uMsg, wParam, lParam );
        }
    }

    LRESULT VisualizerResources::WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
    {
        LRESULT result = 0;

        switch ( uMsg )
        {
        case WM_SIZE:
            
            break;

        case WM_KEYDOWN:

            switch ( wParam )
            {
            case VK_ESCAPE:

                ::DestroyWindow( WindowHandle );
                break;

            case VK_LEFT:

                ++LeftDown;
                break;

            case VK_RIGHT:

                ++RightDown;
                break;
            }

            break;

        case WM_DESTROY:

            ::PostQuitMessage( 0 );
            break;

        default:

            result = DefWindowProcW( WindowHandle, uMsg, wParam, lParam );
            break;
        }
        
        return result;
    }
            
    bool VisualizerResources::OpenWindow( uint32_t width, uint32_t height )
    {
        HICON icon                = nullptr;
        WCHAR exePath[ MAX_PATH ];

        Width  = width;
        Height = height;

        ModuleInstance = ( HINSTANCE )::GetModuleHandle( 0 );

        // Note, if by some whacky happenstance we exceed max-path, then
        // this will truncate to max-path and extracting the icon will fail.
        ::GetModuleFileNameW( 0, exePath, MAX_PATH );

        icon = ::ExtractIconW( ModuleInstance, exePath, 0 );

        // Register the windows class
        WNDCLASSW windowClass = {};

        windowClass.style         = CS_OWNDC;
        windowClass.lpfnWndProc   = DisplayWindowProc;
        windowClass.cbClsExtra    = 0;
        windowClass.cbWndExtra    = sizeof( this );
        windowClass.hInstance     = ModuleInstance;
        windowClass.hIcon         = icon;
        windowClass.hCursor       = ::LoadCursorW( NULL, IDC_ARROW );
        windowClass.hbrBackground = reinterpret_cast< HBRUSH >( ::GetStockObject( BLACK_BRUSH ) );
        windowClass.lpszMenuName  = 0;
        windowClass.lpszClassName = DISPLAY_CLASS_NAME;

        ::RegisterClassW( &windowClass );

        RECT inner;
         
        inner.left   = 0;
        inner.top    = 0;
        inner.right  = width;
        inner.bottom = height;

        ::AdjustWindowRect( &inner, WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME, false );

        WindowHandle =
            ::CreateWindowW( DISPLAY_CLASS_NAME,
                             DISPLAY_TITLE,
                             WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
                             0,
                             0,
                             ( inner.right - inner.left ),
                             ( inner.bottom - inner.top ),
                             0,
                             0,
                             ModuleInstance,
                             0 );

        if ( WindowHandle != nullptr )
        {
            ::SetWindowLongPtr( WindowHandle, 0, reinterpret_cast< LONG_PTR >( this ) );
            ::SetActiveWindow( WindowHandle );
            ::SetFocus( WindowHandle );
        }
        else
        {
            DWORD  error  = ::GetLastError();
            LPWSTR buffer = nullptr;

            ::FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                0,
                error,
                MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
                (LPWSTR)&buffer,
                0,
                0 );

            ::MessageBoxW( nullptr, buffer, L"Error Creating Boondoggle Window", MB_OK | MB_ICONERROR );

            ::LocalFree( buffer );
        }

        return WindowHandle != nullptr;
    }

    void VisualizerResources::ShowError( LPCWSTR message, LPCWSTR caption )
    {
        ::MessageBoxW( WindowHandle, message, caption, MB_OK | MB_ICONERROR );
    }

    bool VisualizerResources::CreateD3D( const LUID* luid )
    {
        COMAutoPtr< IDXGIFactory > dxgiFactory;
        COMAutoPtr< IDXGIAdapter > adapter;

        HRESULT factoryResult = ::CreateDXGIFactory1( __uuidof( IDXGIFactory ), 
                                                      reinterpret_cast< void** >( &( dxgiFactory.raw ) ) );

        if ( factoryResult != ERROR_SUCCESS )
        {
            ShowError( L"Error creating DXGI Factory", L"Error Initializing Direct 3D 11" );
            return false;
        }

        if ( luid != nullptr )
        {
            for ( UINT adapterIndex = 0; ; ++adapterIndex )
            {
                if ( dxgiFactory->EnumAdapters( adapterIndex, &adapter.raw ) == DXGI_ERROR_NOT_FOUND )
                {
                    break;
                }

                DXGI_ADAPTER_DESC adapterDesc;

                adapter->GetDesc( &adapterDesc );

                if ( ::memcmp( &adapterDesc.AdapterLuid, luid, sizeof( LUID ) ) == 0 )
                {
                    break;
                }

                adapter.Release();
            }
        }

        D3D_FEATURE_LEVEL featureLevels = D3D_FEATURE_LEVEL_11_0;
        D3D_FEATURE_LEVEL featureLevel;

        UINT creationFlags = 0;

#if defined(_DEBUG)
        // If the project is in a debug build, enable the debug layer.
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        HRESULT deviceResult = 
            ::D3D11CreateDevice( adapter.raw, 
                                 adapter.raw != nullptr ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, 
                                 nullptr, 
                                 creationFlags,
                                 &featureLevels, 
                                 1, 
                                 D3D11_SDK_VERSION, 
                                 &Device, 
                                 &featureLevel, 
                                 &Context );

        adapter.Release();

        if ( deviceResult != ERROR_SUCCESS )
        {
            ShowError( L"Error creating D3D Device", L"Error Initializing Direct 3D 11" );
        
            return false;
        }


#if defined(_DEBUG)
        
        {
            ID3D11Debug* debug = nullptr;

            HRESULT debugResult = Device->QueryInterface( __uuidof( ID3D11Debug ), (void**)&debug );

            if ( debugResult == ERROR_SUCCESS )
            {
                debug->ReportLiveDeviceObjects( D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL );

                ID3D11InfoQueue *infoQueue = nullptr;

                HRESULT infoQueueResult = debug->QueryInterface( __uuidof( ID3D11InfoQueue ), (void**)&infoQueue );

                if ( infoQueueResult == ERROR_SUCCESS )
                {
                    infoQueue->SetMuteDebugOutput( FALSE );
                    infoQueue->SetBreakOnSeverity( D3D11_MESSAGE_SEVERITY_CORRUPTION, true );
                    infoQueue->SetBreakOnSeverity( D3D11_MESSAGE_SEVERITY_ERROR, true );

                    infoQueue->Release();
                    infoQueue = nullptr;
                }

                debug->Release();
                debug = nullptr;
            }
        }

#endif


        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        
        swapChainDesc.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.BufferCount                        = 2;
        swapChainDesc.BufferDesc.Width                   = Width;
        swapChainDesc.BufferDesc.Height                  = Height;
        swapChainDesc.BufferDesc.RefreshRate.Numerator   = 0;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDesc.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        swapChainDesc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow                       = WindowHandle;
        swapChainDesc.SampleDesc.Count                   = 1;
        swapChainDesc.SampleDesc.Quality                 = 0;
        swapChainDesc.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        swapChainDesc.Windowed                           = true;

        HRESULT swapChainResult = 
            dxgiFactory->CreateSwapChain( Device, 
                                          &swapChainDesc, 
                                          &SwapChain );

        if ( swapChainResult != ERROR_SUCCESS )
        {
            ShowError( L"Error creating swap chain", L"Error Initializing Direct 3D 11" );

            return false;
        }

        SwapChain->GetBuffer( 0, __uuidof( *BackBuffer ), (void**)&BackBuffer );

        HRESULT renderTargetResult = Device->CreateRenderTargetView( BackBuffer, nullptr, &BackBufferTarget );

        if ( renderTargetResult != ERROR_SUCCESS )
        {
            ShowError( L"Error creating back buffer render target", L"Error Initializing Direct 3D 11" );

            return false;
        }

        D3D11_BUFFER_DESC constantBufferDesc = {};

        constantBufferDesc.ByteWidth           = BufferSize; 
        constantBufferDesc.Usage               = D3D11_USAGE::D3D11_USAGE_DYNAMIC;
        constantBufferDesc.BindFlags           = D3D11_BIND_FLAG::D3D11_BIND_CONSTANT_BUFFER;
        constantBufferDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;
        constantBufferDesc.MiscFlags           = 0;
        constantBufferDesc.StructureByteStride = 0;

        HRESULT bufferResult = Device->CreateBuffer( &constantBufferDesc, nullptr, &ConstantBuffer );

        if ( bufferResult != ERROR_SUCCESS )
        {
            ShowError( L"Error creating constant buffer.", L"Error Initializing Direct 3D 11" );

            return false;
        }

        return true;
    }
    
    bool VisualizerResources::CreateSoundTexture( const AudioProcessing& from )
    {
        D3D11_TEXTURE1D_DESC textureDesc = {};

        textureDesc.Width          = static_cast< UINT >( from.SamplesPerPeriod() );
        textureDesc.MipLevels      = 1;
        textureDesc.ArraySize      = 1;
        textureDesc.Format         = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
        textureDesc.BindFlags      = D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;
        textureDesc.Usage          = D3D11_USAGE::D3D11_USAGE_DYNAMIC;
//        textureDesc.MiscFlags      = D3D11_RESOURCE_MISC_FLAG::D3D11_RESOURCE_MISC_GENERATE_MIPS;
        textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;

        HRESULT createTextureResult = Device->CreateTexture1D( &textureDesc, nullptr, &SoundTexture );
    
        if ( createTextureResult != ERROR_SUCCESS )
        {
            ShowError( L"Couldn't create sound texture", L"Error Initializing Sound Texture" );
            return false;
        }
        
        HRESULT srvResult = Device->CreateShaderResourceView( SoundTexture, nullptr, &SoundTextureSRV );

        if ( srvResult != ERROR_SUCCESS )
        {
            ShowError( L"Couldn't create sound texture resource view", L"Error Initializing Sound Texture" );
            return false;
        }

        return true;
    }

    // Windows message pump. Returns false on quit message.
    bool PumpMessages()
    {
        bool  isQuit  = false;
        ::MSG message;

        ::ZeroMemory( &message, sizeof( ::MSG ) );
        
        while ( ::PeekMessage( &message, nullptr, 0, 0, PM_REMOVE ) )
        {
            isQuit |= message.message == WM_QUIT;

            ::TranslateMessage( &message );
            ::DispatchMessage( &message );
        }

        return !isQuit;
    }

    const uint32_t OCULUS_MIRROR_SCALE = 2;
}

using namespace DirectX;

bool DisplayOculusVR( const wchar_t* packagePath )
{
    VisualizerResources resources;

    ::ovrResult oculusResult = ::ovr_Initialize( nullptr );

    if ( OVR_FAILURE( oculusResult ) )
    {
        //::MessageBoxW( nullptr, L"Can't initialize oculus SDK", L"Oculus Error", MB_OK | MB_ICONERROR );
        return false;
    }

    OculusAutoShutdown shutdown;

    if ( resources.OpenWindow( 0, 0 ) )
    {
        OculusSession         oculusSession;
        ::ovrGraphicsLuid     adapterLUID;
        ::ovrResult           result        = ::ovr_Create( &oculusSession.Session, &adapterLUID );

        if ( OVR_FAILURE( result ) )
        {
            //resources.ShowError( L"Failed to find hmd/create oculus session", L"Oculus Error" );
            return false;
        }

        ::ovrHmdDesc hmdDesc = ::ovr_GetHmdDesc( oculusSession.Session );

        resources.Resize( static_cast<uint32_t>( hmdDesc.Resolution.w / OCULUS_MIRROR_SCALE ),
                          static_cast<uint32_t>( hmdDesc.Resolution.h / OCULUS_MIRROR_SCALE ) );

        bool deviceOpen = resources.CreateD3D( reinterpret_cast<const LUID*>( &adapterLUID ) );

        if ( !deviceOpen )
        {
            return true;
        }

        bool packageLoaded = resources.LoadPackage( packagePath );

        if ( !packageLoaded )
        {
            return true;
        }

        OculusSwapChain swapChains[ 2 ];
        ::ovrRecti      viewports[ 2 ];

        bool swapChainCreated = true;

        for ( uint32_t eyeIndex = 0; eyeIndex < 2 && swapChainCreated; ++eyeIndex )
        {
            ::ovrEyeType eye       = static_cast< ::ovrEyeType >( eyeIndex );
            ::ovrSizei   idealSize = 
                ::ovr_GetFovTextureSize( oculusSession.Session, 
                                         eye, 
                                         hmdDesc.DefaultEyeFov[ eyeIndex ], 
                                         1.0f );

            swapChainCreated = 
                swapChains[ eye ].Create( oculusSession.Session, 
                                          resources.Device, 
                                          idealSize.w, 
                                          idealSize.h );

            viewports[ eyeIndex ].Pos.x = 0;
            viewports[ eyeIndex ].Pos.y = 0;
//            viewports[ eyeIndex ].Size  = idealSize;
            viewports[ eyeIndex ].Size.w = idealSize.w;
            viewports[ eyeIndex ].Size.h = idealSize.h;
        }

        if ( !swapChainCreated )
        {
            resources.ShowError( L"Failed to create oculus swap chain.", L"Oculus Error" );
            return true;
        }

        ::ovrMirrorTextureDesc mirrorTextureDesc = {};
        OculusMirrorTexture    mirrorTexture( oculusSession.Session );

        mirrorTextureDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
        mirrorTextureDesc.Width  = resources.Width;
        mirrorTextureDesc.Height = resources.Height;

        ::ovrResult mirrorTextureResult = 
            ::ovr_CreateMirrorTextureDX( oculusSession.Session, 
                                         resources.Device, 
                                         &mirrorTextureDesc, 
                                         &mirrorTexture.Texture );

        if ( OVR_FAILURE( mirrorTextureResult ) )
        {
            resources.ShowError( L"Failed to create oculus mirror texture.", L"Oculus Error" );
            return true;
        }

        long long          frameIndex      = 0;
        PerFrameParameters frameParameters = {};

        frameParameters.BufferMemory            = resources.BufferMemory;
        frameParameters.BufferMemorySize        = BufferSize;
        frameParameters.ConstantBuffer          = resources.ConstantBuffer;
        frameParameters.Effect                  = 0;
        frameParameters.Constants.Time          = 0;
        frameParameters.Constants.TransitionIn  = 1.0f;
        frameParameters.Constants.TransitionOut = 1.0f;

        resources.Effects->RenderInitialTextures( frameParameters );
        
        ::ovr_SetTrackingOriginType( oculusSession.Session, ovrTrackingOrigin_FloorLevel );

        AudioProcessing audio;

        Clock clock;

        if ( !audio.Initialize( frameParameters.Constants ) )
        {
            resources.ShowError( L"Couldn't initialize audio capture.", L"Audio capture error." );
            return true;
        }
        
        int32_t      effect            = 0;
        int32_t      previousLeftDown  = 0;
        int32_t      previousRightDown = 0;
        unsigned int previousButtons   = 0;

        bool isRenderEnabled = true;

        if ( !resources.CreateSoundTexture( audio ) )
        {
            return true;
        }

        frameParameters.SoundTextureSRV = resources.SoundTextureSRV;

        bool hasFirstAudioUpdate = false;

        while ( PumpMessages() )
        {
            ::ovrInputState inputState = {};

            ::ovr_GetInputState( oculusSession.Session, ::ovrControllerType::ovrControllerType_Active, &inputState );

            if ( resources.LeftDown != previousLeftDown || ( ( inputState.Buttons & ~previousButtons ) & ( ::ovrButton_Left | ::ovrButton_Back ) ) > 0 )
            {
                --effect;

                if ( effect < 0 )
                {
                    effect = static_cast< int32_t >( resources.Effects->EffectCount() - 1 );
                }
            }

            if ( resources.RightDown != previousRightDown || ( ( inputState.Buttons & ~previousButtons ) & ( ::ovrButton_Right | ::ovrButton_A ) ) > 0 )
            {
                ++effect;

                if ( effect >= static_cast< int32_t >( resources.Effects->EffectCount() ) )
                {
                    effect = 0;
                }
            }

            if ( effect != frameParameters.Effect )
            {
                clock.Reset();
            }

            frameParameters.Effect = effect;

            previousButtons   = inputState.Buttons;
            previousLeftDown  = resources.LeftDown;
            previousRightDown = resources.RightDown;


            AudioUpdateResult audioUpdateResult = audio.Update( frameParameters.Constants );

            if ( audioUpdateResult == AudioUpdateResult::AUDIO_ERROR )
            {
                resources.ShowError( L"Error pulling audio.", L"Audio capture error." );
                return true;
            }
            else if ( audioUpdateResult == AudioUpdateResult::UPDATED )
            {
                D3D11_MAPPED_SUBRESOURCE subResource;

                HRESULT mappingResult =
                    resources.Context->Map( resources.SoundTexture, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &subResource );

                ::memcpy( subResource.pData, audio.AudioTextureData(), audio.SamplesPerPeriod() * sizeof( float ) * 4 );

                resources.Context->Unmap( resources.SoundTexture, 0 );

                hasFirstAudioUpdate = true;
            }

            if ( !hasFirstAudioUpdate )
            {
                ::Sleep( 1 );
                continue;
            }

            ID3D11Device*        device  = resources.Device;
            ID3D11DeviceContext* context = resources.Context;

            ::ovrEyeRenderDesc eyeDesc[ 2 ];

            eyeDesc[ 0 ] = ::ovr_GetRenderDesc( oculusSession.Session, ovrEye_Left, hmdDesc.DefaultEyeFov[ 0 ] );
            eyeDesc[ 1 ] = ::ovr_GetRenderDesc( oculusSession.Session, ovrEye_Right, hmdDesc.DefaultEyeFov[ 1 ] );

            double sensorSampleTime;

            ::ovrPosef    eyePoses[ 2 ];
            ::ovrVector3f eyeOffsets[]  = { eyeDesc[ 0 ].HmdToEyeOffset, eyeDesc[ 1 ].HmdToEyeOffset };

            ::ovr_GetEyePoses( oculusSession.Session, frameIndex, ovrTrue, eyeOffsets, eyePoses, &sensorSampleTime );

            PerViewParameters viewParameters[ 2 ];

            float time = static_cast< float >( clock.GetElapsedTime() );

            frameParameters.Constants.DeltaTime = time - frameParameters.Constants.Time;
            frameParameters.Constants.Time      = time;

            if ( isRenderEnabled )
            {
                for ( uint32_t eyeIndex = 0; eyeIndex < 2; ++eyeIndex )
                {
                    ID3D11RenderTargetView* target   = swapChains[ eyeIndex ].CurrentTarget();
                    const ::ovrRecti&       viewport = viewports[ eyeIndex ];
                    PerViewParameters&      view     = viewParameters[ eyeIndex ];
                    const ::ovrFovPort&     fov      = eyeDesc[ eyeIndex ].Fov;
                    const ::ovrPosef&       eyePose  = eyePoses[ eyeIndex ];

                    view.Left = viewport.Pos.x;
                    view.Top = viewport.Pos.y;
                    view.Width = viewport.Size.w;
                    view.Height = viewport.Size.h;

                    view.Target = target;
                    view.Constants.EyePosition[ 0 ] = eyePose.Position.x;
                    view.Constants.EyePosition[ 1 ] = eyePose.Position.y;
                    view.Constants.EyePosition[ 2 ] = -eyePose.Position.z; // change handedness
                    view.Constants.EyePosition[ 3 ] = 1.0f;

                    // Create rotation with x and y relative rotation axises flip
                    XMVECTOR rotation = XMVectorSet( -eyePose.Orientation.x, -eyePose.Orientation.y, eyePose.Orientation.z, eyePose.Orientation.w );
                    // Upper left corner of the view frustum at z of 1. Note, z positive, left handed. 
                    XMVECTOR rayScreenUpperLeft = XMVector3Rotate( XMVectorSet( -fov.LeftTan, fov.UpTan, 1.0f, 0.0f ), rotation );
                    // Right direction scaled to width of the frustum at z of 1.
                    XMVECTOR rayScreenRight = XMVector3Rotate( XMVectorSet( fov.LeftTan + fov.RightTan, 0.0f, 0.0f, 0.0f ), rotation );
                    // Down direction scaled to height of the frustum at z of 1.
                    XMVECTOR rayScreenDown = XMVector3Rotate( XMVectorSet( 0.0f, -( fov.DownTan + fov.UpTan ), 0.0f, 0.0f ), rotation ); // top to bottom screen

                    XMStoreFloat4( reinterpret_cast<XMFLOAT4*>( view.Constants.RayScreenUpperLeft ), XMVectorSetW( rayScreenUpperLeft, 0.0f ) );
                    XMStoreFloat4( reinterpret_cast<XMFLOAT4*>( view.Constants.RayScreenRight ), XMVectorSetW( rayScreenRight, 0.0f ) );
                    XMStoreFloat4( reinterpret_cast<XMFLOAT4*>( view.Constants.RayScreenDown ), XMVectorSetW( rayScreenDown, 0.0f ) );
                }

                bool renderResult = resources.Effects->Render( frameParameters, viewParameters, 2 );

                if ( !renderResult )
                {
                    resources.ShowError( L"Error rendering frame", L"Rendering Error" );
                    return true;
                }
            }

            ::ovrLayerEyeFov mainLayer = {};

            mainLayer.Header.Type  = ::ovrLayerType_EyeFov;
            mainLayer.Header.Flags = 0;

            for ( int eyeIndex = 0; eyeIndex < 2; ++eyeIndex )
            {
                swapChains[ eyeIndex ].Commit();

                mainLayer.ColorTexture[ eyeIndex ] = swapChains[ eyeIndex ].Chain;
                mainLayer.Viewport[ eyeIndex ]     = viewports[ eyeIndex ];
                mainLayer.Fov[ eyeIndex ]          = eyeDesc[ eyeIndex ].Fov;
                mainLayer.RenderPose[ eyeIndex ]   = eyePoses[ eyeIndex ];
            }

            mainLayer.SensorSampleTime = sensorSampleTime;

            ::ovrLayerHeader* layers      = &mainLayer.Header;
            ::ovrResult       frameResult = ::ovr_SubmitFrame( oculusSession.Session, frameIndex, nullptr, &layers, 1 );

            if ( !OVR_SUCCESS( frameResult ) )
            {
                resources.ShowError( L"Error submitting frame to oculus SDK", L"Oculus SDK Error" );
                return true;
            }

            isRenderEnabled = frameResult == ::ovrSuccess;

            ++frameIndex;

            COMAutoPtr< ID3D11Texture2D > outputTexture;

            ::ovr_GetMirrorTextureBufferDX( oculusSession.Session, mirrorTexture.Texture, IID_PPV_ARGS( &outputTexture.raw ) );

            resources.Context->CopyResource( resources.BackBuffer, outputTexture.raw );

            resources.SwapChain->Present( 0, 0 );
        }

    }

    return true;
}

void DisplayWindowed( const wchar_t* packagePath, uint32_t width, uint32_t height, float fovInDegrees )
{
    VisualizerResources resources;

    bool isWindowOpen = resources.OpenWindow( width, height );

    if ( !isWindowOpen )
    {
        return;
    }

    resources.ShowWindow();
    
    bool isDeviceCreated = resources.CreateD3D();

    if ( !isDeviceCreated )
    {
        return;
    }

    bool packageLoaded = resources.LoadPackage( packagePath );

    if ( !packageLoaded )
    {
        return;
    }

    PerViewParameters  viewParameters  = {};
    PerFrameParameters frameParameters = {};

    viewParameters.Target = resources.BackBufferTarget;
    viewParameters.Width  = resources.Width;
    viewParameters.Height = resources.Height;
    viewParameters.Left   = 0;
    viewParameters.Top    = 0;

    float halfFovHorizontalTan = ::tanf( 0.5f * static_cast< float >( M_PI ) * fovInDegrees / 180.0f );
    float halfFovVerticalTan   = halfFovHorizontalTan * ( static_cast<float>( resources.Height ) / static_cast<float>( resources.Width ) );

    viewParameters.Constants.RayScreenUpperLeft[ 0 ] = -halfFovHorizontalTan;
    viewParameters.Constants.RayScreenUpperLeft[ 1 ] = halfFovVerticalTan;
    viewParameters.Constants.RayScreenUpperLeft[ 2 ] = 1.0f;
    viewParameters.Constants.RayScreenUpperLeft[ 3 ] = 0.0f;

    viewParameters.Constants.RayScreenRight[ 0 ] = 2.0f * halfFovHorizontalTan;
    viewParameters.Constants.RayScreenRight[ 1 ] = 0.0f;
    viewParameters.Constants.RayScreenRight[ 2 ] = 0.0f;
    viewParameters.Constants.RayScreenRight[ 3 ] = 0.0f;

    viewParameters.Constants.RayScreenDown[ 0 ] = 0.0f;
    viewParameters.Constants.RayScreenDown[ 1 ] = -2.0f * halfFovVerticalTan;
    viewParameters.Constants.RayScreenDown[ 2 ] = 0.0f;
    viewParameters.Constants.RayScreenDown[ 3 ] = 0.0f;

    frameParameters.BufferMemory            = resources.BufferMemory;
    frameParameters.BufferMemorySize        = BufferSize;
    frameParameters.ConstantBuffer          = resources.ConstantBuffer;
    frameParameters.Effect                  = 0;
    frameParameters.Constants.Time          = 0;
    frameParameters.Constants.TransitionIn  = 1.0f;
    frameParameters.Constants.TransitionOut = 1.0f;
    frameParameters.Constants.SoundRMS[ 0 ] = 0;
    frameParameters.Constants.SoundRMS[ 1 ] = 0;

    PumpMessages();
       
    resources.Effects->RenderInitialTextures( frameParameters );

    AudioProcessing audio;

    Clock clock;

    if ( !audio.Initialize( frameParameters.Constants ) )
    {
        resources.ShowError( L"Couldn't initialize audio capture.", L"Audio capture error." );
        return;
    }
    
    int32_t previousLeftDown  = 0;
    int32_t previousRightDown = 0;
    int32_t effect            = 0;

    if ( !resources.CreateSoundTexture( audio ) )
    {
        return;
    }

    frameParameters.SoundTextureSRV = resources.SoundTextureSRV;

    bool hasFirstAudioUpdate = false;

    while ( PumpMessages() )
    {
        if ( resources.LeftDown != previousLeftDown )
        {
            --effect;

            if ( effect < 0 )
            {
                effect = static_cast< int32_t >( resources.Effects->EffectCount() ) - 1;
            }
        }

        if ( resources.RightDown != previousRightDown )
        {
            ++effect;

            if ( effect >= static_cast< int32_t >( resources.Effects->EffectCount() ) )
            {
                effect = 0;
            }
        }

        if ( effect != frameParameters.Effect )
        {
            clock.Reset();
        }

        previousLeftDown  = resources.LeftDown;
        previousRightDown = resources.RightDown;

        AudioUpdateResult audioUpdateResult = audio.Update( frameParameters.Constants );
        
        if ( audioUpdateResult == AudioUpdateResult::AUDIO_ERROR )
        {
            resources.ShowError( L"Error pulling audio.", L"Audio capture error." );
            return;
        }
        else if ( audioUpdateResult == AudioUpdateResult::UPDATED )
        {
            D3D11_MAPPED_SUBRESOURCE subResource;

            HRESULT mappingResult =
                resources.Context->Map( resources.SoundTexture, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &subResource );

            ::memcpy( subResource.pData, audio.AudioTextureData(), audio.SamplesPerPeriod() * sizeof( float ) * 4 );

            resources.Context->Unmap( resources.SoundTexture, 0 );

            hasFirstAudioUpdate = true;
        }

        if ( !hasFirstAudioUpdate )
        {
            ::Sleep( 1 );
            continue;
        }

        float time = static_cast< float >( clock.GetElapsedTime() );

        frameParameters.Constants.DeltaTime = time - frameParameters.Constants.Time;
        frameParameters.Constants.Time      = time;
        frameParameters.Effect              = static_cast< uint32_t >( effect );

        viewParameters.Constants.EyePosition[ 0 ] = 0.0f;
        viewParameters.Constants.EyePosition[ 1 ] = 1.3f;
        viewParameters.Constants.EyePosition[ 2 ] = 0.25f;
        viewParameters.Constants.EyePosition[ 3 ] = 1.0f;

        float red[] = { 1.0f, 0.0, 0.0f, 1.0f };

        resources.Context->ClearRenderTargetView( resources.BackBufferTarget, red );

        bool renderResult = resources.Effects->Render( frameParameters, &viewParameters, 1 );

        if ( !renderResult )
        {
            resources.ShowError( L"Error rendering frame", L"Rendering Error" );
            return;
        }

        resources.SwapChain->Present( 1, 0 );
    }
}
