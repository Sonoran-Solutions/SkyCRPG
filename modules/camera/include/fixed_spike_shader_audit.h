// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// P5.5-02 wave 2 package "shader" (DYL-217). FNV-1a 64 of the bytecode of every
// vertex shader the cut may patch: the 70 Lighting and 125 Utility vertex
// shaders of the shipped `Skyrim - Shaders.bsa` (shadersfx\shaders011.fxp,
// archive SHA-256 fd1ba630f443353e...) that are rigid, position their vertices
// with the per-frame view-projection and have the one arithmetic shape the
// patcher accepts. They share 84 distinct bytecodes. Sorted. Hashes only: no
// shader bytecode is stored here. tests/fixed_spike_shader_policy_test.cpp
// recomputes the list from the archive when SKYCRPG_SHADER_ARCHIVE names it.
#include <array>
#include <cstdint>

namespace skycrpg::camera::fixed::shader {
inline constexpr std::array<std::uint64_t,84> auditedVertexShaders{
    0x00491fef9ddcd9acull,0x005addae52e3ab76ull,0x04e6b0699d787ae1ull,0x071500543ddd34a8ull,0x0780cc0e9e42553eull,0x0a629f416b0781a6ull,
    0x0b2e742a2fb413cdull,0x0d13372c97d7ec42ull,0x139d127c55ceb5e9ull,0x13e92341f762ff72ull,0x15121220d1e4ac13ull,0x18e07ded63b18581ull,
    0x1b9552296cc56009ull,0x21862a1064507061ull,0x22de92dd79008bf6ull,0x24528c6bc9dfdeedull,0x246d880669c8e7a7ull,0x27e0ac4042596330ull,
    0x2cb95f34b7f6f950ull,0x2d2ac35ab104c90aull,0x2d31c4fa0244a353ull,0x30376dcc2be84e85ull,0x31250df84c67a527ull,0x314fd6a1807eb536ull,
    0x3313cbafd10ab731ull,0x339f01069a3ff1c0ull,0x3a7e56b9cdfcaa5cull,0x3d98f2f3a45613f2ull,0x3dbec695284787a5ull,0x45269966bcc13d47ull,
    0x461f2cf45d2cefb9ull,0x487e3c45aff9d872ull,0x4a8b5e8fae300981ull,0x4dd7dbc544fa3a44ull,0x4e247ae41361d74eull,0x4e9ba85a2863ebeeull,
    0x5044baa8d13d2c0cull,0x50d1004455d44f56ull,0x50fef3411604715full,0x56d80fae758da685ull,0x5a41cba35ab94ceaull,0x5b1c7fb8b5757922ull,
    0x5ff35cd18490766bull,0x61745287d63f4a96ull,0x6372f53644b41a76ull,0x660649656c9c769full,0x680a58244a5b55c2ull,0x6a09637a8b2a1674ull,
    0x7c3338bf6dbb6529ull,0x810198eba6249807ull,0x8454129888d8b736ull,0x85977ed38669a524ull,0x9267b626373580aaull,0x92f54567d2fa43b7ull,
    0x954f65bd9a71d6eaull,0x956645833462d27aull,0x9ac8e03b8900c053ull,0x9da75696846a3109ull,0xa5458d91e1247e10ull,0xa839f4d77dfc1970ull,
    0xab21a23cb2e85813ull,0xb0042d6439fcb7d8ull,0xb201d67aa86adf60ull,0xb2a57a8af9091700ull,0xb657da67afd46dceull,0xb95de42c6f585546ull,
    0xbb4963f52b7e1038ull,0xc38c85ff42f74ad9ull,0xc462d311868e9c3cull,0xc518e9a39b805042ull,0xcb411d52601d6557ull,0xced9200a35920a5dull,
    0xd0d789c5f8012162ull,0xd1a70943e7b81607ull,0xd2375f2af210b1beull,0xd72929e7e4cb097aull,0xdad69f92e6afa696ull,0xe1559f2054be83e3ull,
    0xe8f3fadb1ffd1597ull,0xe9d95673117b3b5bull,0xecd49bdc42d94760ull,0xf6ed43af302b8bc4ull,0xfbe360793d366f69ull,0xfd07664cff39f41bull,};
} // namespace skycrpg::camera::fixed::shader
