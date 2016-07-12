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
        : Chain_( nullptr ),
        Session_( nullptr ),
        TextureCount_( 0 ),
        RenderTargets_( nullptr ) {}

    bool Create( ::ovrSession session, ID3D11Device* device, uint32_t width, uint32_t height );
        
    ~OculusSwapChain();

private:

    ::ovrTextureSwapChain                 Chain_;
    ::ovrSession                          Session_;
    COMAutoPtr< ID3D11RenderTargetView >* RenderTargets_;
    int                                   TextureCount_;

};

#endif // -- BOONDOGGLE_OCULUS_HELPERS_H__