#include <d3d11_1.h>
#include <windows.h>
#include "visual_effects.h"
#include "../common/binary_effects_format.h"
#include "../common/boondoggle_helpers.h"
#include "../external/ddstextureloader/DDSTextureLoader.h"
#include <float.h>

namespace
{
    struct PerRenderConstants
    {
        float Resolution[ 2 ];
        float InverseResolution[ 2 ];
    };
    
    void UpdatePerRender( const PerRenderConstants& constants, uint8_t* buffer )
    {
        ::memcpy( buffer + sizeof( PerFrameConstants ), &constants, sizeof( PerRenderConstants ) );
    }


    void UpdatePerFrame( const PerFrameConstants& constants, uint8_t* buffer )
    {
        ::memcpy( buffer, &constants, sizeof( PerFrameConstants ) );
    }


    void UpdatePerView( const PerViewConstants& constants, uint8_t* buffer )
    {
        ::memcpy( buffer + sizeof( PerFrameConstants ) + sizeof( PerRenderConstants ), &constants, sizeof( PerViewConstants ) );
    }


    D3D11_TEXTURE_ADDRESS_MODE ToAddressMode( TextureAddressMode mode )
    {
        switch ( mode )
        {
        case TextureAddressMode::CLAMP:

            return D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_CLAMP;

        case TextureAddressMode::MIRROR:

            return D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_MIRROR;

        case TextureAddressMode::MIRROR_ONCE:

            return D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;

        case TextureAddressMode::WRAP:

            return D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;

        }

        return D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_CLAMP;
    }
}


BoondoggleEffectsPackage::~BoondoggleEffectsPackage()
{
    if ( Package_ != nullptr )
    {
        ::UnmapViewOfFile( reinterpret_cast<const void*>( Package_ ) );
        Package_ = nullptr;
    }

    if ( FileHandle_ != nullptr )
    {
        ::CloseHandle( FileHandle_ );
        FileHandle_ = nullptr;
    }

    delete[] ProceduralTargets_;
    ProceduralTargets_ = nullptr;

    delete[] TextureViews_;
    TextureViews_ = nullptr;

    delete[] PixelShaders_;
    PixelShaders_ = nullptr;

    delete[] Samplers_;
    Samplers_ = nullptr;

    Device_ = nullptr;
    Context_ = nullptr;
}


uint32_t BoondoggleEffectsPackage::EffectCount() const
{
    return Package_->EffectCount;
}


bool BoondoggleEffectsPackage::RenderInitialTextures( const PerFrameParameters& frameParameters )
{
    const VisualEffect& effect = Package_->Effects[ frameParameters.Effect ];

    uint8_t* bufferMemory = frameParameters.BufferMemory;

    TextureViews_[ 0 ] = frameParameters.SoundTextureSRV;

    UpdatePerFrame( frameParameters.Constants, bufferMemory );

    Context_->RSSetState( nullptr );
    Context_->IASetVertexBuffers( 0, 0, nullptr, nullptr, nullptr );
    Context_->IASetIndexBuffer( nullptr, static_cast< DXGI_FORMAT >( 0 ), 0 );
    Context_->IASetInputLayout( nullptr );
    Context_->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    Context_->PSSetConstantBuffers( 0, 1, &frameParameters.ConstantBuffer );
    Context_->VSSetShader( ScreenAlignedQuadVS_.raw, nullptr, 0 );
    Context_->RSSetState( nullptr );

    for ( uint32_t proceduralIndex = 0; proceduralIndex < Package_->ProceduralTextureCount; ++proceduralIndex )
    {
        if ( Package_->ProceduralTextures[ proceduralIndex ].GenerateAtStart )
        {
            bool proceduralResult = RenderProcedural( frameParameters, proceduralIndex );

            if ( !proceduralResult )
            {
                return false;
            }
        }
    }

    return true;
}


