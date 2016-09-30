#include <windows.h>
#include <stdio.h>
#include "../external/json/json.h"
#include <stdlib.h>
#include <unordered_map>
#include <d3dcompiler.h>
#include "../common/binary_effects_format.h"
#include "../common/boondoggle_helpers.h"
#include <memory.h>

namespace
{
    struct ConvertedWideString
    {
        wchar_t* Value;

        // Convert a UTF-8 string to windows unicode.
        ConvertedWideString( const char* input )
            : Value( nullptr )
        {
            int widePathBufferSize = MultiByteToWideChar( CP_UTF8, 0, input, -1, NULL, 0 );

            Value = reinterpret_cast<wchar_t*>( malloc( sizeof( WCHAR ) * widePathBufferSize ) );

            if ( MultiByteToWideChar( CP_UTF8, 0, input, -1, Value, widePathBufferSize ) <= 0 )
            {
                free( Value );
                Value = nullptr;
            }
        }

        ~ConvertedWideString()
        {
            if ( Value != nullptr )
            {
                ::free( Value );
                Value = nullptr;
            }
        }

        ConvertedWideString( const ConvertedWideString& ) = delete;

        ConvertedWideString& operator=( const ConvertedWideString& ) = delete;
    };

    // FNV 64bit style hash, cut down to 32 bits if we are running in 32bit mode - nice and simple
    // but slow on 32-bit. But where possible, we should be running this in 64bit anyway.
    struct StringHash
    {
        inline size_t operator()( const char* input ) const
        {
            uint64_t hash = 14695981039346656037ULL;

            for ( ; *input != 0; ++input )
            {
                hash ^= uint64_t( *input );
                hash *= 1099511628211ULL;
            }

            return static_cast<size_t>( hash );
        }
    };

    struct EqualsString
    {
        inline bool operator()( const char* left, const char* right ) const
        {
            return ::strcmp( left, right ) == 0;
        }
    };

    struct MemoryMappedReadFile
    {
        HANDLE       FileHandle;
        const void*  Data;
        size_t       Size;

        MemoryMappedReadFile() : FileHandle( nullptr ), Data( nullptr ), Size( 0 ) {}

        MemoryMappedReadFile( const MemoryMappedReadFile& ) = delete;

        MemoryMappedReadFile& operator=( const MemoryMappedReadFile& ) = delete;

        // Open using a UTF8 path (convert to wide chars)
        bool Open( const char* path )
        {
            ConvertedWideString widePath( path );

            return Open( widePath.Value );
        }

        bool Open( const wchar_t* path )
        {
            FileHandle = ::CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr );

            if ( FileHandle == INVALID_HANDLE_VALUE || FileHandle == nullptr )
            {
                return false;
            }

            LARGE_INTEGER fileSize;
            BOOL          fileSizeResult = ::GetFileSizeEx( FileHandle, &fileSize );

            if ( !fileSizeResult )
            {
                return false;
            }

            Size = static_cast<size_t>( fileSize.QuadPart );

            HANDLE fileMappingHandle = ::CreateFileMappingW( FileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr );

            if ( fileMappingHandle == nullptr )
            {
                return false;
            }

            Data = ::MapViewOfFile( fileMappingHandle, FILE_MAP_READ, 0, 0, Size );

