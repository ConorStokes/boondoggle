#ifndef BOONDOGGLE_BINARY_EFFECTS_FORMAT_H__
#define BOONDOGGLE_BINARY_EFFECTS_FORMAT_H__

#include <stdint.h>

#pragma once

#if defined( _MSC_VER )
#define BEF_FORCE_INLINE __forceinline
#else
#define BEF_FORCE_INLINE inline
#endif

// Relative addressing used for contiguous memory regions, ala Per Vognsen's GOB.
template < typename PointedType, typename AddressType = int32_t >
struct Relative
{
    AddressType RelativeAddress;

    // Default constructor initializes as a null pointer.
    BEF_FORCE_INLINE Relative() : RelativeAddress( 0 ) {}

    BEF_FORCE_INLINE Relative( const PointedType* actualPointer )
        : RelativeAddress( actualPointer == nullptr ? 
                           0 : 
                           static_cast< AddressType >( reinterpret_cast< const uint8_t* >( actualPointer ) - reinterpret_cast< const uint8_t* >( &RelativeAddress ) ) ) {}

    /*
     * We remove the copy constructor and assignment operator here, which we could make work
     * using a 64bit relative address.
     */

    Relative( const Relative< PointedType >& from ) = delete;

    Relative& operator=( const Relative< PointedType >& from ) = delete;

    BEF_FORCE_INLINE Relative< PointedType >& operator=( const PointedType* actualPointer )
    {
        RelativeAddress = 
            actualPointer == nullptr ? 
            0 : 
            static_cast< AddressType >( reinterpret_cast< const uint8_t* >( actualPointer ) - reinterpret_cast< const uint8_t* >( &RelativeAddress ) );
    
        return *this;
    }

    BEF_FORCE_INLINE PointedType* Raw()
    {
        return RelativeAddress == 0 ?
            nullptr :
            reinterpret_cast<PointedType*>( reinterpret_cast< uint8_t*>( &RelativeAddress ) + RelativeAddress );
    }

    BEF_FORCE_INLINE const PointedType* Raw() const
    {
        return RelativeAddress == 0 ?
            nullptr :
            reinterpret_cast<const PointedType*>( reinterpret_cast<const uint8_t*>( &RelativeAddress ) + RelativeAddress );
    }

    BEF_FORCE_INLINE operator const PointedType*( ) const { return Raw(); }

    BEF_FORCE_INLINE operator PointedType*( ) { return Raw(); };

    BEF_FORCE_INLINE PointedType* operator->() { return Raw(); }

    BEF_FORCE_INLINE const PointedType* operator->() const { return Raw(); }

    BEF_FORCE_INLINE PointedType& operator*() { return *Raw(); }

    BEF_FORCE_INLINE const PointedType& operator*() const { return *Raw(); }

    BEF_FORCE_INLINE PointedType& operator[]( size_t index ) { return *( Raw() + index ); }

    BEF_FORCE_INLINE const PointedType& operator[]( size_t index ) const { return *( Raw() + index ); }

    BEF_FORCE_INLINE bool IsValid( const uint8_t* endBuffer, uint32_t count = 1 ) const
    {
        return RelativeAddress == 0 || reinterpret_cast<const uint8_t*>( Raw() + count ) <= endBuffer;
    }

    BEF_FORCE_INLINE bool IsValidNotNull( const uint8_t* endBuffer, uint32_t count = 1 ) const
    {
        return RelativeAddress != 0 && reinterpret_cast<const uint8_t*>( Raw() + count ) <= endBuffer;
    }

    BEF_FORCE_INLINE bool IsNull() const { return RelativeAddress == 0; }
};


enum class TextureFilterMode : uint8_t
{
    NEAREST     = 0, // nearest for min, mag and mip
    BILINEAR    = 1, // linear for min/mag, nearest for mip.
    TRILINEAR   = 2,  // linear for min, mag and mip
    ANISOTROPIC = 3
};

enum class TextureAddressMode : uint8_t
{
    WRAP        = 1,
    MIRROR      = 2,
    CLAMP       = 3,
    MIRROR_ONCE = 4
};

enum class MagicCodes : uint32_t
{
    HEADER_CODE = 0xEA7B0075
};

enum class CodeVersions : uint32_t
{
    VERSION_1_0 = 0x00010000
};

enum class ProceduralFormats : uint32_t
{
    RGBA8_UNORM      = 0,
    RGBA8_UNORM_SRGB = 1,
    RGBA16F          = 2,
    R32F             = 3,
    RGBA32F          = 4
};

struct ResourceBlob
{
    uint32_t                           ResourceSize;
    Relative< uint8_t >                Data;
};

struct ProceduralTexture
{
    uint32_t                           ShaderId;
    ProceduralFormats                  Format;
    uint32_t                           Width;
    uint32_t                           Height;
//    uint32_t                           Depth;
    uint32_t                           SourceTextureCount;
    Relative< uint32_t >               SourceTextures;
    uint32_t                           SourceSamplerCount;
    Relative< uint32_t >               SourceSamplers;
    bool                               GenerateMipMaps;
    bool                               GenerateAtStart;
};

struct Sampler
{
    TextureAddressMode                 AddressModes[ 3 ];
    TextureFilterMode                  Filter;
    uint8_t                            MaxAnisotropy;
};

struct VisualEffect
{
    uint32_t                           ShaderId;
    uint32_t                           SourceTextureCount;
    Relative< uint32_t >               SourceTextures;
    uint32_t                           SourceSamplerCount;
    Relative< uint32_t >               SourceSamplers;
    uint32_t                           ProceduralTextureCount;
    Relative< uint32_t >               ProceduralTextures;
    float                              TransitionInTime;
    float                              TransitionOutTime;
    bool                               UseSoundTexture;
};

struct BoondogglePackageHeader
{
    MagicCodes                         MagicCode;              // Magic code for the file format.
    CodeVersions                       Version; 

    uint32_t                           ShaderCount;            // The number of pixel shader resources
    Relative< ResourceBlob >           Shaders;                // Compiled pixel shader blobs.

    uint32_t                           StaticTextureCount;     // The number of static texture resources
    Relative< ResourceBlob >           StaticTextures;         // Static textures resources - blobs as DDS files.

    uint32_t                           ProceduralTextureCount;
    Relative< ProceduralTexture >      ProceduralTextures;

    uint32_t                           SamplerCount;
    Relative< Sampler >                Samplers;

    uint32_t                           EffectCount;
    Relative< VisualEffect >           Effects;

    ResourceBlob                       ScreenAlignedQuadVS;
};

bool ValidatePackage( const BoondogglePackageHeader& package, const uint8_t* endOfPackage );

#endif // -- BOONDOGGLE_BINARY_EFFECTS_FORMAT_H__