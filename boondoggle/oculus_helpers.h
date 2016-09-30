#ifndef BOONDOGGLE_OCULUS_HELPERS_H__
#define BOONDOGGLE_OCULUS_HELPERS_H__

#include <stdint.h>
#include "../common/boondoggle_helpers.h"
#include "OVR_CAPI_D3D.h"
#include "d3d11.h"

#pragma once

class OculusSwapChain
{
public:

    OculusSwapChain()
        : Chain( nullptr ),
        Session_( nullptr ),
        TextureCount_( 0 ),
        RenderTargets_( nullptr ) {}

    bool Create( ::ovrSession session, ID3D11Device* device, uint32_t width, uint32_t height );
        
    ID3D11RenderTargetView* CurrentTarget();

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

struct OculusAutoShutdown
{
    OculusAutoShutdown() {}

    OculusAutoShutdown( const OculusAutoShutdown& ) = delete;

    OculusAutoShutdown& operator=( const OculusAutoShutdown& ) = delete;

    ~OculusAutoShutdown() { ::ovr_Shutdown(); }
};

#endif // -- BOONDOGGLE_OCULUS_HELPERS_H__