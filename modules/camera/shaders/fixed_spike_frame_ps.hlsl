// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// P5.5-02 wave 2 package "shader", fifth pass (Stage 2 of the seamless
// cutaway): the frame pass's pixel shader. Written for this project from
// scratch. Drawn once per frame over the finished HDR scene (before image
// space) with blending src*alpha + dst*(1-alpha), so the output alpha is how
// far the pixel moves towards the void colour: it only ever darkens towards
// it, never adds. Built offline by modules/camera/tools/frame_shaders.py
// (vkd3d-shader, ps_5_0); the bytes are in
// include/fixed_spike_shader_frame_shaders.h. The constant layout is
// FrameConstants in include/fixed_spike_shader_frame_policy.h. The door void
// (iDoorVoid=1, include/fixed_spike_shader_door_policy.h) is read only when
// doorShape.x (the doors drawn) is at least 1.
cbuffer FramePass : register(b0)
{
    float4 inverseColumn[4]; // clip position -> world position relative to the engine's position adjustment
    float4 viewport;         // x, y, 2/width, 2/height of the main view
    float4 depthRange;       // min depth, 1/(max-min), void threshold, unused
    float4 cut;              // feet x, feet y, plane z, band floor z (relative to the position adjustment)
    float4 shape;            // band on, inner radius, outer radius, falloff on
    float4 guard;            // player guard inner and outer radius, vignette strength, vignette start
    float4 voidColor;        // linear, as the main view's clear colour
    float4 doorShape;        // doors drawn (0..4), soft edge, depth behind the plane, unused
    float4 door[8];          // per door: plane point x, y, normal x, y; lane low z, high z, half width, start
};
Texture2D<float> sceneDepth : register(t0);

// Behind a load door's plane past its start (soft over the edge) and short of the depth, inside its lane
// (soft over the edge beyond the half width) and between its heights (soft over the edge outside them).
float doorFade(float3 world)
{
    float fade = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        if (float(i) < doorShape.x)
        {
            float4 a = door[2 * i];
            float4 b = door[2 * i + 1];
            float dx = world.x - a.x;
            float dy = world.y - a.y;
            float back = -(dx * a.z + dy * a.w);
            float across = abs(dx * a.w - dy * a.z);
            float edge = doorShape.y;
            float depth = doorShape.z;
            float lane = smoothstep(b.w, b.w + edge, back) * (1.0 - smoothstep(depth, depth + edge, back)) * (1.0 - smoothstep(b.z, b.z + edge, across)) *
                smoothstep(b.x - edge, b.x, world.z) * (1.0 - smoothstep(b.y, b.y + edge, world.z));
            fade = max(fade, lane);
        }
    }
    return fade;
}

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
        // Door void: the passage behind a nearby load door goes to the void colour.
        if (doorShape.x > 0.5)
            amount = max(amount, doorFade(world));
    }
    // Vignette: a light darkening towards the frame's corners.
    float edge = guard.z * smoothstep(guard.w, 1.0, length(ndc) * 0.70710678);
    amount = 1.0 - (1.0 - amount) * (1.0 - edge);
    return float4(voidColor.rgb, saturate(amount));
}
