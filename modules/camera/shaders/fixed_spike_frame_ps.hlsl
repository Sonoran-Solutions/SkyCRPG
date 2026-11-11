// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// P5.5-02 wave 2 package "shader", fifth pass (Stage 2 of the seamless
// cutaway): the frame pass's pixel shader. Written for this project from
// scratch. Drawn once per frame over the finished HDR scene (before image
// space) with blending src*alpha + dst*(1-alpha), so the output alpha is how
// far the pixel moves towards the void colour: it only ever darkens towards
// it, never adds. Built offline by modules/camera/tools/frame_shaders.py
// (vkd3d-shader, ps_5_0); the bytes are in
// include/fixed_spike_shader_frame_shaders.h. The constant layout is
// FrameConstants in include/fixed_spike_shader_frame_policy.h.
cbuffer FramePass : register(b0)
{
    float4 inverseColumn[4]; // clip position -> world position relative to the engine's position adjustment
    float4 viewport;         // x, y, 2/width, 2/height of the main view
    float4 depthRange;       // min depth, 1/(max-min), void threshold, unused
    float4 cut;              // feet x, feet y, plane z, band floor z (relative to the position adjustment)
    float4 shape;            // band on, inner radius, outer radius, falloff on
    float4 guard;            // player guard inner and outer radius, vignette strength, vignette start
    float4 voidColor;        // linear, as the main view's clear colour
};
Texture2D<float> sceneDepth : register(t0);

float4 main(float4 position : SV_Position) : SV_Target
{
    float depth = sceneDepth.Load(int3(int2(position.xy), 0));
    float2 ndc = float2((position.x - viewport.x) * viewport.z - 1.0, 1.0 - (position.y - viewport.y) * viewport.w);
    // The void (nothing drawn, depth still the clear value 1.0) is exactly the void colour.
    float amount = 1.0;
    if (depth < depthRange.z)
    {
        float4 clip = float4(ndc, (depth - depthRange.x) * depthRange.y, 1.0);
        float4 h = float4(dot(clip, inverseColumn[0]), dot(clip, inverseColumn[1]), dot(clip, inverseColumn[2]), dot(clip, inverseColumn[3]));
        float3 world = h.xyz / h.w;
        float reach = length(world.xy - cut.xy);
        // Soft cut edge: surfaces darken over the band under the plane; nothing above the plane
        // (actors are never cut) and nothing near the player's feet is touched.
        float band = 0.0;
        if (shape.x > 0.5 && world.z <= cut.z)
            band = smoothstep(cut.w, cut.z, world.z) * smoothstep(guard.x, guard.y, reach);
        // Radial falloff: full brightness inside the inner radius, the void colour beyond the outer.
        float fall = 0.0;
        if (shape.w > 0.5)
            fall = smoothstep(shape.y, shape.z, reach);
        amount = 1.0 - (1.0 - band) * (1.0 - fall);
    }
    // Vignette: a light darkening towards the frame's corners.
    float edge = guard.z * smoothstep(guard.w, 1.0, length(ndc) * 0.70710678);
    amount = 1.0 - (1.0 - amount) * (1.0 - edge);
    return float4(voidColor.rgb, saturate(amount));
}