            return Data != nullptr;
        }
    };

    struct FreeJsonValue
    {
        json_value_s* Value;

        FreeJsonValue() : Value( nullptr ) {}

        ~FreeJsonValue()
        {
            if ( Value != nullptr )
            {
                ::free( Value );
                Value = nullptr;
            }
        }
    };

    TextureAddressMode ParseAddressMode( const char* addressModeString )
    {
        TextureAddressMode result = TextureAddressMode::WRAP;

        if ( addressModeString != nullptr )
        {
            if ( ::_stricmp( addressModeString, "mirror" ) == 0 )
            {
                result = TextureAddressMode::MIRROR;
            }
            else if ( ::_stricmp( addressModeString, "clamp" ) == 0 )
            {
                result = TextureAddressMode::CLAMP;
            }
            else if ( ::_stricmp( addressModeString, "mirror_once" ) == 0 )
            {
                result = TextureAddressMode::MIRROR_ONCE;
            }
        }

        return result;
    }

    TextureFilterMode ParseFilterMode( const char* filterModeString )
    {
        TextureFilterMode result = TextureFilterMode::TRILINEAR;

        if ( filterModeString != nullptr )
        {
            if ( ::_stricmp( filterModeString, "nearest" ) == 0 )
            {
                result = TextureFilterMode::NEAREST;
            }
            else if ( ::_stricmp( filterModeString, "bilinear" ) == 0 )
            {
                result = TextureFilterMode::BILINEAR;
            }
            else if ( ::_stricmp( filterModeString, "anisotropic" ) == 0 )
            {
                result = TextureFilterMode::ANISOTROPIC;
            }
        }

        return result;
    }


    const char* GetString( const json_object_s* object, const char* name, const char* defaultValue = nullptr )
    {
        for ( const json_object_element_s* element = object->start; element != nullptr; element = element->next )
        {
            if ( element->value->type == json_type_e::json_type_string &&
                 ::_stricmp( element->name->string, name ) == 0 )
            {
                const json_string_s* stringValue = reinterpret_cast<const json_string_s*>( element->value->payload );

                return stringValue->string;
            }
        }

        return defaultValue;
    }

    bool GetBool( const json_object_s* object, const char* name, bool defaultValue )
    {
        for ( const json_object_element_s* element = object->start; element != nullptr; element = element->next )
        {
            if ( ( element->value->type == json_type_e::json_type_true ||
                   element->value->type == json_type_e::json_type_false ) &&
                 ::_stricmp( element->name->string, name ) == 0 )
            {
                return element->value->type == json_type_e::json_type_true;
            }
        }

        return defaultValue;
    }

    bool TryGetNumber( const json_object_s* object, const char* name, double* numberResult )
    {
        for ( const json_object_element_s* element = object->start; element != nullptr; element = element->next )
        {
            if ( element->value->type == json_type_e::json_type_number &&
                 ::_stricmp( element->name->string, name ) == 0 )
            {
                const json_number_s* numberValue = reinterpret_cast<const json_number_s*>( element->value->payload );

                *numberResult = ::strtod( numberValue->number, nullptr );
                return true;
            }
        }

        return false;
    }

    const json_object_s* GetChildObject( const json_object_s* object, const char* name )
    {
        for ( const json_object_element_s* element = object->start; element != nullptr; element = element->next )
        {
            if ( element->value->type == json_type_e::json_type_object &&
                 ::_stricmp( element->name->string, name ) == 0 )
            {
                return reinterpret_cast<const json_object_s*>( element->value->payload );
            }
        }

        return nullptr;
    }

    const json_array_s* GetChildArray( const json_object_s* object, const char* name )
    {
        for ( const json_object_element_s* element = object->start; element != nullptr; element = element->next )
        {
            if ( element->value->type == json_type_e::json_type_array &&
                 ::_stricmp( element->name->string, name ) == 0 )
            {
                return reinterpret_cast<const json_array_s*>( element->value->payload );
            }
        }

        return nullptr;
    }

    struct OutputAllocator
    {
        static const size_t AllocationSize  = 1024 * 1024 * 1024;
        static const size_t CommitBlockSize = 4 * 1024 * 1024;

        void*  Allocation;
        size_t HighWatermark;
        size_t Committed;

        OutputAllocator()
            : Allocation( nullptr ),
              HighWatermark( 0 ),
              Committed( 0 )
        {
        }

        bool Initialize()
        {
            Allocation = ::VirtualAlloc( nullptr, AllocationSize, MEM_RESERVE, PAGE_READWRITE );

            if ( Allocation == nullptr )
            {
                printf( "Couldn't reserve memory\n" );
                return false;
            }

            if ( ::VirtualAlloc( Allocation, CommitBlockSize, MEM_COMMIT, PAGE_READWRITE ) == nullptr )
            {
                printf( "Couldn't commit memory\n" );
                return false;
            }

            Committed     = CommitBlockSize;
            HighWatermark = 0;

            return true;
        }

        ~OutputAllocator()
        {
            if ( ::VirtualFree( Allocation, 0, MEM_RELEASE ) == FALSE )
            {
                printf( "Error freeing pool allocation.\n" );
            }
        }

        void* Allocate( size_t size, size_t alignment )
        {
            size_t paddedBegin  = ( HighWatermark + alignment - 1 ) & ~( alignment - 1 );
            size_t newWaterMark = paddedBegin + size;
            
            if ( newWaterMark > AllocationSize )
            {
                printf( "Couldn't allocate memory, pool exhausted.\n" );
                exit( EXIT_FAILURE ); // this isn't a great thing to-do, but we don't have any resources in destructors that won't be cleaned up by the OS            
            }

            if ( newWaterMark > Committed )
            {
                size_t newCommitted = ( Committed + ( newWaterMark - Committed ) + CommitBlockSize - 1 ) & ~( CommitBlockSize - 1 );
                size_t extraCommit  = newCommitted - newCommitted;

                void* committedAllocation = 
                    ::VirtualAlloc( static_cast<uint8_t*>( Allocation ) + Committed, extraCommit, MEM_COMMIT, PAGE_READWRITE );

                if ( committedAllocation == nullptr )
                {
                    printf( "Couldn't commit memory.\n" );
                    exit( EXIT_FAILURE ); // this isn't a great thing to-do, but we don't have any resources in destructors that won't be cleaned up by the OS
                }

                Committed = newCommitted;
            }

            HighWatermark = newWaterMark;

            return reinterpret_cast<uint8_t*>( Allocation ) + paddedBegin;
        }

        template < typename AllocationType >
        AllocationType* Allocate( size_t count = 1 )
        {
            AllocationType* result =
                reinterpret_cast<AllocationType*>(
                    Allocate( sizeof( AllocationType ) * count,
                              alignof( AllocationType ) ) );

            if ( result != nullptr )
            {
                for ( AllocationType* where = result, *end = result + count; where < end; ++where )
                {
                    new ( where ) AllocationType();
                }
            }

            return result;
        }

        void Reset()
        {
            HighWatermark = 0;
        }
    };
}

