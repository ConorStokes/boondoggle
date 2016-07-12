#ifndef BOONDOGGLE_VISUALIZER_H__
#define BOONDOGGLE_VISUALIZER_H__

#include <stdint.h>

#pragma once

namespace Boondoggle
{
    void DisplayOculusVR();

    void DisplayWindowed( uint32_t width, uint32_t height );
}

#endif // -- BOONDOGGLE_VISUALIZER_H__