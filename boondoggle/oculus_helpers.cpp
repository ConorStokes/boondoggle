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

    ::ovrResult swapChainResult = ::ovr_CreateTextureSwapChainDX( session, device, &swapChainDesc, &Chain );

    if ( OVR_FAILURE( swapChainResult ) )
    {
        return false;
    }

    ::ovrResult chainLengthResult = ovr_GetTextureSwapChainLength( session, Chain, &TextureCount_ );

    if ( OVR_FAILURE( chainLengthResult ) )
    {
        return false;
    }

    RenderTargets_ = new COMAutoPtr< ID3D11RenderTargetView >[ TextureCount_ ];

    for ( int textureIndex = 0; textureIndex < TextureCount_; ++textureIndex )
    {
        COMAutoPtr< ID3D11Texture2D > texture;

        ::ovrResult textureResult = ::ovr_GetTextureSwapChainBufferDX( session, Chain, textureIndex, IID_PPV_ARGS( &texture.raw ) );

        if ( OVR_FAILURE( textureResult ) )
        {
            return false;
        }

        D3D11_RENDER_TARGET_VIEW_DESC targetDesc = {};

        targetDesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        targetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

        HRESULT targetResult = device->CreateRenderTargetView( texture.raw, &targetDesc, &RenderTargets_[ textureIndex ].raw );

        if ( targetResult != ERROR_SUCCESS )
        {
            return false;
        }
    }

    return true;
}

ID3D11RenderTargetView* OculusSwapChain::CurrentTarget()
{
    ID3D11RenderTargetView* result = nullptr;
    int                     index  = 0;

    if ( Chain != nullptr && RenderTargets_ != nullptr )
    {
        ::ovrResult currentIndexResult =
            ::ovr_GetTextureSwapChainCurrentIndex( Session_, Chain, &index );

        if ( OVR_SUCCESS( currentIndexResult ) )
        {
            result = RenderTargets_[ index ].raw;
        }
    }

    return result;
}

OculusSwapChain::~OculusSwapChain()
{
    if ( RenderTargets_ != nullptr )
    {
        delete[] RenderTargets_;
        RenderTargets_ = nullptr;
    }

    TextureCount_ = 0;

    if ( Chain != nullptr )
    {
        ::ovr_DestroyTextureSwapChain( Session_, Chain );
        Chain = nullptr;
    }
}