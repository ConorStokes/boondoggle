#ifndef BOONDOGGLE_VISUAL_EFFECTS_H__
#define BOONDOGGLE_VISUAL_EFFECTS_H__

#pragma once

#include <stdint.h>
#include "../common/boondoggle_helpers.h"
#include <d3d11_1.h>
#include "shared_render_constants.h"

struct BoondogglePackageHeader;

struct PerFrameParameters
{
    ID3D11Buffer*             ConstantBuffer;
    uint8_t*                  BufferMemory; // memory to copy to, for use to set the constant buffer.
    size_t                    BufferMemorySize;
    uint32_t                  Effect;
    PerFrameConstants         Constants;
    ID3D11ShaderResourceView* SoundTextureSRV;
};

struct PerViewParameters
{
    uint32_t                Left;
    uint32_t                Top;
    uint32_t                Width;
    uint32_t                Height;
    ID3D11RenderTargetView* Target;
    PerViewConstants        Constants;
};

class BoondoggleEffectsPackage
{
public:

    BoondoggleEffectsPackage() :
        Package_( nullptr ),
        PackageSize_( 0 ),
        FileHandle_( nullptr ),
        ProceduralTargets_( nullptr ),
        TextureViews_( nullptr ),
        PixelShaders_( nullptr ),
        Samplers_( nullptr ),
        ScreenAlignedQuadVS_( nullptr ),
        Device_( nullptr ),
        Context_( nullptr )
    {
    }

    bool CreateResources( ID3D11Device* device, ID3D11DeviceContext* context, HWND windowHandle, size_t textureMaxSize, const wchar_t* packageName );

    ~BoondoggleEffectsPackage();

    bool RenderInitialTextures( const PerFrameParameters& frameParameters );

    bool Render( const PerFrameParameters& frameParameters, /* array */ const PerViewParameters* views, uint32_t viewCount );

    uint32_t EffectCount() const;

    BoondoggleEffectsPackage( const BoondoggleEffectsPackage& ) = delete;

    BoondoggleEffectsPackage& operator=( const BoondoggleEffectsPackage& ) = delete;

private:

    bool RenderProcedural( const PerFrameParameters& frameParameters, uint32_t proceduralIndex );

    const BoondogglePackageHeader*          Package_;

    size_t                                  PackageSize_;
    HANDLE                                  FileHandle_;
    COMAutoPtr< ID3D11RenderTargetView >*   ProceduralTargets_;
    COMAutoPtr< ID3D11ShaderResourceView >* TextureViews_;
    COMAutoPtr< ID3D11PixelShader        >* PixelShaders_;
    COMAutoPtr< ID3D11SamplerState >*       Samplers_;
    COMAutoPtr< ID3D11VertexShader >        ScreenAlignedQuadVS_;
    COMAutoPtr< ID3D11RasterizerState >     RasterizerState_;
    COMAutoPtr< ID3D11DepthStencilState >   DepthStencilState_;
    COMAutoPtr< ID3D11BlendState >          BlendState_;

    ID3D11Device*                           Device_;
    ID3D11DeviceContext*                    Context_;

};

#endif // --BOONDOGGLE_VISUAL_EFFECTS_H__
