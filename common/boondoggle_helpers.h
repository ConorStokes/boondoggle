#ifndef BOONDOGGLE_HELPERS_H__
#define BOONDOGGLE_HELPERS_H__

#pragma once

template < typename COMType >
void COMRelease( COMType*& toRelease )
{
    if ( toRelease != nullptr )
    {
        toRelease->Release();
        toRelease = nullptr;
    }
}

// Simple RAII type for auto releasing COM pointers.
template <typename IntrusiveType>
struct COMAutoPtr
{
    IntrusiveType* raw;

    COMAutoPtr( IntrusiveType* value = nullptr )
        : raw( value ) {}

    void Release()
    {
        COMRelease( raw );
    }

    IntrusiveType* operator->() { return raw; }

    const IntrusiveType* operator->() const { return raw; }

    IntrusiveType& operator*() { return *raw }

    const IntrusiveType& operator*() const { return *raw }
    
    COMAutoPtr( const COMAutoPtr< IntrusiveType >& ) = delete;

    COMAutoPtr< IntrusiveType >& operator=( const COMAutoPtr< IntrusiveType >& ) = delete;

    ~COMAutoPtr()
    {
        Release();
    }
};

#endif // --BOONDOGGLE_HELPERS_H__