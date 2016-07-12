#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdint.h>
#include "visualizer.h"
#include "../common/boondoggle_helpers.h"

namespace
{
    const WCHAR* const DisplayWindowClassName = L"Boondoggle";
    const WCHAR* const DisplayWindowTitle     = L"Boondoggle";

    struct DisplayWindow
    {
        HINSTANCE               ModuleInstance;
        HWND                    WindowHandle;
        ID3D11Device*           Device;
        ID3D11DeviceContext*    Context;
        IDXGISwapChain*         SwapChain;
        ID3D11Texture2D*        BackBuffer;
        ID3D11RenderTargetView* BackBufferTarget;
        LONG                    Width;
        LONG                    Height;

        DisplayWindow();

        bool OpenWindow( uint32_t width, uint32_t height );

        bool OpenDevice( /*optional*/ const LUID* luid = nullptr );
        
        void Show();

        LRESULT WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );

        void ShowError( LPCWSTR message, LPCWSTR caption );

//        void Resize();

        ~DisplayWindow();

        DisplayWindow( const DisplayWindow& ) = delete;

        DisplayWindow& operator=( const DisplayWindow& ) = delete;
    };

    DisplayWindow::DisplayWindow() 
        : WindowHandle( nullptr ),
          Device( nullptr ),
          Context( nullptr )
    {
    }
    
    DisplayWindow::~DisplayWindow()
    {
        COMRelease( BackBufferTarget );
        COMRelease( BackBuffer );
        COMRelease( SwapChain );
        COMRelease( Context );
        COMRelease( Device );

        if ( WindowHandle != nullptr )
        {
            ::DestroyWindow( WindowHandle );
            
            WindowHandle = nullptr;

            ::UnregisterClassW( DisplayWindowClassName, ModuleInstance );
        }
    }

    void DisplayWindow::Show()
    {
        if ( WindowHandle != nullptr )
        {
            ::ShowWindow( WindowHandle, SW_SHOW );
        }
    }

    LRESULT CALLBACK DisplayWindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
    {
        DisplayWindow* window = reinterpret_cast< DisplayWindow* >( ::GetWindowLongPtr( hWnd, 0 ) );

        if ( window != nullptr )
        {
            return window->WindowProc( uMsg, wParam, lParam );
        }
        else
        {
            return ::DefWindowProcW( hWnd, uMsg, wParam, lParam );
        }
    }

    LRESULT DisplayWindow::WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
    {
        LRESULT result = 0;

        switch ( uMsg )
        {
        case WM_SIZE:


            break;

        case WM_KEYDOWN:

            if ( wParam == VK_ESCAPE )
            {
                ::DestroyWindow( WindowHandle );
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
            
    bool DisplayWindow::OpenWindow( uint32_t width, uint32_t height )
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
        WNDCLASSW windowClass;

        ::ZeroMemory( &windowClass, sizeof( WNDCLASSW ) );

        windowClass.style         = CS_OWNDC;
        windowClass.lpfnWndProc   = DisplayWindowProc;
        windowClass.cbClsExtra    = 0;
        windowClass.cbWndExtra    = sizeof( this );
        windowClass.hInstance     = ModuleInstance;
        windowClass.hIcon         = icon;
        windowClass.hCursor       = ::LoadCursorA( NULL, IDC_ARROW );
        windowClass.hbrBackground = ( HBRUSH )::GetStockObject( BLACK_BRUSH );
        windowClass.lpszMenuName  = 0;
        windowClass.lpszClassName = DisplayWindowClassName;

        ::RegisterClassW( &windowClass );

        RECT inner;
         
        inner.left   = 0;
        inner.top    = 0;
        inner.right  = width;
        inner.bottom = height;

        ::AdjustWindowRect( &inner, WS_OVERLAPPEDWINDOW, false );

        WindowHandle =
            ::CreateWindowW( DisplayWindowClassName,
                             DisplayWindowTitle,
                             WS_OVERLAPPEDWINDOW,
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

    void DisplayWindow::ShowError( LPCWSTR message, LPCWSTR caption )
    {
        ::MessageBoxW( WindowHandle, message, caption, MB_OK | MB_ICONERROR );
    }

    bool DisplayWindow::OpenDevice( const LUID* luid )
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

        HRESULT deviceResult = 
            ::D3D11CreateDevice( adapter.raw, 
                                 D3D_DRIVER_TYPE_HARDWARE, 
                                 nullptr, 
                                 0, 
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

        DXGI_SWAP_CHAIN_DESC swapChainDesc;
        
        ::ZeroMemory( &swapChainDesc, sizeof( swapChainDesc ) );
        
        swapChainDesc.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.BufferCount                        = 2;
        swapChainDesc.BufferDesc.Width                   = Width;
        swapChainDesc.BufferDesc.Height                  = Height;
        swapChainDesc.BufferDesc.RefreshRate.Numerator   = 0;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDesc.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
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

        Context->OMSetRenderTargets( 1, &BackBufferTarget, nullptr );

        float black[] = { 1.0f, 0.0f, 0.0f, 0.0f };

        Context->ClearRenderTargetView( BackBufferTarget, black );

        SwapChain->Present( 0, 0 );

        return true;
    }


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
}

void Boondoggle::DisplayOculusVR()
{
    DisplayWindow window;
}

void Boondoggle::DisplayWindowed( uint32_t width, uint32_t height )
{
    DisplayWindow window;

    if ( window.OpenWindow( width, height ) )
    {
        window.Show();

        window.OpenDevice();

        while ( PumpMessages() )
        {

        }
    }
}
