#ifndef BOONDOGGLE_VISUALIZER_H__
#define BOONDOGGLE_VISUALIZER_H__

#include <stdint.h>

#pragma once

// Try and display on the oculus - if the oculus doesn't initialise, return false so we can try 
// and display windowed.
// Runs the display loop.
bool DisplayOculusVR( const wchar_t* packagePath );

// Display Windowed. Runs the display loop.
void DisplayWindowed( const wchar_t* packagePath, uint32_t width, uint32_t height, float fovInDegrees );

#endif // -- BOONDOGGLE_VISUALIZER_H__