bool BoondoggleEffectsPackage::RenderProcedural( const PerFrameParameters& frameParameters, uint32_t proceduralIndex )
{
    const ProceduralTexture& procedural = Package_->ProceduralTextures[ proceduralIndex ];

    Context_->OMSetRenderTargets( 1, &ProceduralTargets_[ proceduralIndex ].raw, nullptr );

    D3D11_VIEWPORT viewport =
    {
        0,
        0,
        static_cast<float>( procedural.Width ),
        static_cast<float>( procedural.Height ),
        0.0f,
        1.0f
    };

    Context_->RSSetViewports( 1, &viewport);
    Context_->PSSetShader( PixelShaders_[ procedural.ShaderId ].raw, nullptr, 0 );

    D3D11_MAPPED_SUBRESOURCE bufferMap;

    PerRenderConstants perRenderConstants = {};

    perRenderConstants.Resolution[ 0 ]        = static_cast< float >( procedural.Width );
    perRenderConstants.Resolution[ 1 ]        = static_cast< float >( procedural.Height );
    perRenderConstants.InverseResolution[ 0 ] = 1.0f / static_cast< float >( procedural.Width );
    perRenderConstants.InverseResolution[ 1 ] = 1.0f / static_cast< float >( procedural.Height );

    UpdatePerRender( perRenderConstants, frameParameters.BufferMemory );

    HRESULT mapResult = Context_->Map( frameParameters.ConstantBuffer, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &bufferMap );

    if ( mapResult != ERROR_SUCCESS )
    {
        return false;
    }

    ::memcpy( bufferMap.pData, frameParameters.BufferMemory, frameParameters.BufferMemorySize );

    Context_->Unmap( frameParameters.ConstantBuffer, 0 );

    for ( uint32_t sourceTextureIndex = 0; sourceTextureIndex < procedural.SourceTextureCount; ++sourceTextureIndex )
    {
        Context_->PSSetShaderResources( sourceTextureIndex, 1, &TextureViews_[ procedural.SourceTextures[ sourceTextureIndex ] ].raw );
    }

    for ( uint32_t sourceSamplerIndex = 0; sourceSamplerIndex < procedural.SourceSamplerCount; ++sourceSamplerIndex )
    {
        Context_->PSSetSamplers( sourceSamplerIndex, 1, &Samplers_[ procedural.SourceSamplers[ sourceSamplerIndex ] ].raw );
    }

    Context_->PSSetShaderResources( procedural.SourceTextureCount, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - procedural.SourceTextureCount, nullptr );
    Context_->Draw( 3, 0 );

    if ( procedural.GenerateMipMaps )
    {
        Context_->GenerateMips( TextureViews_[ 1 + Package_->StaticTextureCount + proceduralIndex ].raw );
    }

    return true;
}


