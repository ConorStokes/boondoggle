#include "oculus_helpers.h"

bool OculusSwapChain::Create( ::ovrSession session, ID3D11Device* device, uint32_t width, uint32_t height )
{
    Session_ = session;

    ::ovrTextureSwapChainDesc swapChainDesc = {};

    swapChainDesc.Type        = ::ovrTexture_2D;
    swapChainDesc.Width       = static_cast< int >( width );
    swapChainDesc.Height      = static_cast< int >( height );
    swapChainDesc.ArraySize   = 1;
    swapChainDesc.MipLevels   = 1;
    swapChainDesc.Format      = ::OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
    swapChainDesc.SampleCount = 1;
    swapChainDesc.BindFlags   = ::ovrTextureBind_DX_RenderTarget;
    swapChainDesc.MiscFlags   = ::ovrTextureMisc_None;
    swapChainDesc.StaticImage = ovrFalse;

    ::ovrResult swapChainResult = ::ovr_CreateTextureSwapChainDX( session, device, &swapChainDesc, &Chain_ );

    if ( OVR_FAILURE( swapChainResult ) )
    {
        return false;
    }

    ::ovrResult chainLengthResult = ovr_GetTextureSwapChainLength( session, Chain_, &TextureCount_ );

    if ( OVR_FAILURE( chainLengthResult ) )
    {
        return false;
    }

    RenderTargets_ = new COMAutoPtr< ID3D11RenderTargetView >[ TextureCount_ ];

    for ( int textureIndex = 0; textureIndex < TextureCount_; ++textureIndex )
    {
        COMAutoPtr< ID3D11Texture2D > texture;

        ::ovrResult textureResult = ::ovr_GetTextureSwapChainBufferDX( session, Chain_, textureIndex, IID_PPV_ARGS( &texture.raw ) );

        if ( OVR_FAILURE( textureResult ) )
        {
            return false;
        }

        device->CreateRenderTargetView( texture.raw, nullptr, &RenderTargets_[ textureIndex ].raw );
    }

    return true;
}

OculusSwapChain::~OculusSwapChain()
{
    if ( RenderTargets_ != nullptr )
    {
        delete[] RenderTargets_;
        RenderTargets_ = nullptr;
    }

    TextureCount_ = 0;

    if ( Chain_ != nullptr )
    {
        ::ovr_DestroyTextureSwapChain( Session_, Chain_ );
        Chain_ = nullptr;
    }
}