typedef std::unordered_map< const char*, uint32_t, StringHash, EqualsString > StringIdMap;

int wmain( int argc, const wchar_t** argv )
{
    MemoryMappedReadFile mainFile;

    if ( argc < 3 )
    {
        printf( "Usage: \n" );
        printf( "    boondoggle_compiler.exe <input_file> <output_file>\n" );
        return EXIT_FAILURE;
    }

    bool mainFileResult = mainFile.Open( argv[ 1 ] );

    if ( !mainFileResult )
    {
        printf( "Couldn't open package description file\n" );
        return EXIT_FAILURE;
    }

    json_parse_result_s parseResult = {};
    FreeJsonValue       parsedValue;
    
    parsedValue.Value = ::json_parse_ex( mainFile.Data, mainFile.Size, json_parse_flags_allow_simplified_json, nullptr, nullptr, &parseResult );

    if ( parsedValue.Value == nullptr || parseResult.error != json_parse_error_e::json_parse_error_none )
    {
        const char* errorString = "unknown";

        switch ( parseResult.error )
        {
        case json_parse_error_e::json_parse_error_expected_comma:

            errorString = "expected comma";
            break;

        case json_parse_error_e::json_parse_error_expected_colon:

            errorString = "expected colon";
            break;

        case json_parse_error_e::json_parse_error_expected_opening_quote:

            errorString = "expected opening quote";
            break;

        case json_parse_error_e::json_parse_error_invalid_string_escape_sequence:

            errorString = "invalid string escape sequence";
            break;

        case json_parse_error_e::json_parse_error_invalid_number_format:

            errorString = "invalid number format";
            break;

        case json_parse_error_e::json_parse_error_invalid_value:

            errorString = "invalid value";
            break;

        case json_parse_error_e::json_parse_error_premature_end_of_buffer:

            errorString = "unexpected end of file";
            break;

        case json_parse_error_e::json_parse_error_invalid_string:

            errorString = "invalid string";
            break;

        case json_parse_error_e::json_parse_error_allocator_failed:

            errorString = "allocation failed";
            break;
        }

        printf( "Parsing error \"%s\" Line: %d Column: %d \n", errorString, static_cast< int >( parseResult.error_line_no ), static_cast< int >( parseResult.error_row_no ) );

        return EXIT_FAILURE;
    }

    if ( parsedValue.Value->type != json_type_e::json_type_object )
    {
        printf( "Expected JSON object type as root value in parse\n" );
        return EXIT_FAILURE;
    }

    const json_object_s* rootObject              = reinterpret_cast<const json_object_s*>( parsedValue.Value->payload );
    const json_array_s*  shadersArray            = GetChildArray( rootObject, "shaders" );
    const json_array_s*  samplersArray           = GetChildArray( rootObject, "samplers" );
    const json_array_s*  staticTexturesArray     = GetChildArray( rootObject, "static_textures" );
    const json_array_s*  proceduralTexturesArray = GetChildArray( rootObject, "procedural_textures" );
    const json_array_s*  effectsArray            = GetChildArray( rootObject, "effects" );
    const json_object_s* vertexQuadShaderObject  = GetChildObject( rootObject, "vertex_quad_shader" );

    if ( shadersArray == nullptr || shadersArray->length == 0 )
    {
        printf( "Shaders element is not an array, at least one shader needed to define effects\n" );
        return EXIT_FAILURE;
    }

    OutputAllocator fileSpace;

    if ( !fileSpace.Initialize() )
    {
        printf( "Couldn't initialize allocator for file output.\n" );
        return EXIT_FAILURE;
    }

    BoondogglePackageHeader* header = fileSpace.Allocate< BoondogglePackageHeader >();

    header->MagicCode   = MagicCodes::HEADER_CODE;
    header->Version     = CodeVersions::VERSION_1_0;
    header->ShaderCount = static_cast< uint32_t >( shadersArray->length );
    header->Shaders     = fileSpace.Allocate< ResourceBlob >( shadersArray->length );

    StringIdMap shaderIdMap;

    uint32_t shaderIndex = 0;

    for ( const json_array_element_s* shaderEntry = shadersArray->start; 
          shaderEntry != nullptr; 
          shaderEntry = shaderEntry->next,
          ++shaderIndex )
    {
        if ( shaderEntry->value->type != json_type_e::json_type_object )
        {
            printf( "Shader entry is not an object\n" );
            return EXIT_FAILURE;
        }

        const json_object_s* shaderObject   = reinterpret_cast<const json_object_s*>( shaderEntry->value->payload );
        const char*          id             = GetString( shaderObject, "id" );
        const char*          shaderFilePath = GetString( shaderObject, "file" );
        const char*          entryPoint     = GetString( shaderObject, "entry_point", "main" );

        if ( id == nullptr || shaderFilePath == nullptr )
        {
            printf( "Bad shader definition\n" );
            return EXIT_FAILURE;
        }

        const json_array_s* definesArray = GetChildArray( shaderObject, "defines" );
        std::vector< D3D_SHADER_MACRO > defines;

        if ( definesArray != nullptr )
        {
            defines.reserve( definesArray->length + 1 );

            for ( const json_array_element_s* defineEntry = definesArray->start;
                  defineEntry != nullptr;
                  defineEntry = defineEntry->next )
            {
                if ( defineEntry->value->type != json_type_e::json_type_object )
                {
                    printf( "Shader define entry is not an object for shader %s\n", id );
                    return EXIT_FAILURE;
                }

                const json_object_s* defineObject = reinterpret_cast<const json_object_s*>( defineEntry->value->payload );

                const char* defineName       = GetString( defineObject, "name" );
                const char* defineDefinition = GetString( defineObject, "definition", "" );

                if ( defineName == nullptr )
                {
                    printf( "Shader define has no name for shader %s\n", id );
                    return EXIT_FAILURE;
                }

                D3D_SHADER_MACRO macro = { defineName, defineDefinition };
                
                defines.push_back( macro );
            }
        }

        D3D_SHADER_MACRO nullTerminator = { nullptr, nullptr };

        defines.push_back( nullTerminator );

        ConvertedWideString convertedFilePath( shaderFilePath );

        COMAutoPtr< ID3DBlob > shaderBlob;
        COMAutoPtr< ID3DBlob > errorBlob;

        HRESULT compileResult = 
            ::D3DCompileFromFile( 
                convertedFilePath.Value, 
                &defines[ 0 ], 
                D3D_COMPILE_STANDARD_FILE_INCLUDE, 
                entryPoint, 
                "ps_5_0", 
                0/*D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION*/,
                0, 
                &shaderBlob.raw, 
                &errorBlob.raw );

        if ( compileResult != ERROR_SUCCESS )
        {
            printf( "Pixel shader %s (%s) had compilation error(s)\n", id, shaderFilePath );
            printf( "%s\n", reinterpret_cast< const char* >( errorBlob->GetBufferPointer() ) );

            return EXIT_FAILURE;
        }

        ResourceBlob& storedShaderBlob = header->Shaders[ shaderIndex ];

        storedShaderBlob.ResourceSize = static_cast< uint32_t >( shaderBlob->GetBufferSize() );
        storedShaderBlob.Data         = reinterpret_cast< uint8_t* >( fileSpace.Allocate( shaderBlob->GetBufferSize(), 1 ) );

        ::memcpy( storedShaderBlob.Data.Raw(), shaderBlob->GetBufferPointer(), storedShaderBlob.ResourceSize );

        shaderIdMap[ id ] = shaderIndex;
    }

    StringIdMap samplerIdMap;

    if ( samplersArray != nullptr )
    {
        header->SamplerCount = static_cast< uint32_t >( samplersArray->length );
        header->Samplers     = fileSpace.Allocate< Sampler >( samplersArray->length );

        uint32_t samplerIndex = 0;

        for ( const json_array_element_s* samplerEntry = samplersArray->start;
              samplerEntry != nullptr;
              samplerEntry = samplerEntry->next )
        {
            if ( samplerEntry->value->type != json_type_e::json_type_object )
            {
                printf( "Sampler is not an object\n" );
                return EXIT_FAILURE;
            }

            const json_object_s* samplerObject = reinterpret_cast<const json_object_s*>( samplerEntry->value->payload );
            Sampler&             sampler       = header->Samplers[ samplerIndex ];
            const char*          id            = GetString( samplerObject, "id" );

            if ( id == nullptr )
            {
                printf( "Sampler does not have id field\n" );
                return EXIT_FAILURE;
            }

            // parse address modes and filter - note, will use default 
            sampler.Filter            = ParseFilterMode( GetString( samplerObject, "filter" ) );
            sampler.AddressModes[ 0 ] = ParseAddressMode( GetString( samplerObject, "address_u" ) );
            sampler.AddressModes[ 1 ] = ParseAddressMode( GetString( samplerObject, "address_v" ) );
            sampler.AddressModes[ 2 ] = ParseAddressMode( GetString( samplerObject, "address_w" ) );
            
            double maxAnisotropy = 0.0;

            if ( TryGetNumber( samplerObject, "max_anisotropy", &maxAnisotropy ) )
            {
                sampler.MaxAnisotropy = static_cast<uint8_t>( maxAnisotropy );
            }

            samplerIdMap[ id ] = samplerIndex;
            ++samplerIndex;
        }
    }
    else
    {
        header->SamplerCount = 0;
        header->Samplers     = fileSpace.Allocate< Sampler >( 0 );
    }

    StringIdMap textureIdMap;

    uint32_t textureIndex = 0;

    textureIdMap[ "sound" ] = textureIndex;

    ++textureIndex;
    
    if ( staticTexturesArray != nullptr )
    {
        header->StaticTextureCount = static_cast< uint32_t >( staticTexturesArray->length );
        header->StaticTextures     = fileSpace.Allocate< ResourceBlob >( staticTexturesArray->length );

        uint32_t staticTexturesIndex = 0;

        for ( const json_array_element_s* staticTextureEntry = staticTexturesArray->start;
              staticTextureEntry != nullptr;
              staticTextureEntry = staticTextureEntry->next )
        {
            if ( staticTextureEntry->value->type != json_type_e::json_type_object )
            {
                printf( "Static texture is not an object\n" );
                return EXIT_FAILURE;
            }

            const json_object_s* staticTextureObject = reinterpret_cast<const json_object_s*>( staticTextureEntry->value->payload );
            ResourceBlob&        textureBlob         = header->StaticTextures[ staticTexturesIndex ];

            const char*          id                  = GetString( staticTextureObject, "id" );
            const char*          textureFilePath     = GetString( staticTextureObject, "file" );

            if ( id == nullptr || textureFilePath == nullptr )
            {
                printf( "Static texture has bad definition.\n" );
                return EXIT_FAILURE;
            }
            
            MemoryMappedReadFile textureFile;
                        
            if (  !textureFile.Open( textureFilePath ) )
            {
                printf( "Couldn't open texture %s\n", textureFilePath );
                return EXIT_FAILURE;
            }
            
            // parse address modes and filter - note, will use default 
            textureBlob.ResourceSize = static_cast< uint32_t >( textureFile.Size );
            textureBlob.Data         = reinterpret_cast< uint8_t* >( fileSpace.Allocate( textureFile.Size, 1 ) );

            ::memcpy( textureBlob.Data.Raw(), textureFile.Data, textureFile.Size );

            textureIdMap[ id ] = textureIndex;

            ++staticTexturesIndex;
            ++textureIndex;
        }
    }
    else
    {
        header->StaticTextureCount = 0;
        header->StaticTextures     = fileSpace.Allocate< ResourceBlob >( 0 );
    }

    StringIdMap proceduralTextureIdMap;

    if ( proceduralTexturesArray != nullptr )
    {
        header->ProceduralTextureCount = static_cast< uint32_t >( proceduralTexturesArray->length );
        header->ProceduralTextures     = fileSpace.Allocate< ProceduralTexture >( proceduralTexturesArray->length );

        uint32_t proceduralTextureIndex = 0;

        for ( const json_array_element_s* proceduralTextureEntry = proceduralTexturesArray->start;
              proceduralTextureEntry != nullptr;
              proceduralTextureEntry = proceduralTextureEntry->next )
        {
            if ( proceduralTextureEntry->value->type != json_type_e::json_type_object )
            {
                printf( "Procedural texture is not an object\n" );
                return EXIT_FAILURE;
            }

            const json_object_s* proceduralTextureObject = reinterpret_cast<const json_object_s*>( proceduralTextureEntry->value->payload );
            const char*          id                      = GetString( proceduralTextureObject, "id" );
            
            if ( id == nullptr )
            {
                printf( "Poorly formed procedural texture\n" );
                return EXIT_FAILURE;
            }

            textureIdMap[ id ]           = textureIndex;
            proceduralTextureIdMap[ id ] = proceduralTextureIndex;

            ++textureIndex;
            ++proceduralTextureIndex;
        }

        proceduralTextureIndex = 0;

        for ( const json_array_element_s* proceduralTextureEntry = proceduralTexturesArray->start;
              proceduralTextureEntry != nullptr;
              proceduralTextureEntry = proceduralTextureEntry->next )
        {
            if ( proceduralTextureEntry->value->type != json_type_e::json_type_object )
            {
                printf( "Procedural texture is not an object\n" );
                return EXIT_FAILURE;
            }

            const json_object_s* proceduralTextureObject = reinterpret_cast<const json_object_s*>( proceduralTextureEntry->value->payload );
            ProceduralTexture&   proceduralTexture       = header->ProceduralTextures[ proceduralTextureIndex ];
            const char*          shader                  = GetString( proceduralTextureObject, "shader" );
            double               width                   = 0;
            double               height                  = 0;
            const char*          id                      = GetString( proceduralTextureObject, "id" );

            if ( shader == nullptr ||
                 !TryGetNumber( proceduralTextureObject, "width", &width ) ||
                 !TryGetNumber( proceduralTextureObject, "height", &height ) )
            {
                printf( "Poorly formed procedural texture\n" );
                return EXIT_FAILURE;
            }

            proceduralTexture.Width           = static_cast< uint32_t >( width );
            proceduralTexture.Height          = static_cast< uint32_t >( height );
            proceduralTexture.GenerateAtStart = GetBool( proceduralTextureObject, "generate_at_start", false );
            proceduralTexture.GenerateMipMaps = GetBool( proceduralTextureObject, "generate_mips", true );

            StringIdMap::const_iterator shaderIndex = shaderIdMap.find( shader );

            if ( shaderIndex == shaderIdMap.end() )
            {
                printf( "Couldn't find shader %s for procedural texture %s\n", shader, id );
                return EXIT_FAILURE;
            }

            proceduralTexture.ShaderId = shaderIndex->second;

            const json_array_s* sourceSamplersArray = GetChildArray( proceduralTextureObject, "samplers" );

            if ( sourceSamplersArray != nullptr )
            {
                proceduralTexture.SourceSamplerCount = static_cast< uint32_t >( sourceSamplersArray->length );
                proceduralTexture.SourceSamplers     = fileSpace.Allocate< uint32_t >( sourceSamplersArray->length );

                uint32_t sourceSamplerIndex = 0;

                for ( const json_array_element_s* samplerEntry = sourceSamplersArray->start;
                      samplerEntry != nullptr;
                      samplerEntry = samplerEntry->next )
                {
                    if ( samplerEntry->value->type != json_type_e::json_type_string )
                    {
                        printf( "Procedural texture %s had sampler reference that wasn't a string\n", id );
                        return EXIT_FAILURE;
                    }

                    const json_string_s* samplerId = reinterpret_cast<const json_string_s*>( samplerEntry->value->payload );

                    StringIdMap::const_iterator samplerIndex = samplerIdMap.find( samplerId->string );

                    if ( samplerIndex == samplerIdMap.end() )
                    {
                        printf( "Couldn't find sampler %s for procedural texture %s\n", samplerId->string, id );
                        return EXIT_FAILURE;
                    }

                    proceduralTexture.SourceSamplers[ sourceSamplerIndex ] = samplerIndex->second;
                    ++sourceSamplerIndex;
                }
            }
            else
            {
                proceduralTexture.SourceSamplerCount = 0;
                proceduralTexture.SourceSamplers     = fileSpace.Allocate< uint32_t >( 0 );
            }

            const json_array_s* sourceTexturesArray = GetChildArray( proceduralTextureObject, "textures" );

            if ( sourceTexturesArray != nullptr )
            {
                proceduralTexture.SourceTextureCount = static_cast< uint32_t >( sourceTexturesArray->length );
                proceduralTexture.SourceTextures     = fileSpace.Allocate< uint32_t >( sourceTexturesArray->length );

                uint32_t sourceTextureIndex = 0;

                for ( const json_array_element_s* textureEntry = sourceTexturesArray->start;
                      textureEntry != nullptr;
                      textureEntry = textureEntry->next )
                {
                    if ( textureEntry->value->type != json_type_e::json_type_string )
                    {
                        printf( "Procedural texture %s had texture reference that wasn't a string\n", id );
                        return EXIT_FAILURE;
                    }

                    const json_string_s* textureId = reinterpret_cast<const json_string_s*>( textureEntry->value->payload );

                    StringIdMap::const_iterator textureIndex = textureIdMap.find( textureId->string );

                    if ( textureIndex == textureIdMap.end() )
                    {
                        printf( "Couldn't find texture %s for procedural texture %s\n", textureId->string, id );
                        return EXIT_FAILURE;
                    }

                    proceduralTexture.SourceTextures[ sourceTextureIndex ] = textureIndex->second;
                    ++sourceTextureIndex;
                }
            }
            else
            {
                proceduralTexture.SourceTextureCount = 0;
                proceduralTexture.SourceTextures     = fileSpace.Allocate< uint32_t >( 0 );
            }
            
            ++proceduralTextureIndex;
        }
    }
    else
    {
        header->ProceduralTextureCount = 0;
        header->ProceduralTextures     = fileSpace.Allocate< ProceduralTexture >( 0 );
    }

    if ( effectsArray == nullptr || effectsArray->length == 0 )
    {
        printf( "No effects defined.\n" );
        return EXIT_FAILURE;
    }

    header->EffectCount = static_cast< uint32_t >( effectsArray->length );
    header->Effects     = fileSpace.Allocate< VisualEffect >( effectsArray->length );

    uint32_t effectIndex = 0;

    for ( const json_array_element_s* effectEntry = effectsArray->start;
          effectEntry != nullptr;
          effectEntry = effectEntry->next )
    {
        if ( effectEntry->value->type != json_type_e::json_type_object )
        {
            printf( "Effect is not an object\n" );
            return EXIT_FAILURE;
        }

        const json_object_s* effectObject      = reinterpret_cast<const json_object_s*>( effectEntry->value->payload );
        VisualEffect&        effect            = header->Effects[ effectIndex ];
        const char*          shader            = GetString( effectObject, "shader" );
        const char*          id                = GetString( effectObject, "id", "<unnamed>" );
        double               transitionInTime  = 0.0;
        double               transitionOutTime = 0.0;

        if ( shader == nullptr )
        {
            printf( "Poorly formed procedural texture\n" );
            return EXIT_FAILURE;
        }

        if ( !TryGetNumber( effectObject, "transition_in_time", &transitionInTime ) )
        {
            transitionInTime = 0;
        }

        if ( !TryGetNumber( effectObject, "transition_out_time", &transitionOutTime ) )
        {
            transitionOutTime = 0;
        }

        effect.TransitionInTime  = static_cast< float >( transitionInTime );
        effect.TransitionOutTime = static_cast< float >( transitionOutTime );

        StringIdMap::const_iterator shaderIndex = shaderIdMap.find( shader );

        if ( shaderIndex == shaderIdMap.end() )
        {
            const char* id = GetString( effectObject, "id", "<unnamed>" );

            printf( "Couldn't find shader %s for effect %s\n", shader, id );
            return EXIT_FAILURE;
        }

        effect.ShaderId        = shaderIndex->second;
        effect.UseSoundTexture = false;

        const json_array_s* sourceSamplersArray = GetChildArray( effectObject, "samplers" );

        if ( sourceSamplersArray != nullptr )
        {
            effect.SourceSamplerCount = static_cast< uint32_t >( sourceSamplersArray->length );
            effect.SourceSamplers     = fileSpace.Allocate< uint32_t >( sourceSamplersArray->length );

            uint32_t sourceSamplerIndex = 0;

            for ( const json_array_element_s* samplerEntry = sourceSamplersArray->start;
                  samplerEntry != nullptr;
                  samplerEntry = samplerEntry->next )
            {
                if ( samplerEntry->value->type != json_type_e::json_type_string )
                {
                    printf( "Effect %s had sampler reference that wasn't a string\n", id );
                    return EXIT_FAILURE;
                }

                const json_string_s* samplerId = reinterpret_cast<const json_string_s*>( samplerEntry->value->payload );

                StringIdMap::const_iterator samplerIndex = samplerIdMap.find( samplerId->string );

                if ( samplerIndex == samplerIdMap.end() )
                {
                    printf( "Couldn't find sampler %s for effect %s\n", samplerId->string, id );
                    return EXIT_FAILURE;
                }

                effect.SourceSamplers[ sourceSamplerIndex ] = samplerIndex->second;
                ++sourceSamplerIndex;
            }
        }
        else
        {
            effect.SourceSamplerCount = 0;
            effect.SourceSamplers     = fileSpace.Allocate< uint32_t >( 0 );
        }

        const json_array_s* sourceTexturesArray = GetChildArray( effectObject, "textures" );

        if ( sourceTexturesArray != nullptr )
        {
            effect.SourceTextureCount = static_cast< uint32_t >( sourceTexturesArray->length );
            effect.SourceTextures     = fileSpace.Allocate< uint32_t >( sourceTexturesArray->length );

            uint32_t sourceTextureIndex = 0;

            for ( const json_array_element_s* textureEntry = sourceTexturesArray->start;
                  textureEntry != nullptr;
                  textureEntry = textureEntry->next )
            {
                if ( textureEntry->value->type != json_type_e::json_type_string )
                {
                    printf( "Effect %s had texture reference that wasn't a string\n", id );
                    return EXIT_FAILURE;
                }

                const json_string_s* textureId = reinterpret_cast<const json_string_s*>( textureEntry->value->payload );

                StringIdMap::const_iterator textureIndex = textureIdMap.find( textureId->string );

                if ( textureIndex == textureIdMap.end() )
                {
                    printf( "Couldn't find texture %s for effect %s\n", textureId->string, id );
                    return EXIT_FAILURE;
                }

                effect.SourceTextures[ sourceTextureIndex ] = textureIndex->second;
                ++sourceTextureIndex;

                if ( strcmp( textureId->string, "sound" ) == 0 )
                {
                    effect.UseSoundTexture = true;
                }
            }
        }
        else
        {
            effect.SourceTextureCount = 0;
            effect.SourceTextures     = fileSpace.Allocate< uint32_t >( 0 );
        }

        const json_array_s* proceduralsArray = GetChildArray( effectObject, "procedural_texture" );

        if ( proceduralsArray != nullptr )
        {
            effect.ProceduralTextureCount = static_cast< uint32_t >( proceduralsArray->length );
            effect.ProceduralTextures     = fileSpace.Allocate< uint32_t >( proceduralsArray->length );

            uint32_t proceduralTextureIndex = 0;

            for ( const json_array_element_s* proceduralTextureEntry = proceduralsArray->start;
                  proceduralTextureEntry != nullptr;
                  proceduralTextureEntry = proceduralTextureEntry->next )
            {
                if ( proceduralTextureEntry->value->type != json_type_e::json_type_string )
                {
                    printf( "Effect %s had procedural reference that wasn't a string\n", id );
                    return EXIT_FAILURE;
                }

                const json_string_s*        proceduralId    = reinterpret_cast<const json_string_s*>( proceduralTextureEntry->value->payload );
                StringIdMap::const_iterator proceduralIndex = textureIdMap.find( proceduralId->string );

                if ( proceduralIndex == proceduralTextureIdMap.end() )
                {
                    printf( "Couldn't find procedural texture %s for effect %s\n", proceduralId->string, id );
                    return EXIT_FAILURE;
                }

                effect.ProceduralTextures[ proceduralTextureIndex ] = proceduralIndex->second;
                ++proceduralTextureIndex;
            }
        }
        else
        {
            effect.ProceduralTextureCount = 0;
            effect.ProceduralTextures     = fileSpace.Allocate< uint32_t >( 0 );
        }

        ++effectIndex;
    }

    if ( vertexQuadShaderObject == nullptr )
    {
        printf( "Couldn't find vertex quad shader\n" );
        return EXIT_FAILURE;
    }

    {
        const char* shaderFilePath = GetString( vertexQuadShaderObject, "file" );
        const char* entryPoint     = GetString( vertexQuadShaderObject, "entry_point", "main" );

        if ( shaderFilePath == nullptr )
        {
            printf( "Bad shader definition\n" );
            return EXIT_FAILURE;
        }

        const json_array_s* definesArray = GetChildArray( vertexQuadShaderObject, "defines" );
        std::vector< D3D_SHADER_MACRO > defines;

        if ( definesArray != nullptr )
        {
            defines.reserve( definesArray->length + 1 );

            for ( const json_array_element_s* defineEntry = definesArray->start;
                  defineEntry != nullptr;
                  defineEntry = defineEntry->next )
            {
                if ( defineEntry->value->type != json_type_e::json_type_object )
                {
                    printf( "Vertex shader define entry is not an object for vertex quad shader\n" );
                    return EXIT_FAILURE;
                }

                const json_object_s* defineObject     = reinterpret_cast<const json_object_s*>( defineEntry->value->payload );
                const char*          defineName       = GetString( defineObject, "name" );
                const char*          defineDefinition = GetString( defineObject, "definition", "" );

                if ( defineName == nullptr )
                {
                    printf( "Vertex shader define has no name for vertex quad shader\n" );
                    return EXIT_FAILURE;
                }

                D3D_SHADER_MACRO macro = { defineName, defineDefinition };
                
                defines.push_back( macro );
            }
        }

        D3D_SHADER_MACRO nullTerminator = { nullptr, nullptr };

        defines.push_back( nullTerminator );

        ConvertedWideString convertedFilePath( shaderFilePath );

        COMAutoPtr< ID3DBlob > shaderBlob;
        COMAutoPtr< ID3DBlob > errorBlob;

        HRESULT compileResult = 
            ::D3DCompileFromFile( convertedFilePath.Value, 
                                  &defines[ 0 ], 
                                  D3D_COMPILE_STANDARD_FILE_INCLUDE, 
                                  entryPoint, 
                                  "vs_5_0", 
                                  /*D3DCOMPILE_DEBUG*/0, 
                                  0, 
                                  &shaderBlob.raw, 
                                  &errorBlob.raw );

        if ( compileResult != ERROR_SUCCESS )
        {
            printf( "Vertex Quad Shader (%s) had compilation error(s)\n", shaderFilePath );
            printf( "%s\n", reinterpret_cast< const char* >( errorBlob->GetBufferPointer() ) );

            return EXIT_FAILURE;
        }

        ResourceBlob& storedShaderBlob = header->ScreenAlignedQuadVS;

        storedShaderBlob.ResourceSize = static_cast< uint32_t >( shaderBlob->GetBufferSize() );
        storedShaderBlob.Data         = reinterpret_cast< uint8_t* >( fileSpace.Allocate( shaderBlob->GetBufferSize(), 1 ) );

        ::memcpy( storedShaderBlob.Data.Raw(), shaderBlob->GetBufferPointer(), storedShaderBlob.ResourceSize );
    }

    bool packageValid = ValidatePackage( *header, static_cast<const uint8_t*>( fileSpace.Allocation ) + fileSpace.HighWatermark );

    if ( !packageValid )
    {
        printf( "Output package validation failed\n" );
        return EXIT_FAILURE;
    }

    HANDLE outputFile = ::CreateFileW( argv[ 2 ], GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );

    if ( outputFile == INVALID_HANDLE_VALUE || outputFile == nullptr )
    {
        printf( "Could not create/open output file.\n" );
        return EXIT_FAILURE;
    }

    DWORD bytesWritten    = 0;
    BOOL  writeFileResult = 
        ::WriteFile( outputFile, 
                     fileSpace.Allocation, 
                     static_cast<DWORD>( fileSpace.HighWatermark ), 
                     &bytesWritten, 
                     nullptr );

    ::CloseHandle( outputFile );
    outputFile = nullptr;

    if ( writeFileResult == FALSE || bytesWritten != static_cast<DWORD>( fileSpace.HighWatermark ) )
    {
        printf( "Couldn't write output file.\n" );
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}