#ifndef BOONDOGGLE_OCULUS_HELPERS_H__
#define BOONDOGGLE_OCULUS_HELPERS_H__

#include <stdint.h>
#include "../common/boondoggle_helpers.h"
#include "OVR_CAPI_D3D.h"
#include "d3d11.h"

#pragma once

// Creation and auto-destruction for the swap chain, also implements basic functions to get the 
// right render target view from the swap chain.
class OculusSwapChain
{
public:

    OculusSwapChain()
        : Chain( nullptr ),
        Session_( nullptr ),
        TextureCount_( 0 ),
        RenderTargets_( nullptr ) {}

    // Create the swap chain
    bool Create( ::ovrSession session, ID3D11Device* device, uint32_t width, uint32_t height );
        
    // Get the current render target on a created swap chain.
    ID3D11RenderTargetView* CurrentTarget();

    // Commit the current texture in the swap chain.
    void Commit() { ::ovr_CommitTextureSwapChain( Session_, Chain ); }

    ~OculusSwapChain();
    
    OculusSwapChain( const OculusSwapChain& ) = delete;

    OculusSwapChain& operator=( const OculusSwapChain& ) = delete;

    ::ovrTextureSwapChain                 Chain;

private:

    ::ovrSession                          Session_;
    COMAutoPtr< ID3D11RenderTargetView >* RenderTargets_;
    int                                   TextureCount_;

};

// Auto-destruction for the oculus session.
struct OculusSession
{
    ::ovrSession Session;

    OculusSession() : Session( nullptr ) { }

    OculusSession( const OculusSession& ) = delete;

    OculusSession& operator=( const OculusSession& ) = delete;

    ~OculusSession() 
    { 
        if ( Session != nullptr )
        {
            ::ovr_Destroy( Session ); 
        }
    }
};

// Auto destruction for the mirror texture.
struct OculusMirrorTexture
{
    ::ovrMirrorTexture Texture;
    ::ovrSession       Session;

    OculusMirrorTexture( ::ovrSession session ) : Texture( nullptr ), Session( session ) {}

    OculusMirrorTexture( const OculusMirrorTexture& ) = delete;

    OculusMirrorTexture& operator=( const OculusMirrorTexture& ) = delete;

    ~OculusMirrorTexture()
    {
        if ( Texture != nullptr )
        {
            ::ovr_DestroyMirrorTexture( Session, Texture );
        }
    }
};

// Auto shutdown for the oculus sdk initialization.
struct OculusAutoShutdown
{
    OculusAutoShutdown() {}

    OculusAutoShutdown( const OculusAutoShutdown& ) = delete;

    OculusAutoShutdown& operator=( const OculusAutoShutdown& ) = delete;

    ~OculusAutoShutdown() { ::ovr_Shutdown(); }
};

#endif // -- BOONDOGGLE_OCULUS_HELPERS_H__