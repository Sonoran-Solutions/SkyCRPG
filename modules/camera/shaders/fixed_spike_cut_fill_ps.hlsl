// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
// P5.5-02 wave 2 package "shader", Stage 3 step 3.1: the solid-cap pixel shader of the back-face fill
// (iCutFill). Written for this project from scratch. It is drawn a second time over a draw the cut has
// sliced, with the same patched vertex shader and plane but front faces culled, so it shows exactly the
// back faces under the plane: the inside of a cut wall or rock shell, seen through the cut, becomes a flat
// cap. Built offline by modules/camera/tools/frame_shaders.py (vkd3d-shader, ps_5_0); the bytes are in
// include/fixed_spike_shader_frame_shaders.h. Constants: FillConstants in include/fixed_spike_shader_fill_policy.h.
cbuffer CutFill : register(b13)
{
    float4 capColour;  // linear, as the scene's lit colour
    float4 guardRect;  // the player's screen rectangle in pixels: x0, y0, x1, y1
    float4 guardDepth; // x: the farthest depth of the player's box; y: 1 when the guard is on
};
struct Targets
{
    float4 colour : SV_Target0; // the lit colour target: the cap colour
    float4 motion : SV_Target1; // motion vectors: not written (blend write mask 0)
    float4 normal : SV_Target2; // view normal and specular mask: a neutral normal, no specular or reflection
    float4 extra  : SV_Target3; // the optional fourth target: not written (blend write mask 0)
};
Targets main(float4 position : SV_Position)
{
    // Player guard: no cap in front of the player inside the ellipse that fits the player's screen rectangle.
    if (guardDepth.y > 0.5)
    {
        float2 centre = (guardRect.xy + guardRect.zw) * 0.5;
        float2 extent = max((guardRect.zw - guardRect.xy) * 0.5, float2(1.0, 1.0));
        float2 d = (position.xy - centre) / extent;
        if (dot(d, d) <= 1.0 && position.z <= guardDepth.x)
            discard;
    }
    Targets t;
    t.colour = float4(capColour.rgb, 1.0);
    t.motion = float4(0.0, 0.0, 0.0, 1.0);
    t.normal = float4(0.5, 0.5, 0.0, 0.0);
    t.extra = float4(0.0, 0.0, 0.0, 0.0);
    return t;
}