bool BoondoggleEffectsPackage::Render( const PerFrameParameters& frameParameters, /* array */ const PerViewParameters* views, uint32_t viewCount )
{
    if ( frameParameters.Effect >= Package_->EffectCount )
    {
        return false;
    }

    TextureViews_[ 0 ] = frameParameters.SoundTextureSRV;

    const VisualEffect& effect = Package_->Effects[ frameParameters.Effect ];

    uint8_t* bufferMemory = frameParameters.BufferMemory;

    UpdatePerFrame( frameParameters.Constants, bufferMemory );

    ID3D11Buffer* vertexBuffer = nullptr;
    UINT          zero         = 0;

    Context_->OMSetDepthStencilState( DepthStencilState_.raw, 0 );
    Context_->RSSetState( RasterizerState_.raw );
    Context_->IASetVertexBuffers( 0, 1, &vertexBuffer, &zero, &zero );
    Context_->IASetIndexBuffer( nullptr, static_cast<DXGI_FORMAT>( 0 ), 0 );
    Context_->IASetInputLayout( nullptr );
    Context_->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    Context_->PSSetConstantBuffers( 0, 1, &frameParameters.ConstantBuffer );
    Context_->VSSetShader( ScreenAlignedQuadVS_.raw, nullptr, 0 );

    for ( uint32_t frameProceduralIndex = 0; frameProceduralIndex < effect.ProceduralTextureCount; ++frameProceduralIndex )
    {
        uint32_t proceduralIndex  = effect.ProceduralTextures[ frameProceduralIndex ];
        bool     proceduralResult = RenderProcedural( frameParameters, proceduralIndex );

        if ( !proceduralResult )
        {
            return false;
        }
    }

    for ( uint32_t sourceTextureIndex = 0; sourceTextureIndex < effect.SourceTextureCount; ++sourceTextureIndex )
    {
        Context_->PSSetShaderResources( sourceTextureIndex, 1, &TextureViews_[ effect.SourceTextures[ sourceTextureIndex ] ].raw );
    }

    for ( uint32_t sourceSamplerIndex = 0; sourceSamplerIndex < effect.SourceSamplerCount; ++sourceSamplerIndex )
    {
        Context_->PSSetSamplers( sourceSamplerIndex, 1, &Samplers_[ effect.SourceSamplers[ sourceSamplerIndex ] ].raw );
    }

    Context_->PSSetShader( PixelShaders_[ effect.ShaderId ].raw, nullptr, 0 );

    //Context_->PSSetShaderResources( effect.SourceTextureCount,
    //                                D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - effect.SourceTextureCount,
    //                                nullptr );

    for ( uint32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex )
    {
        const PerViewParameters& viewParameters     = views[ viewIndex ];
        PerRenderConstants       perRenderConstants = {};

        perRenderConstants.Resolution[ 0 ]        = static_cast< float >( viewParameters.Width );
        perRenderConstants.Resolution[ 1 ]        = static_cast< float >( viewParameters.Height );
        perRenderConstants.InverseResolution[ 0 ] = 1.0f / static_cast< float >( viewParameters.Width );
        perRenderConstants.InverseResolution[ 1 ] = 1.0f / static_cast< float >( viewParameters.Height );

        UpdatePerRender( perRenderConstants, frameParameters.BufferMemory );
        UpdatePerView( viewParameters.Constants, frameParameters.BufferMemory );

        D3D11_MAPPED_SUBRESOURCE bufferMap;

        HRESULT mapResult =
            Context_->Map( frameParameters.ConstantBuffer, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &bufferMap );

        if ( mapResult != ERROR_SUCCESS )
        {
            return false;
        }

        ::memcpy( bufferMap.pData, frameParameters.BufferMemory, frameParameters.BufferMemorySize );

        Context_->Unmap( frameParameters.ConstantBuffer, 0 );

        Context_->OMSetRenderTargets( 1, &viewParameters.Target, nullptr );

        D3D11_VIEWPORT          d3dViewport =
        {
            static_cast<float>( viewParameters.Left ),
            static_cast<float>( viewParameters.Top ),
            static_cast<float>( viewParameters.Width ),
            static_cast<float>( viewParameters.Height ),
            0.0f,
            1.0f
        };

        Context_->RSSetViewports( 1, &d3dViewport );
        Context_->Draw( 3, 0 );
    }

    return true;
}


