#ifndef BOONDOGGLE_HELPERS_H__
#define BOONDOGGLE_HELPERS_H__

#pragma once

#if defined( _MSC_VER )
#define BEH_FORCE_INLINE __forceinline
#else
#define BEH_FORCE_INLINE inline
#endif

template < typename COMType >
BEH_FORCE_INLINE void COMRelease( COMType*& toRelease )
{
    if ( toRelease != nullptr )
    {
        toRelease->Release();
        toRelease = nullptr;
    }
}

// Simple smart pointer type for auto releasing COM pointers.
// While I wouldn't use a reference counted smart pointer usually,
// for COM you are already paying for reference counting and quite
// often for D3D you end up with an array of such pointers.
template <typename IntrusiveType>
struct COMAutoPtr
{
    IntrusiveType* raw;

    BEH_FORCE_INLINE COMAutoPtr( IntrusiveType* value = nullptr )
        : raw( value ) {}

    BEH_FORCE_INLINE void Release()
    {
        COMRelease( raw );
    }

    BEH_FORCE_INLINE IntrusiveType* operator->() { return raw; }

    BEH_FORCE_INLINE const IntrusiveType* operator->() const { return raw; }

    BEH_FORCE_INLINE IntrusiveType& operator*() { return *raw }

    BEH_FORCE_INLINE const IntrusiveType& operator*() const { return *raw }
    
    BEH_FORCE_INLINE COMAutoPtr( const COMAutoPtr< IntrusiveType >& from )
    {
        raw = from.raw;

        if ( raw != nullptr )
        {
            raw->AddRef();
        }
    }

    BEH_FORCE_INLINE COMAutoPtr< IntrusiveType >& operator=( const COMAutoPtr< IntrusiveType >& from )
    {
        if ( from.raw != nullptr )
        {
            from.raw->AddRef();
        }

        if ( raw != nullptr )
        {
            raw->Release();
        }

        raw = from.raw;

        return *this;
    }

    BEH_FORCE_INLINE COMAutoPtr< IntrusiveType >& operator=( IntrusiveType* from )
    {
        if ( from != nullptr )
        {
            from->AddRef();
        }

        if ( raw != nullptr )
        {
            raw->Release();
        }

        raw = from;

        return *this;
    }

    BEH_FORCE_INLINE ~COMAutoPtr()
    {
        Release();
    }
};

#endif // --BOONDOGGLE_HELPERS_H__