// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// P5.5-02 wave 2 package "shader", fifth pass (Stage 2 of the seamless
// cutaway): the frame pass's vertex shader. Written for this project from
// scratch. One triangle that covers the whole viewport, from the vertex index
// alone: no vertex buffer, no input layout. Built offline by
// modules/camera/tools/frame_shaders.py (vkd3d-shader, vs_5_0); the bytes the
// DLL creates its shader from are in include/fixed_spike_shader_frame_shaders.h.
float4 main(uint id : SV_VertexID) : SV_Position
{
    float2 corner = float2((id << 1) & 2, id & 2);
    return float4(corner.x * 2.0 - 1.0, 1.0 - corner.y * 2.0, 0.0, 1.0);
}
