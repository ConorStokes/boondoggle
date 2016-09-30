#include <windows.h>
#include "visualizer.h"

int wmain( int argc, const wchar_t** argv )
{
    const wchar_t* packageFile = L"example.bdg";
    
    if ( argc >= 2 )
    {
        packageFile = argv[ 1 ];
    }
    
    bool oculusResult = DisplayOculusVR( packageFile );

    if ( !oculusResult )
    {
        uint32_t width  = static_cast< uint32_t >( ( GetSystemMetrics( SM_CXSCREEN ) * 5 ) / 6 );
        uint32_t height = static_cast< uint32_t >( ( GetSystemMetrics( SM_CYSCREEN ) * 5 ) / 6 );

        DisplayWindowed( packageFile, width, height, 80.0f );
    }
    
    return 0;
}