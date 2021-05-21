# Boondoggle!
A VR music visualizer based around shadertoy style ray marching.

Here's a [blog post](https://conorstokes.github.io/2016/10/12/boondoggle-the-vr-music-visualizer) describing the visualizer.

Currently only targetted at Windows and D3D 11. Supports Oculus HMD targets, with OpenVR hopefully coming soon.

Build by generating Visual Studio project files with [GENie](https://github.com/bkaradzic/GENie) (binary included in the repository) using a modern Visual Studio target (releases have been built with Visual Studio 2015 Professional) and then building via Visual Studio (or your favourite tool for building Visual Studio projects). Note, the Oculus SDK is needed for building and the path that contains "LibOVR" should be referenced (without a trailing slash) by the environment variable "OVR_DIR". 

Use the included compiler to build Visualizer Effects Packages (example included in the example directory). The boondoggle runtime can take the effects package as a command line parameter.

# Attributions
Note, this project makes use of/includes [json.h](https://github.com/sheredom/json.h) by Neil Henning for JSON parsing, [kissfft](https://github.com/itdaniher/kissfft) by Mark Borgerding for frequency analysis and [DDSTextureLoader](https://github.com/Microsoft/DirectXTK) from the DirectX Tool Kit for texture loading. Also, [GENie](https://github.com/bkaradzic/GENie) by Branimir Karadžić for generating project files. 
