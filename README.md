# DX12 GPU-Based Particle Sim

A GPU-based particle simulation utilizing DirectX 12. All buffers are staged in the GPU and the simulation is calculated via compute, leaving only command list building on the CPU.

Emitters currently contain simple programmable attributes, such as:
* Emit count per frame
* Emitter AABB
* Spawn velocity range
* Spawn acceleration range
* Lifetime
* Scale over lifetime

<img src="https://github.com/lukephilipps/lukephilipps/blob/351258d83017ff3066882d1308b2a0d4a46341a1/Particles_1.gif" alt="A sim with force fields." width="776"/>
<img src="https://github.com/lukephilipps/lukephilipps/blob/fd41e33bd5a7605da1c1f272e32a44428d43d031/Particles_0.gif" alt="A simulation of a torch." width="776"/>
<img src="https://github.com/lukephilipps/lukephilipps/blob/fd41e33bd5a7605da1c1f272e32a44428d43d031/Particles.png" alt="Image of the simulation." width="776"/>
<p align="center"><img src="https://github.com/lukephilipps/lukephilipps/blob/63fad7702fcf405f7b4c0c137f84b3030c2ec82f/yippeee.gif" alt="The confetti creature."/></p>

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
