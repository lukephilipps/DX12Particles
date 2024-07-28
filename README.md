# DX12 GPU-Based Particle Sim

A GPU-based particle simulation implemented in DirectX 12. All buffers are staged in the GPU and the simulation is calculated via compute, leaving only command list building on the CPU.

Emitters currently contain simple programmable attributes, such as:
* Emit count per frame
* Emitter AABB
* Spawn velocity range
* Spawn acceleration range
* Lifetime
* Scale over lifetime

![Image of the simulation.](https://github.com/lukephilipps/lukephilipps/blob/5296b6283495af5f030a637af7974a576314054a/Particles.gif)

## Next Additions
This project is acting as my introduction to DX12 programming, and has been a wonderful learning experience. Moving forward my main two focuses are making the simulation more efficient and learning as much as I can.
* Assemble command lists in GPU via indirect commands
* Emitter editing via GUI
* Fix SSAO implementation
* Creating a model loading pipeline

## Using
* [DDSTextureLoader](https://github.com/microsoft/DirectXTK12/wiki/DDSTextureLoader)

## References
* [Jeremiah van Oosten's Amazing DX12 Tutorials](https://www.3dgep.com/category/graphics-programming/directx/directx-12/)
* [turanszkij's Blog Post About GPU-Based Particle Systems in Their Engine, Wicked Engine](https://wickedengine.net/2017/11/gpu-based-particle-simulation/)
* [DirectX-Specs](https://microsoft.github.io/DirectX-Specs/)
* [Microsoft's Indirect Drawing Sample](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12ExecuteIndirect)
* [Microsoft's MiniEngine Sample](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine)
* [Random Function Used in Emitter](https://stackoverflow.com/questions/5149544/can-i-generate-a-random-number-inside-a-pixel-shader)