bool BoondoggleEffectsPackage::CreateResources( ID3D11Device* device, ID3D11DeviceContext* context, HWND windowHandle, size_t textureMaxSize, const wchar_t* packageName )
{
    Device_     = device;
    Context_    = context;
    FileHandle_ = ::CreateFileW( packageName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr );

    if ( FileHandle_ == INVALID_HANDLE_VALUE || FileHandle_ == nullptr )
    {
        ::MessageBoxW( windowHandle, L"Couldn't open package file", L"Package Load Error", MB_OK | MB_ICONERROR );
        return false;
    }

    LARGE_INTEGER fileSize;
    BOOL          fileSizeResult = ::GetFileSizeEx( FileHandle_, &fileSize );

    if ( !fileSizeResult )
    {
        ::MessageBoxW( windowHandle, L"Couldn't get file size", L"Package Load Error", MB_OK | MB_ICONERROR );
        return false;
    }

    PackageSize_ = static_cast< size_t >( fileSize.QuadPart );

    HANDLE fileMappingHandle = ::CreateFileMappingW( FileHandle_, nullptr, PAGE_READONLY, 0, 0, nullptr );

    if ( fileMappingHandle == nullptr )
    {
        ::MessageBoxW( windowHandle, L"Couldn't create file package mapping", L"Package Load Error", MB_OK | MB_ICONERROR );
        return false;
    }

    void* filePointer = ::MapViewOfFile( fileMappingHandle, FILE_MAP_READ, 0, 0, PackageSize_ );

    Package_ = reinterpret_cast< const BoondogglePackageHeader* >( filePointer );

    ::CloseHandle( fileMappingHandle );

    if ( Package_ == nullptr ||
         !ValidatePackage( *Package_, reinterpret_cast< const uint8_t* >( filePointer ) + PackageSize_ ) )
    {
        ::MessageBoxW( windowHandle, L"Package file not valid", L"Package Load Error", MB_OK | MB_ICONERROR );
        return false;
    }

    TextureViews_ = new COMAutoPtr< ID3D11ShaderResourceView >[ 1 + Package_->StaticTextureCount + Package_->ProceduralTextureCount ];

    for ( uint32_t textureIndex = 0; textureIndex < Package_->StaticTextureCount; ++textureIndex )
    {
        HRESULT textureCreationResult =
            DirectX::CreateDDSTextureFromMemory( device,
                                                 context,
                                                 Package_->StaticTextures[ textureIndex ].Data,
                                                 Package_->StaticTextures[ textureIndex ].ResourceSize,
                                                 nullptr,
                                                 &TextureViews_[ textureIndex + 1 ].raw,
                                                 textureMaxSize );

        if ( textureCreationResult != ERROR_SUCCESS )
        {
            ::MessageBoxW( windowHandle, L"Couldn't create texture", L"Package Load Error", MB_OK | MB_ICONERROR );
            return false;
        }
    }

    D3D11_RASTERIZER_DESC rasterizerDesc = {};

    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.CullMode              = D3D11_CULL_MODE::D3D11_CULL_BACK;
    rasterizerDesc.DepthBias             = 0;
    rasterizerDesc.DepthBiasClamp        = 0;
    rasterizerDesc.DepthClipEnable       = TRUE;
    rasterizerDesc.FillMode              = D3D11_FILL_MODE::D3D11_FILL_SOLID;
    rasterizerDesc.FrontCounterClockwise = TRUE;
    rasterizerDesc.MultisampleEnable     = TRUE;
    rasterizerDesc.ScissorEnable         = FALSE;
    rasterizerDesc.SlopeScaledDepthBias  = 0;

    HRESULT rasterizerResult = Device_->CreateRasterizerState( &rasterizerDesc, &RasterizerState_.raw );

    if ( rasterizerResult != ERROR_SUCCESS )
    {
        ::MessageBoxW( windowHandle, L"Couldn't create rasterizer state", L"Package Load Error", MB_OK | MB_ICONERROR );
        return false;
    }
    
    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};

    depthStencilDesc.DepthEnable                  = FALSE;
    depthStencilDesc.DepthWriteMask               = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.StencilEnable                = FALSE;
    depthStencilDesc.StencilReadMask              = 0;
    depthStencilDesc.StencilWriteMask             = 0;
    depthStencilDesc.DepthFunc                    = D3D11_COMPARISON_ALWAYS;
    depthStencilDesc.FrontFace.StencilFunc        =
    depthStencilDesc.BackFace.StencilFunc         = D3D11_COMPARISON_ALWAYS;
    depthStencilDesc.FrontFace.StencilDepthFailOp = 
    depthStencilDesc.BackFace.StencilDepthFailOp  = 
    depthStencilDesc.FrontFace.StencilPassOp      = 
    depthStencilDesc.BackFace.StencilPassOp       = 
    depthStencilDesc.FrontFace.StencilFailOp      = 
    depthStencilDesc.BackFace.StencilFailOp       = D3D11_STENCIL_OP_KEEP;

    HRESULT depthStateResult = Device_->CreateDepthStencilState( &depthStencilDesc, &DepthStencilState_.raw );

    if ( rasterizerResult != ERROR_SUCCESS )
    {
        ::MessageBoxW( windowHandle, L"Couldn't create depth stencil state", L"Package Load Error", MB_OK | MB_ICONERROR );
        return false;
    }

 //   D3D11_

    ProceduralTargets_ = new COMAutoPtr< ID3D11RenderTargetView >[ Package_->ProceduralTextureCount ];

    for ( uint32_t proceduralIndex = 0; proceduralIndex < Package_->ProceduralTextureCount; ++proceduralIndex )
    {
        const ProceduralTexture& procedural  = Package_->ProceduralTextures[ proceduralIndex ];
        D3D11_TEXTURE2D_DESC     textureDesc = {};

        textureDesc.Width     = procedural.Width;
        textureDesc.Height    = procedural.Height;
        textureDesc.MipLevels = procedural.GenerateMipMaps ? 0 : 1;
        textureDesc.ArraySize = 1;

        switch ( procedural.Format )
        {
        case ProceduralFormats::R32F:

            textureDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
            break;

        case ProceduralFormats::RGBA16F:

            textureDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT;
            break;

        case ProceduralFormats::RGBA32F:

            textureDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
            break;

        case ProceduralFormats::RGBA8_UNORM:

            textureDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
            break;

        case ProceduralFormats::RGBA8_UNORM_SRGB:

            textureDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            break;
        }

        textureDesc.BindFlags        = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET | D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.MiscFlags        = D3D11_RESOURCE_MISC_FLAG::D3D11_RESOURCE_MISC_GENERATE_MIPS;

        COMAutoPtr< ID3D11Texture2D > texture;

        HRESULT createTextureResult = device->CreateTexture2D( &textureDesc, nullptr, &texture.raw );

        if ( createTextureResult != ERROR_SUCCESS )
        {
            ::MessageBoxW( windowHandle, L"Couldn't create procedural texture", L"Package Load Error", MB_OK | MB_ICONERROR );
            return false;
        }

        HRESULT createTargetResult = device->CreateRenderTargetView( texture.raw, nullptr, &ProceduralTargets_[ proceduralIndex ].raw );

        if ( createTargetResult != ERROR_SUCCESS )
        {
            ::MessageBoxW( windowHandle, L"Couldn't create render target view", L"Package Load Error", MB_OK | MB_ICONERROR );
            return false;
        }

        HRESULT createViewResult = device->CreateShaderResourceView( texture.raw, nullptr, &TextureViews_[ 1 + Package_->StaticTextureCount + proceduralIndex ].raw );

        if ( createViewResult != ERROR_SUCCESS )
        {
            ::MessageBoxW( windowHandle, L"Couldn't create shader resource view", L"Package Load Error", MB_OK | MB_ICONERROR );
            return false;
        }
    }

    PixelShaders_ = new COMAutoPtr< ID3D11PixelShader >[ Package_->ShaderCount ];

    for ( uint32_t shaderIndex = 0; shaderIndex < Package_->ShaderCount; ++shaderIndex )
    {
        HRESULT shaderResult =
            device->CreatePixelShader( Package_->Shaders[ shaderIndex ].Data,
                                       Package_->Shaders[ shaderIndex ].ResourceSize,
                                       nullptr,
                                       &PixelShaders_[ shaderIndex ].raw );

        if ( shaderResult != ERROR_SUCCESS )
        {
            ::MessageBoxW( windowHandle, L"Couldn't create pixel shader", L"Package Load Error", MB_OK | MB_ICONERROR );
            return false;
        }
    }

    HRESULT vertexShaderResult =
        device->CreateVertexShader( Package_->ScreenAlignedQuadVS.Data,
                                    Package_->ScreenAlignedQuadVS.ResourceSize,
                                    nullptr,
                                    &ScreenAlignedQuadVS_.raw );

    if ( vertexShaderResult != ERROR_SUCCESS )
    {
        ::MessageBoxW( windowHandle, L"Couldn't create vertex shader", L"Package Load Error", MB_OK | MB_ICONERROR );
        return false;
    }

    Samplers_ = new COMAutoPtr< ID3D11SamplerState >[ Package_->SamplerCount ];

    for ( uint32_t samplerIndex = 0; samplerIndex < Package_->SamplerCount; ++samplerIndex )
    {
        const Sampler&     sampler     = Package_->Samplers[ samplerIndex ];
        D3D11_SAMPLER_DESC samplerDesc = {};

        samplerDesc.AddressU = ToAddressMode( sampler.AddressModes[ 0 ] );
        samplerDesc.AddressV = ToAddressMode( sampler.AddressModes[ 1 ] );
        samplerDesc.AddressW = ToAddressMode( sampler.AddressModes[ 2 ] );
        samplerDesc.MinLOD   = -FLT_MAX;
        samplerDesc.MaxLOD   = FLT_MAX;

        switch ( sampler.Filter )
        {
        case TextureFilterMode::BILINEAR:

            samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            break;

        case TextureFilterMode::NEAREST:

            samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_POINT;
            break;

        case TextureFilterMode::ANISOTROPIC:

            samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_ANISOTROPIC;
            samplerDesc.MaxAnisotropy = sampler.MaxAnisotropy;
            break;

        case TextureFilterMode::TRILINEAR:
        default:

            samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
            break;

        }

        HRESULT createSamplerResult =
            device->CreateSamplerState( &samplerDesc, &Samplers_[ samplerIndex ].raw );

        if ( createSamplerResult != ERROR_SUCCESS )
        {
            ::MessageBoxW( windowHandle, L"Couldn't create sampler", L"Package Load Error", MB_OK | MB_ICONERROR );
            return false;
        }
    }

    return true;
}
