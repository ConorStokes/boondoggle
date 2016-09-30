#include "binary_effects_format.h"


bool ValidatePackage( const BoondogglePackageHeader& package, const uint8_t* endOfPackage )
{
    if ( package.MagicCode != MagicCodes::HEADER_CODE ||
         !package.Shaders.IsValidNotNull( endOfPackage, package.ShaderCount ) ||
         !package.StaticTextures.IsValidNotNull( endOfPackage, package.StaticTextureCount ) ||
         !package.ProceduralTextures.IsValidNotNull( endOfPackage, package.ProceduralTextureCount ) ||
         !package.Samplers.IsValidNotNull( endOfPackage, package.SamplerCount ) ||
         !package.Effects.IsValidNotNull( endOfPackage, package.EffectCount ) ||
         !package.ScreenAlignedQuadVS.Data.IsValidNotNull( endOfPackage, package.ScreenAlignedQuadVS.ResourceSize ) &&
         package.EffectCount > 0 )
    {
        return false;
    }

    uint32_t totalTextures = 1 + package.StaticTextureCount + package.ProceduralTextureCount;

    for ( uint32_t shaderIndex = 0; shaderIndex < package.ShaderCount; ++shaderIndex )
    {
        const ResourceBlob& shader = package.Shaders[ shaderIndex ];

        if ( !shader.Data.IsValidNotNull( endOfPackage, shader.ResourceSize ) )
        {
            return false;
        }
    }

    for ( uint32_t staticTextureIndex = 0; staticTextureIndex < package.StaticTextureCount; ++staticTextureIndex )
    {
        const ResourceBlob& staticTexture = package.StaticTextures[ staticTextureIndex ];

        if ( !staticTexture.Data.IsValidNotNull( endOfPackage, staticTexture.ResourceSize ) )
        {
            return false;
        }
    }

    for ( uint32_t proceduralIndex = 0; proceduralIndex < package.ProceduralTextureCount; ++proceduralIndex )
    {
        const ProceduralTexture& procedural = package.ProceduralTextures[ proceduralIndex ];

        if ( procedural.ShaderId >= package.ShaderCount ||
             !procedural.SourceTextures.IsValidNotNull( endOfPackage, procedural.SourceTextureCount ) ||
             !procedural.SourceSamplers.IsValidNotNull( endOfPackage, procedural.SourceSamplerCount ) )
        {
            return false;
        }

        for ( uint32_t sourceTextureIndex = 0; sourceTextureIndex < procedural.SourceTextureCount; ++sourceTextureIndex )
        {
            if ( procedural.SourceTextures[ sourceTextureIndex ] >= totalTextures )
            {
                return false;
            }
        }

        for ( uint32_t sourceSamplerIndex = 0; sourceSamplerIndex < procedural.SourceSamplerCount; ++sourceSamplerIndex )
        {
            if ( procedural.SourceSamplers[ sourceSamplerIndex ] >= package.SamplerCount )
            {
                return false;
            }
        }
    }

    for ( uint32_t effectIndex = 0; effectIndex < package.EffectCount; ++effectIndex )
    {
        const VisualEffect& effect = package.Effects[ effectIndex ];

        if ( effect.ShaderId >= package.ShaderCount ||
             !effect.SourceTextures.IsValidNotNull( endOfPackage, effect.SourceTextureCount ) ||
             !effect.SourceSamplers.IsValidNotNull( endOfPackage, effect.SourceSamplerCount ) ||
             !effect.ProceduralTextures.IsValidNotNull( endOfPackage, effect.ProceduralTextureCount ) )
        {
            return false;
        }

        for ( uint32_t sourceTextureIndex = 0; sourceTextureIndex < effect.SourceTextureCount; ++sourceTextureIndex )
        {
            if ( effect.SourceTextures[ sourceTextureIndex ] >= totalTextures )
            {
                return false;
            }
        }

        for ( uint32_t sourceSamplerIndex = 0; sourceSamplerIndex < effect.SourceSamplerCount; ++sourceSamplerIndex )
        {
            if ( effect.SourceSamplers[ sourceSamplerIndex ] >= package.SamplerCount )
            {
                return false;
            }
        }

        for ( uint32_t sourceProceduralIndex = 0; sourceProceduralIndex < effect.ProceduralTextureCount; ++sourceProceduralIndex )
        {
            if ( effect.SourceSamplers[ sourceProceduralIndex ] >= package.ProceduralTextureCount )
            {
                return false;
            }
        }
    }

    return true;
}