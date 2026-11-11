// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#pragma once
// P5.5-02 wave 2 package "shader" (DYL-217), second pass. FNV-1a 64 of the
// bytecode of every Effect-class vertex shader the cut may patch: the 332 of
// the 606 Effect vertex shaders of the shipped `Skyrim - Shaders.bsa`
// (shadersfx\shaders011.fxp, archive SHA-256 fd1ba630f443353e...) that are
// rigid, position their vertices with the per-frame view-projection and have
// the one arithmetic shape the patcher accepts (the other 274 are skinned).
// They share 114 distinct bytecodes; none is also on the Lighting/Utility list.
// Sorted. Hashes only: no shader bytecode is stored here.
// tests/fixed_spike_shader_policy_test.cpp recomputes the list from the
// archive when SKYCRPG_SHADER_ARCHIVE names it.
#include <array>
#include <cstdint>

namespace skycrpg::camera::fixed::shader {
inline constexpr std::array<std::uint64_t,114> auditedEffectVertexShaders{
    0x05d6c763b47e938bull,0x070fe03c128c2380ull,0x07d8bfaffc543086ull,0x08ecd3896f01ebd9ull,0x0e7e242e1e3cede0ull,0x0e85170d98de624full,
    0x10f5d65790d425a2ull,0x161e527774fcc1a9ull,0x16544d499246400eull,0x1865e9b45f8802dfull,0x1dfa92eb88b102b7ull,0x24c1837f1dd58335ull,
    0x2540bda832487b54ull,0x2b74465afd1f0a8cull,0x2dd5806163c0662dull,0x2e72f569757777ccull,0x3280b7b073179cf3ull,0x35550bd1821f6e10ull,
    0x37074a8134fe4693ull,0x38530db8b12a965aull,0x3a861f35cd78eb0full,0x3d27e772365a6287ull,0x3df1945e5ebf6298ull,0x3f97bdc52de6e256ull,
    0x3ffe6a7e20115c12ull,0x4427cc5c799a3ad0ull,0x45937ff88dc1b97aull,0x474f67be82806ecfull,0x47e4dea6f4433f3eull,0x48177236a2a0503eull,
    0x4ac5e9d34a595ff5ull,0x4bace726ab4a9988ull,0x4be48d1f35617801ull,0x501f92a4665a0477ull,0x50375e237e1a0c63ull,0x518b73381be52178ull,
    0x55ae8d496a5a2a1dull,0x56616ee386141965ull,0x5b365fa4d95e6271ull,0x5cb2cc84e026fad7ull,0x5eeaefad13cf0314ull,0x5fea0c51e25b07b6ull,
    0x61de3228ffe3fb8dull,0x6509c4069044b634ull,0x65dfb56941d4f143ull,0x6617d686b4f22a1aull,0x66223e1e43ffedacull,0x67a1e86c03321652ull,
    0x684cd955e45c100eull,0x686e5e33468e0aecull,0x6d40f9b9a8026c01ull,0x714b8b1d684d2d42ull,0x7d64600b184ae72bull,0x805fad9edd12686bull,
    0x82cd39745cb45c2bull,0x830dd6ec461d9815ull,0x8406c15c9cb16a46ull,0x841533fb33b5f86bull,0x883dec383a76ae00ull,0x89af044e799abd2aull,
    0x8a3ef4375e092674ull,0x8c86f455aedaf1b6ull,0x95f06772f9d96c2dull,0x970ea1961ed11eb2ull,0x98ab5ecd9a037b43ull,0x99939ffa9c56056full,
    0xa5cadbc03db438e2ull,0xa62331926a2b4ccdull,0xa7010de52b46a00full,0xa7a3f698136db3b8ull,0xa8e87395815cb4b3ull,0xa9329bef95170d97ull,
    0xaa97dded7f387187ull,0xadf11f3004b129b5ull,0xafd4ce899450a78full,0xb82f8e9e4dc53ed9ull,0xbabf054f4e2c8dceull,0xbc4e163c429f49bdull,
    0xbc5a69f9de0a4765ull,0xc1693c35b3f13ee0ull,0xc1b83c5a98ffb50cull,0xc453e80322b2f968ull,0xc4713d7c69eba006ull,0xc641a90fc8ea2302ull,
    0xc82c13c7ddbcbcdcull,0xc8724d7802841f36ull,0xcc5606a6def6a8a3ull,0xcd5b70b803af423bull,0xce2b07287d0162acull,0xcee1d1bd3d46a081ull,
    0xcfb332b209ff00a9ull,0xd0ddd292f72ec5d6ull,0xd37a5254ff6105a5ull,0xd608ef23032c6c01ull,0xd62572dce59befc6ull,0xd9383351e7e68c50ull,
    0xdcbd4596c3db8d49ull,0xdd4217a9ec23d153ull,0xde4ada25cf09c7b7ull,0xe069f242165fe7afull,0xe19a34c2a43e3e81ull,0xe206054f768b0eb1ull,
    0xe55b976770b995ffull,0xe6589fb038a2ae00ull,0xe856e76fc35d737dull,0xe87d2c693fbd6fadull,0xe9c5da9a6eb7e938ull,0xeb603fdac15d9c0cull,
    0xebf4ada6cff47291ull,0xf4946815b3effb9aull,0xf9427d61a0e23249ull,0xfb55c9d0abec32eaull,0xfba6c29807f69ca1ull,0xff58401e5c7f6b63ull,
};
} // namespace skycrpg::camera::fixed::shader
