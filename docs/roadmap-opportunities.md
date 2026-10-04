# Modernization opportunities (appendix to ROADMAP.md)

Generated from the 2026-10-04 code analysis. Gains are estimates from reading the code and from comparable idTech3 forks; nothing was measured.

Compatibility values: `client-only-opt-in`, `client-only-transparent`, `server-only-transparent`, `needs-vm-or-mod-changes`, `breaks-protocol-or-mods`.

## Index

| id | title | effort | compatibility |
|---|---|---|---|
| RB-1 | Measure first: non-cheat perf overlay, GPU timer queries, draw-call/state counters, frametime log | S | client-only-opt-in |
| RB-2 | Quick wins: drop the glow-pass glFinish, use CopyTexSubImage for postprocess gamma, skip an identity LUT pass | S | client-only-transparent |
| RB-3 | Larger tess buffer (renderer-local constant) and moving big stack arrays to static storage | S | client-only-transparent |
| RB-4 | Trim per-batch GL call overhead in the stage iterator | S | client-only-transparent |
| RB-5 | Static world geometry in VBOs (fixed-function GL 1.5, no GLSL) with per-batch fallback | L | client-only-opt-in |
| RB-6 | Streaming ring-buffer VBO for the dynamic tess path (models, effects, 2D) | M | client-only-opt-in |
| RB-7 | FBO scene target: no more full-screen copies, plus the foundation for MSAA, render scale, high precision, bloom and grading | M | client-only-opt-in |
| RB-8 | Color-grading presets on top of the existing 3D-LUT gamma pass | S | client-only-opt-in |
| RB-9 | GPU per-pixel dynamic lights (ARB_fragment_program), opt-in r_dlightMode | M | client-only-opt-in |
| RB-10 | Collapse 3+ shader stages into one pass using 4+ texture units | M | client-only-opt-in |
| RB-11 | Frame pacing and input latency: bounded GPU queue via ARB_sync, documented adaptive vsync, optional precise limiter | S | client-only-opt-in |
| RB-12 | Graphics quality presets built from existing cvars (no renderer code) | S | client-only-opt-in |
| RB-13 | Reintroduce a render thread (front-end/back-end overlap): not recommended now | XL | client-only-transparent |
| FE-1 | Per-frame arena for Ghoul2 renderable surfaces and scratch buffers (also fixes a leak) | S | client-only-transparent |
| FE-2 | Fix the 32-dlight shift UB that switches off all world dynamic lighting | S | client-only-transparent |
| FE-3 | Cache per-model and per-instance lookups in the Ghoul2 hot path (bit-exact) | M | client-only-transparent |
| FE-4 | SIMD for Ghoul2 bone math, plus a NEON port of skinning | M | client-only-transparent |
| FE-5 | Skin each Ghoul2 surface once per frame and reuse it across passes | M | client-only-transparent |
| FE-6 | Worker pool for Ghoul2 skinning (and later skeletons) in the frontend | L | client-only-transparent |
| FE-7 | Lazy bolt-chain evaluation: full skeleton only for rendered models | L | client-only-transparent |
| FE-8 | GPU skinning for Ghoul2 with static per-surface vertex buffers | XL | client-only-transparent |
| FE-9 | Dynamic-light staging, culling and priority (keep the 32 that matter) | M | client-only-opt-in |
| FE-10 | Lift scene limits for effect-heavy fights: entity index fix, wider sort key, poly culling | M | client-only-transparent |
| FE-11 | Opt-in quaternion interpolation for Ghoul2 animation (smoother limbs, fewer flops) | M | client-only-opt-in |
| FE-12 | Bring back flares with non-stalling occlusion queries (map lights and dlights) | M | client-only-opt-in |
| FE-13 | Modernize the dormant weather system and allow per-map opt-in weather | M | client-only-opt-in |
| FE-14 | Cache surface-sprite placement and add an opt-in density multiplier | M | client-only-transparent |
| FE-15 | 'Showcase' quality preset built only from existing cvars | S | client-only-opt-in |
| FE-16 | Measurement first: microsecond phase profiler, plus synthetic Ghoul2 golden and benchmark tests | M | client-only-transparent |
| FE-17 | Release build flags: LTO, optional PGO, explicit FP contraction policy | S | client-only-transparent |
| FE-18 | Parallel image decoding at level load (and optional DDS for HD packs) | L | client-only-transparent |
| VQ-1 | Graphics presets: Classic (default) / Enhanced / Ultra, via a cfg in assetsmv plus a menu_patch entry | S | client-only-opt-in |
| VQ-2 | Color grading, saturation/contrast/vibrance and dithering folded into the existing post-gamma 3D LUT | S | client-only-opt-in |
| VQ-3 | Dynamic glow: resolution-aware glow buffer, remove the per-frame glFinish, skip portal/mirror views | S | client-only-opt-in |
| VQ-4 | Streamline the default post-gamma pass (copy into existing storage, skip it when it is the identity) | S | client-only-transparent |
| VQ-5 | Infrastructure: FBO scene target plus a small built-in post-process program manager (ARB first, GLSL 1.20 optional) | M | client-only-opt-in |
| VQ-6 | HDR scene buffer (RGBA16F) with tonemapping, plus modern threshold bloom (mip-chain / dual-Kawase) | M | client-only-opt-in |
| VQ-7 | Render scale and supersampling (r_renderScale), supersampled screenshots and AVI | S | client-only-opt-in |
| VQ-8 | Alpha-to-coverage for alpha-tested stages when MSAA is on | S | client-only-opt-in |
| VQ-9 | Per-pixel dynamic lights (saber light, blaster bolts, explosions), with a higher internal light cap | M | client-only-opt-in |
| VQ-10 | FXAA (later SMAA) plus contrast-adaptive sharpening on the 3D view | M | client-only-opt-in |
| VQ-11 | Cinematic capture: motion blur by sub-frame accumulation (and optional depth of field) for demos and AVI | M | client-only-opt-in |
| VQ-12 | Soft particles for the engine-side FX system | M | client-only-opt-in |
| VQ-13 | SSAO / contact shadows for vertex-lit models | M | client-only-opt-in |
| VQ-14 | High-resolution texture-pack enablement: NPOT resampling, pre-compressed DDS (BC1/3/5/7), faster lookups | M | client-only-opt-in |
| VQ-15 | 4K text polish: high-resolution console font atlases and an optional aspect-correct HUD for legacy cgame | S | client-only-opt-in |
| VQ-16 | Player shadow maps (soft, fairness-aware) as a new opt-in shadow mode | L | client-only-opt-in |
| VQ-17 | Normal/specular/parallax mapping and PBR (a rend2-class renderer): defer, consider an optional separate renderer later | XL | client-only-opt-in |
| VQ-18 | Document visual cvars and fix doc/code default mismatches; add a 'r_postinfo' status line to gfxinfo | S | client-only-transparent |
| SV-1 | Server profiler with microsecond timers and per-phase counters, plus a load-test harness that runs the real code | M | server-only-transparent |
| SV-2 | Table-driven static Huffman encode/decode (bit-identical output) | S | server-only-transparent |
| SV-3 | Size the snapshot entity ring for high tick rates and bots (power-of-two, cvar-scaled) | S | server-only-transparent |
| SV-4 | Per-client snapshot entity budget with priority and graceful degradation instead of dropped snapshots | M | server-only-transparent |
| SV-5 | Send queued fragments as soon as the rate allows (do not wait for the next server frame) | M | server-only-transparent |
| SV-6 | Coalesce configstring updates per frame (opt-in) to stop reliable-command overflow | M | server-only-transparent |
| SV-7 | Faster world queries: clip the trace's entity box to the world hit, doubly linked sector lists, optional deeper tree | S | server-only-transparent |
| SV-8 | Build and encode client snapshots in parallel after GAME_RUN_FRAME | L | server-only-transparent |
| SV-9 | Snapshot-builder micro-optimizations, including visibility shared across clients | M | server-only-transparent |
| SV-10 | Faster QVM JIT for the game module (port Quake3e's optimizing x86/x64 compiler) | XL | server-only-transparent |
| SV-11 | Server-side demo recording (svrecord / auto-record every match per player) | M | server-only-transparent |
| SV-12 | Relay/broadcast mode ('MVTV') so spectators can go beyond the 32 slots | XL | server-only-transparent |
| SV-13 | Hardening against abusive load: optional usercmd rate cap, per-client CPU accounting, throttled 'disconnect' replies | S | server-only-transparent |
| SV-14 | Tick/rate presets plus sv_fps clamp and fixes to hibernation docs and behavior | S | server-only-transparent |
| SV-15 | Engine-provided entity history ('rewind') service for mod-side lag compensation (fork API) | L | needs-vm-or-mod-changes |
| PX-1 | One-command graphics presets (classic / modern / competitive / movie) built only from existing cvars | S | client-only-opt-in |
| PX-2 | Color grading through the existing post-process gamma 3D LUT | S | client-only-opt-in |
| PX-3 | Opt-in Hor+ widescreen FOV correction for mods without their own | S | client-only-opt-in |
| PX-4 | Engine-side free-fly camera for demos (works with every mod and every 1.02/1.03/1.04 demo) | M | client-only-opt-in |
| PX-5 | Camera paths and keyframes for demos (cinematic fly-throughs) | M | client-only-opt-in |
| PX-6 | Clean feed: hide the HUD in demos, photos and video without relying on mod cvars | S | client-only-opt-in |
| PX-7 | Demo transport controls, on-screen timeline and smoother slow motion | S | client-only-opt-in |
| PX-8 | Demo seeking: fast-forward, rewind and jump to time or bookmark | L | client-only-opt-in |
| PX-9 | Built-in FFmpeg export (MP4/H.264/H.265 via pipe), cross-platform | M | client-only-opt-in |
| PX-10 | Motion blur (sub-frame accumulation) for video export, supersampling later | M | client-only-opt-in |
| PX-11 | Fix AVI audio capture when OpenAL is enabled | S | client-only-transparent |
| PX-12 | Faster, lossless capture: PNG screenshots, PBO async readback, libjpeg-turbo | S | client-only-opt-in |
| PX-13 | Photo mode (freeze, fly, frame, shoot) | S | client-only-opt-in |
| PX-14 | Engine-drawn performance, network and speed overlay that works with all mods | S | client-only-opt-in |
| PX-15 | Modern controller support: SDL GameController, dual-stick look, gyro and rumble | M | client-only-opt-in |
| PX-16 | Borderless desktop fullscreen and Alt+Enter | S | client-only-opt-in |
| PX-17 | Console and chat quality of life | S | client-only-opt-in |
| PX-18 | Join links, quick play and find-a-player | M | client-only-opt-in |
| PX-19 | Optional Discord Rich Presence | M | client-only-opt-in |
| PX-20 | Automatic highlight bookmarks for auto-demos | S | client-only-opt-in |
| PX-21 | timedemo statistics for benchmarking (percentiles, CSV) | S | client-only-opt-in |
| PX-22 | Demo cut and trim for shareable clips | M | client-only-opt-in |
| PX-23 | Sub-millisecond time and cgame camera cooperation through a fork-specific API (MVSDK mods only) | L | needs-vm-or-mod-changes |
| PX-24 | Server browser 2.0 in the engine-shipped jk2mvmenu | M | needs-vm-or-mod-changes |
| BM-1 | Monotonic microsecond timer (Sys_Microseconds) for all profiling paths | S | client-only-transparent |
| BM-2 | `benchmark` command: per-frame capture, percentiles, CSV, uncapped timedemo | M | client-only-transparent |
| BM-3 | Reproducible Windows client benchmark protocol, usable today with no code | S | client-only-transparent |
| BM-4 | Golden-frame visual regression test for the classic look | S | client-only-transparent |
| BM-5 | Renderer counters for draw calls, texture binds and state changes (r_speeds 8) | S | client-only-transparent |
| BM-6 | GPU frame time via GL_ARB_timer_query | M | client-only-opt-in |
| BM-7 | Server frame statistics: `serverstats` command and per-second CSV | M | server-only-transparent |
| BM-8 | Bot-based server load test protocol (real assets) with the documented gotchas | S | server-only-transparent |
| BM-9 | sv_benchBotSnapshots: encode bot snapshots and discard them | S | server-only-transparent |
| BM-10 | Real-client network load with headless clients | S | server-only-transparent |
| BM-11 | CI: asset-free dedicated server smoke test and bot soak (verified here) | M | server-only-transparent |
| BM-12 | CI: headless client smoke test via SDL offscreen and Mesa llvmpipe (verified here) | S | client-only-transparent |
| BM-13 | CI: sanitizer soak and static analysis jobs | M | server-only-transparent |
| BM-14 | CI: deterministic instruction-count performance regression check (Callgrind) | M | server-only-transparent |
| BM-15 | Opt-in Tracy profiler build | M | client-only-transparent |
| BM-16 | Opt-in performance overlay HUD to make improvements visible | S | client-only-opt-in |
| BM-17 | CI hygiene: retired runner images, and no engine in the tests workflow | S | client-only-transparent |
| BM-18 | Docs: fix CVARS.rst drift around hibernation and document the test gotchas | S | server-only-transparent |

## Rendering backend performance (tr_backend.cpp, tr_shade.cpp, tr_shade_calc.cpp, tr_surface.cpp, tr_cmds.cpp, tr_init.cpp, qgl.h, sdl_window.cpp, plus the frame loop in common.cpp)

**Current state.** HOW VERTICES REACH THE GPU. Everything goes through one CPU staging struct, `tess` (tr_local.h:1513-1545). For every draw surface, the backend calls rb_surfaceTable (tr_surface.cpp:1712-1731), which copies the surface into tess every frame:
- world faces: RB_SurfaceFace (tr_surface.cpp:1281-1342);
- curved patches: RB_SurfaceGrid (1382-1538), which also recomputes the LOD row/column tables per frame;
- triangle soups: RB_SurfaceTriangles (298-364);
- MD3: RB_SurfaceMesh (1237-1273);
- Ghoul2: tr_ghoul2.cpp:2143, CPU skinning already SSE2 at 2199-2260.
What gets copied: xyz as a 16-byte padded vec4, the normal (only when shader->needsNormal), base st plus up to 4 lightmap st sets, a vertex color, dlight bits, and indexes rebased to 32-bit (GL_INDEX_TYPE is GL_UNSIGNED_INT, tr_local.h:16-17). The vertex color comes from ComputeFinalVertexColor per vertex per frame, with light-style math (tr_surface.cpp:251-290).

At RB_EndSurface (tr_shade.cpp:1562-1618), the shader's iterator runs. It is almost always RB_StageIteratorGeneric (tr_shade.cpp:1214-1339), because r_ignoreFastPath defaults to 1 (tr_init.cpp:1098) and makes ComputeStageIteratorFunc return early (tr_shader.cpp:2619-2622). The generic iterator does this:
1. Deforms mutate tess.xyz/normal in place (RB_DeformTessGeometry, tr_shade_calc.cpp:567-610).
2. glVertexPointer points at tess.xyz, followed by glLockArraysEXT(0,numVertexes) (CVA, tr_shade.cpp:1271-1276).
3. Per stage (RB_IterateStagesGeneric, 1111-1208): ComputeColors (719-984) writes tess.svars.colors and ComputeTexCoords (991-1094) writes svars.texcoords[0..1]. Then gl*Pointer is set to these client arrays, and one glDrawElements(GL_TRIANGLES, n, GL_UNSIGNED_INT, tess.indexes) is issued with client-side indexes (R_DrawElements, 151-187). r_primitives 1/3 switch to the legacy glArrayElement and immediate-mode strip paths.

Nothing is resident on the GPU. qgl.h #defines the GL 1.1 entry points directly (qgl.h:31+), and GLimp_InitExtensions loads only multitexture, CVA, NV combiners, ARB vp/fp, texture_rectangle, aniso and lod_bias (tr_init.cpp:370-622): no VBO, FBO, sync or timer queries. glext.h (version 20141118) already declares all of them (glext.h:530, 1171, 1478, 1549).

BATCH SIZE AND FLUSHES. SHADER_MAX_VERTEXES is 1000 and SHADER_MAX_INDEXES is 6000 (qcommon/qfiles.h:10-11), so tess is about 128 KB. A batch ends when:
- the sort key's shader, fog or dlight changes, or the entity changes for a shader that is not entityMergable (tr_backend.cpp:595-605);
- the next surface would overflow the buffer (RB_CHECKOVERFLOW tr_local.h:1557 calls RB_CheckOverflow tr_surface.cpp:27-49; grids split by rows at 1437-1448; shadow-shader batches stop at half size);
- a 2D shader changes (tr_backend.cpp:886-892);
- every command boundary (tr_backend.cpp:1029, 1237, 1291, 1312, 1372).
The 2048x2048 lightmap atlas (tr_bsp.cpp:168-283) already merges lightmap pages. World surfaces sharing a texture therefore share a shader_t and arrive consecutively in the radix-sorted list (tr_main.cpp:1058; sort key layout tr_local.h:948-950). So the 1000-vertex cap, not shader changes, is often what splits big world batches.

MULTI-PASS vs MULTITEXTURE. CollapseMultitexture (tr_shader.cpp:2729-2830, called once at 3329) can merge only stages 0+1. It requires a matching blend pair from a fixed table (modulate or add, 2687-2717) and identical rgbGen/alphaGen. GL_SelectTexture only knows units 0 and 1 (tr_backend.cpp:52-77). As a result, every stage after the collapsed pair costs one more full draw of the batch: glow stages, detail, envmap, and the light-styled lightmaps (CGEN_LIGHTMAP1-3, tr_shade.cpp:859-870). The same goes for each dlight touching the batch (ProjectDlightTexture/NewProjectDlightTexture, tr_shade.cpp:409-675, a CPU per-vertex loop plus an index filter per light), the fog pass (685-712, 1303-1305) and surface-sprite stages (1325-1338). With r_DynamicGlow, the whole drawsurf list is walked and re-tessellated a second time for hasGlow shaders (tr_backend.cpp:1054-1057).

PER-VERTEX CPU WORK PER FRAME, PER BATCH:
- Always: the surface copy (about 60-100 B written per vertex). Per stage, a memset or memcpy of colors (4 B/vertex) and texcoords (8 B/vertex/bundle).
- Only where shaders ask for it:
  - deformVertexes wave: one table lookup per vertex (tr_shade_calc.cpp:107-149);
  - deform normals: 3 noise lookups per vertex (158-182);
  - autosprite / autosprite2 rebuild the quads (378-558);
  - tcGen environment: a normalize per vertex (908-932);
  - tcMod turb, scroll, scale, rotate, stretch and transform: 2D affine or sin-table work per vertex (937-1035);
  - rgbGen lightingDiffuse (1104-1152), alphaGen specular (1044-1097) and alphaGen portal (tr_shade.cpp:922-951);
  - fog texcoords (tr_shade_calc.cpp:823-901), plus a second fog-texcoord evaluation for adjustColorsForFog (749-803);
  - dlights: about 15 flops per vertex per light.
Waveforms are evaluated once per batch, but the result is written per vertex.

STATE CHANGES AND DRAW CALLS. Several state helpers already skip redundant calls: GL_State diffs the state bits (tr_backend.cpp:193-380), and GL_Bind (28-47), GL_TexEnv (157-185) and GL_Cull (111-152) cache their last value. Even so, every batch re-issues the following, without caching:
- client-state enable/disable and gl*Pointer calls (tr_shade.cpp:1251-1285);
- polygon offset toggles (1239-1243, 1319-1322);
- lock/unlock;
- per multitexture stage, 2x GL_SelectTexture (each is glActiveTexture plus glClientActiveTexture) and glEnable/glDisable(GL_TEXTURE_2D) on unit 1 (370-399);
- per entity change, glLoadMatrixf and possibly glDepthRange (tr_backend.cpp:651-673).
There are no draw-call or state-change counters. r_speeds (a CHEAT cvar, tr_init.cpp:1178) prints only shaders, surfaces, vertices and triangles (tr_cmds.cpp:21-26; the "dc" figure is depth complexity), and backend time is measured in whole milliseconds (tr_backend.cpp:1400, 1435). My estimate, unmeasured, for a typical FFA scene: 300-1000 glDrawElements per frame and roughly 10k GL calls, all with client arrays that the driver must copy on every draw.

STATIC WORLD IN VBOs: FEASIBLE WITHOUT GLSL. Positions, base st, lightmap st (already atlas-packed at load, tr_bsp.cpp:296+) and non-vertex-lit vertex colors (ComputeFinalVertexColor returns white plus the vertex alpha) are static for faces, triangle soups and patches stored at fixed subdivision. Brush models can also use their model-space data under the existing per-entity matrix.
Everything else can be decided per batch at draw time, on the remapped shader, so R_RemapShader and mvremap keep working:
- tcMod scroll, scale, rotate, stretch and transform are affine, so the fixed-function GL_TEXTURE matrix can apply them;
- tcGen vector maps to glTexGen OBJECT_LINEAR;
- rgbGen/alphaGen const, wave, entity and CGEN_LIGHTMAPn style colors become a single glColor;
- CGEN_VERTEX x identityLight can be precomputed, since overbright is latched.
These must fall back to the existing tess path:
- deforms;
- tcMod turb and tcGen environment;
- rgbGen lightingDiffuse, alphaGen specular/portal and disintegrate effects;
- LIGHTMAP_BY_VERTEX surfaces whose colors depend on animated styleColors (tr_surface.cpp:260-289);
- shaders with surface-sprite stages, which read tess (tr_surfacesprites.cpp:1397);
- dlit batches (dlighted is already in the sort key, so these are separate batches anyway);
- fogged batches (fog texcoords are view-dependent and clipped, tr_shade_calc.cpp:868-899), unless done with texgen or ARB vp.
Patch LOD (r_lodCurveError) would no longer apply to VBO-resident patches.

RENDER THREAD. SMP has been fully removed, with vestiges left:
- R_IssueRenderCommands calls RB_ExecuteRenderCommands synchronously inside RE_SwapBuffers (tr_cmds.cpp:73-95, 466-506);
- a single backEndData is allocated (tr_init.cpp:1306-1309);
- the r_smp/r_showSmp pointers are declared but never registered (tr_init.cpp:51-52);
- glConfig.smpActive is never set (it is read at tr_init.cpp:1039 and tr_main.cpp:847);
- dlightBits[SMP_FRAMES] is always indexed with frame 0 (tr_local.h:22, tr_world.cpp:163);
- renderThreadActive is unused (tr_cmds.cpp:5).
On one thread, the order is: VM (cgame), then frontend, then the whole backend at the end of the frame.

VSYNC, PACING, LATENCY.
- r_swapInterval defaults to 0 (sdl_window.cpp:1006). It is applied at context creation (866) and on change in WIN_Present (324-337). SDL accepts -1 (adaptive vsync), but this is undocumented.
- The context is a legacy/compat one: windowDesc.gl.majorVersion is left at 0 (tr_init.cpp:644; sdl_window.cpp:768-800).
- com_maxfps defaults to 125 (sys_main.cpp:94). Com_Frame computes an integer minMsec = 1000/maxfps with a bias carry (common.cpp:2872-2898), so 144 or 165 cannot be hit exactly. It then sleeps with NET_Sleep and busy-waits the last millisecond (2902-2909). There is no microsecond timer anywhere.
- Input is sampled right after the sleep (IN_Frame common.cpp:2912), then the event loop (2915), SV_Frame (2934), the event loop again, and CL_Frame (2976). Input-to-render ordering on the CPU is good. With vsync off, there is no limit on driver queue depth.
- r_finish 1 forces a glFinish before the 3D view (tr_backend.cpp:430-436).
- r_DynamicGlow 1 forces an unconditional glFinish in the middle of the frame (tr_backend.cpp:1059).
- 2D-only frames (menus) glFinish at swap because finishCalled is never set (tr_backend.cpp:1268-1270).
- Postprocess gamma is the default (r_gammamethod 2, sdl_window.cpp:1019). Every frame it does a full-screen glCopyTexImage2D, which re-specifies the texture each time (tr_backend.cpp:1326), plus a 64^3 3D-LUT pass (1305-1357; LUT built at tr_image.cpp:2974-2994).
- Dynamic glow costs 3 full-resolution and 5 blur-resolution framebuffer copies per frame (tr_backend.cpp:1046, 1065, 1083, 1614, 1633).
- glGetError is not called per frame by default (r_ignoreGLErrors 1, tr_init.cpp:1119; tr_cmds.cpp:408-415).

**Already exists (do not reinvent):**
- Lightmap atlas: 128x128 lightmaps packed into 2048x2048 atlases with UV repacking at load (tr_bsp.cpp:168-283, 296+). No separate r_mergeLightmaps work is needed.
- SSE2 Ghoul2 CPU skinning (tr_ghoul2.cpp:2199-2260)
- Radix sort of draw surfaces (tr_main.cpp:1058) and the 32-bit sort key (tr_local.h:948-950)
- Post-processing gamma/overbright through a 64^3 3D LUT with ARB vertex/fragment programs, on by default with r_gammamethod 2 (tr_backend.cpp:1305-1357, tr_image.cpp:2914-2994, sdl_window.cpp:1019, 1066-1079). It is a ready hook for color grading.
- Dynamic glow: r_DynamicGlow, r_DynamicGlowPasses/Delta/Intensity/Soft/Width/Height, r_saberGlow (tr_init.cpp:1075-1081, 1233; tr_backend.cpp:1040-1095, 1496-1806)
- Fixed-function fast paths RB_StageIteratorVertexLitTexture and RB_StageIteratorLightmappedMultitexture (tr_shade.cpp:1345-1557). They are disabled by default via r_ignoreFastPath 1.
- Compiled vertex arrays (r_ext_compiled_vertex_array), ARB_multitexture (r_ext_multitexture), texenv add, anisotropic filtering (r_ext_texture_filter_anisotropic, default 2), S3TC (r_ext_compress_textures/lightmaps), r_textureLODBias, r_openglMipMaps, r_textureMode (default GL_LINEAR_MIPMAP_NEAREST), r_picmip (default 1)
- MSAA on the default framebuffer: r_ext_multisample (sdl_window.cpp:691, 764-765, 1017)
- Vsync r_swapInterval (SDL; -1 = adaptive where supported), r_displayRefresh, r_highdpi, r_finish, r_fullscreen/r_noborder/r_mode
- com_maxfps (125), com_maxfpsUnfocused, com_maxfpsMinimized, com_busyWait, timedemo (prints average fps, cl_main.cpp:479-484), com_speeds
- Perf and debug cvars: r_speeds (cheat), r_measureOverdraw, r_showtris/r_shownormals, r_debugSort, r_nobind, r_skipBackEnd. r_logFile exists, but GLimp_LogComment is an empty macro (tr_local.h:1491).
- Quality and perf toggles: r_fastsky, r_dynamiclight, r_newDLights, r_dlightBacks, r_environmentMapping, r_surfaceSprites, r_vertexLight, r_lodbias/r_lodscale/r_lodCurveError/r_subdivisions, r_maxpolys/r_maxpolyverts, r_primitives
- SDL window plumbing that can already request a GL major/minor version, a core/compat profile and a debug context (windowDesc_t, sdl_window.cpp:768-800)
- glext.h already declares the VBO, FBO, ARB_sync, timer-query and GLSL typedefs (glext.h:530, 658, 1171, 1478, 1549, 4519)

### RB-1 — Measure first: non-cheat perf overlay, GPU timer queries, draw-call/state counters, frametime log
*tooling · effort S · client-only-opt-in*

Add counters in R_DrawElements (draws, indexes), GL_Bind (real binds), GL_State (real changes), gl*Pointer and client-state toggles, and RB_EndSurface flush reasons (shader change vs RB_CheckOverflow overflow). Add sub-millisecond CPU timing of the frontend, backend tessellation, backend GL submission and swap wait. Add GPU backend time via GL_ARB_timer_query (glQueryCounter, 2-3 frames of latency, optional). Expose them as a non-cheat overlay (e.g. r_perfOverlay) and as a CSV log during timedemo, with min, 1% low and 99th-percentile frametime. Today r_speeds is CVAR_CHEAT and prints only vertex and triangle counts at millisecond resolution.

- **Player-visible effect:** An optional on-screen frametime graph and counters, available on any server. Benchmarks become reproducible with a demo plus timedemo.
- **Expected gain:** No fps gain by itself. It is the prerequisite to validate RB-3 to RB-11, since nothing can be measured in this environment. High confidence it is needed.
- **Risk:** Very low. Timer queries are optional (guard with an extension check). Counter increments are negligible.
- **Evidence:** `src/renderer/tr_init.cpp:1178 (r_speeds is CVAR_CHEAT)`; `src/renderer/tr_cmds.cpp:13-63 (only shaders/surfaces/vertices/triangles; 'dc' is depth complexity)`; `src/renderer/tr_backend.cpp:1400,1435 (backend msec from ri.Milliseconds*timescale)`; `src/renderer/tr_shade.cpp:151-187 (single choke point for draws)`; `src/client/cl_main.cpp:479-484 (timedemo prints average fps only)`; `src/renderer/glext.h:1549 (PFNGLQUERYCOUNTERPROC already declared)`

### RB-2 — Quick wins: drop the glow-pass glFinish, use CopyTexSubImage for postprocess gamma, skip an identity LUT pass
*perf-3d · effort S · client-only-transparent*

(a) Delete the unconditional qglFinish() between the glow render and the copy in RB_DrawSurfs. GL orders glCopyTexSubImage2D after the preceding draws, so it is not needed for correctness, but it serializes the CPU against the GPU every frame when r_DynamicGlow is 1. (b) RB_GammaCorrection calls glCopyTexImage2D every frame, which re-specifies the storage. Allocate once (and on resize) and use glCopyTexSubImage2D. (c) Skip RE_GammaCorrection when the LUT is the identity (r_gamma 1 and overbrightBits 0).

- **Player-visible effect:** Higher, steadier fps with dynamic glow on. Slightly cheaper frames with default gamma. No visual change.
- **Expected gain:** (a) Restores CPU/GPU overlap. Likely +5-25% fps with r_DynamicGlow 1 when CPU and GPU times are comparable, near 0 when strongly GPU-bound. Medium confidence, from the stall structure. (b) About 0.05-0.3 ms per frame on iGPUs at 1080p-1440p (driver-dependent reallocation cost). Low-medium confidence. (c) Only applies to non-default configurations.
- **Risk:** Low. The glFinish may have been a workaround for an old ATI texture_rectangle driver (see g_bTextureRectangleHack). Keep a cvar to restore it if a regression is reported.
- **Prerequisites:** RB-1 to quantify (optional)
- **Evidence:** `src/renderer/tr_backend.cpp:1054-1065 (glow pass, qglFinish at 1059)`; `src/renderer/tr_backend.cpp:1323-1327 (qglCopyTexImage2D every frame)`; `src/renderer/tr_cmds.cpp:453-457 (gamma pass queued every frame)`; `src/sdl/sdl_window.cpp:1019 (r_gammamethod default 2 = postprocessing)`; `src/renderer/tr_image.cpp:2955-2985 (LUT is identity only when g==1 and shift==0)`

### RB-3 — Larger tess buffer (renderer-local constant) and moving big stack arrays to static storage
*perf-3d · effort S · client-only-transparent*

Thanks to the lightmap atlas, world surfaces with the same texture share a shader_t and sort together. The 1000-vertex / 6000-index tess cap then splits them into many flushes, and each flush repeats the whole stage pipeline: state, lock, one draw per stage, dlight and fog passes. Introduce a renderer-local TESS_MAX_VERTEXES (e.g. 4096; indexes x6) rather than editing qcommon/qfiles.h, which is a file-format header. Keep the model-load limit checks at the old values or relax them deliberately. Move the per-call stack buffers in the dlight and fog-modulate functions (clipBits, texCoordsArray, colorArray, hitIndexes; about 37 KB today, about 150 KB at 4096) to static arrays. Resize tr_surfacesprites and tr_quicksprite arrays consistently.

- **Player-visible effect:** Fewer hitches on open maps and in big fights. No visual change.
- **Expected gain:** Fewer flushes and draw calls for large merged world batches, perhaps 10-30% fewer batches on open maps. Backend CPU -0-10% on its own; it compounds with RB-4/RB-5. Low-medium confidence, because it depends on how many batches currently hit the cap. RB-1 flush-reason counters will tell. ioquake3 and OpenJK keep 1000, partly for cache locality: tess grows from about 128 KB to about 520 KB.
- **Risk:** Medium-low. Larger working set (cache misses). Sentinel checks in RB_EndSurface read the last element. RB_ShadowTessEnd uses the half-size rule. All users of the constant must be updated together.
- **Prerequisites:** RB-1 (flush-reason counter)
- **Evidence:** `src/qcommon/qfiles.h:10-11`; `src/renderer/tr_local.h:1513-1545,1557`; `src/renderer/tr_surface.cpp:27-49,1437-1448`; `src/renderer/tr_shade.cpp:415-418,554-557,1571-1576`; `src/renderer/tr_shade_calc.cpp:751,771,789`; `src/renderer/tr_model.cpp:819-822,1322-1328,1450-1456`; `src/renderer/tr_surfacesprites.cpp:300-302`; `src/renderer/tr_quicksprite.h:23-27`; `src/renderer/tr_bsp.cpp:168-283 (atlas makes world batches merge)`

### RB-4 — Trim per-batch GL call overhead in the stage iterator
*perf-3d · effort S · client-only-transparent*

Cache client-array enables and pointers in glState and skip redundant gl*Pointer and Enable/DisableClientState calls. Keep unit 1 enabled across consecutive multitexture stages instead of glEnable/glDisable(GL_TEXTURE_2D) plus 2x GL_SelectTexture per stage. Use glDrawRangeElements(0, numVertexes-1), which is GL 1.2 core, so drivers do not scan the indexes. Use 16-bit indexes for tess draws, since the vertex count is always under 65536 (halves index traffic). Skip glLockArraysEXT for single-pass batches, where CVA brings nothing. Make R_DrawElements read r_primitives once per frame rather than per draw.

- **Player-visible effect:** Slightly higher fps in CPU/driver-bound situations (iGPUs, laptops, 240 Hz+ players). No visual change.
- **Expected gain:** Backend CPU -3-10% (low confidence; compatibility-profile drivers vary). Its main value is to make RB-5/RB-6 easier.
- **Risk:** Low-medium. Stale cached state causes hard-to-see bugs, especially around code that bypasses the GL_* wrappers: the glow blur uses qglActiveTextureARB directly, and gamma binds texture targets directly.
- **Prerequisites:** RB-1
- **Evidence:** `src/renderer/tr_shade.cpp:1251-1285 (client state toggled every batch)`; `src/renderer/tr_shade.cpp:354-400 (multitexture unit switching)`; `src/renderer/tr_shade.cpp:151-187 (glDrawElements with GL_UNSIGNED_INT client indexes)`; `src/renderer/tr_local.h:16-17`; `src/renderer/tr_backend.cpp:52-77,1551-1569,1669`

### RB-5 — Static world geometry in VBOs (fixed-function GL 1.5, no GLSL) with per-batch fallback
*perf-3d · effort L · client-only-opt-in*

At map load, upload positions, base st, atlas lightmap st (MAXLIGHTMAPS sets) and static vertex colors for faces, triangle soups, patches (at fixed subdivision) and optionally brush models into one VBO and a static IBO, recording each surface's index range. During RB_RenderDrawSurfList, a batch whose resolved (remapped) shader is 'static' collects index ranges instead of copying vertices. It then draws each stage with glMultiDrawElements (GL 1.4) from VBO offsets, so there is no 1000-vertex cap.

A static shader has no deforms, no tcMod turb, no tcGen environment, no lightingDiffuse/specular/portal and no surface sprites; is not fogged or dlit, or has those passes on the old path; and is not LIGHTMAP_BY_VERTEX with animated styles. Static stages are handled as follows:
- affine tcMods (scroll, scale, rotate, stretch, transform) through the GL_TEXTURE matrix;
- tcGen vector through glTexGen;
- constant, wave and entity colors and CGEN_LIGHTMAPn style colors through glColor;
- multi-pass by switching attribute offsets only.
All other batches keep the existing tess path. Ship behind r_vbo (default 0 until validated), with r_speeds counters showing the share of VBO vs CPU batches.

- **Player-visible effect:** Noticeably higher and steadier fps on big maps, especially on laptops/iGPUs and for high-refresh players. The classic look is unchanged, except patches stop LOD-popping (always full detail).
- **Expected gain:** Removes the per-frame CPU copy of world vertices (RB_SurfaceFace/Grid/Triangles and ComputeFinalVertexColor) and the driver's client-array copies for those batches. Estimate: backend CPU -25-50% in world-heavy scenes and +10-35% fps when CPU/driver-bound; about 0 when capped at com_maxfps 125 with headroom or when GPU-bound. Basis: code structure, plus Quake3e's r_vbo (same idea: static world in VBO, dynamic shaders on the CPU path), which is generally reported to help most on large maps, and rend2, which keeps everything in VBOs. Low-medium confidence on magnitude; measure with RB-1. Also makes Mesa glthread / NVIDIA threaded optimization effective.
- **Risk:** Medium:
- Correctness of the static-shader classification. Validate by A/B screenshot diffs over many maps.
- Shader remaps (R_RemapShader, R_RemapShaderAdvanced with lightmap/style modes, mvremap): decide at draw time on the resolved shader.
- r_vertexLight, r_lightmap and r_fullbright interactions.
- Patch LOD semantics change.
- More hunk/VRAM use (tens of MB at most for JK2 maps).
- Must load the GL 1.5 entry points through WIN_GL_GetProcAddress, because qgl.h binds only GL 1.1 and opengl32.dll exports only 1.1.
- **Prerequisites:** RB-1; RB-4 (state caching makes mixing VBO and client-array batches safe); Function-pointer loading for GL 1.5 buffer objects in GLimp_InitExtensions
- **Evidence:** `src/renderer/tr_surface.cpp:251-290,298-364,1281-1342,1382-1538 (per-frame copies and LOD)`; `src/renderer/tr_shade.cpp:991-1094 (tcGen/tcMod on CPU)`; `src/renderer/tr_shade_calc.cpp:937-1035 (tcMods: turb non-affine, the others affine)`; `src/renderer/tr_shade_calc.cpp:823-901 (fog texcoords view-dependent and clipped, so fallback)`; `src/renderer/tr_shade.cpp:1325-1338 and src/renderer/tr_surfacesprites.cpp:1397 (surface sprites read tess)`; `src/renderer/tr_shade.cpp:322-342 (remap resolution at batch start)`; `src/renderer/tr_backend.cpp:595-605 (batch keys)`; `src/renderer/tr_init.cpp:370-622 (no VBO extension loading)`; `src/renderer/qgl.h:31+ (GL 1.1 bound statically)`; `src/renderer/tr_bsp.cpp:296+ (atlas UVs already final at load)`

### RB-6 — Streaming ring-buffer VBO for the dynamic tess path (models, effects, 2D)
*perf-3d · effort M · client-only-opt-in*

Replace client arrays for the remaining CPU-built batches with one ring buffer. Use glMapBufferRange UNSYNCHRONIZED|INVALIDATE_RANGE with orphaning on wrap, or glBufferSubData as a fallback. Upload xyz once per batch, upload svars colors/texcoords per stage (or all stages up front), and draw from offsets. Indexes go into a matching ring IBO.

- **Player-visible effect:** Smoother frames with many players and effects on drivers that handle client arrays poorly. No visual change.
- **Expected gain:** 0-15% backend CPU; highly driver-dependent. Low confidence. Its main value is unblocking glthread and keeping a future core-profile/GLSL renderer option open.
- **Risk:** Medium. A naive glBufferSubData can stall and regress. Needs per-driver testing (NVIDIA, AMD, Mesa, Intel Windows, macOS legacy GL 2.1).
- **Prerequisites:** RB-1; RB-4
- **Evidence:** `src/renderer/tr_shade.cpp:1159-1206 (per-stage client pointers)`; `src/renderer/tr_shade.cpp:1271-1276 (CVA as the only reuse mechanism)`; `src/renderer/tr_backend.cpp:874-942 (2D quads through tess)`

### RB-7 — FBO scene target: no more full-screen copies, plus the foundation for MSAA, render scale, high precision, bloom and grading
*perf-3d · effort M · client-only-opt-in*

Render the 3D view (and optionally 2D) into an FBO when EXT/ARB_framebuffer_object is available. Postprocess gamma then samples the FBO texture directly instead of glCopyTexImage2D. Dynamic glow renders into its own FBO and blur ping-pong FBOs instead of 3 full-resolution and 5 blur-resolution copies. Opt-in extras on the same plumbing:
- MSAA renderbuffer with resolve (r_ext_multisample currently depends on the window pixel format);
- r_renderScale / supersampling;
- RGB10A2 or RGBA16F color, which removes the banding caused by halving with identityLight and doubling in the LUT under overbright;
- glow resolution relative to the screen instead of a fixed 320x240.
This is the base needed for bloom, color grading (RB-8) and any later post effects (comparable: Quake3e r_fbo/r_hdr/r_bloom/r_renderScale).

- **Player-visible effect:** Default: same image, a little faster. Opt-in: crisper anti-aliasing and supersampling, smoother dark gradients, and nicer saber glow at high resolutions.
- **Expected gain:** Saves 1 full-screen copy per frame (default gamma) or 3 full-res plus 5 small copies (glow). Roughly 0.1-1 ms per frame on iGPUs at 1080p-1440p, more at 4K; negligible on high-end dGPUs. Medium confidence. Its main gain is enabling the visual upgrades.
- **Risk:** Medium:
- Resize and hi-DPI handling (R_UpdateImages);
- screenshots and video capture must read from the FBO;
- r_stereo and GL_FRONT draw-buffer paths;
- MSAA resolve of the depth used by glow;
- needs a fallback to the current copy path when FBOs are missing.
- **Prerequisites:** RB-1; Function-pointer loading for FBO entry points
- **Evidence:** `src/renderer/tr_backend.cpp:1040-1095 (scene/glow/blur copies at 1046,1065,1083)`; `src/renderer/tr_backend.cpp:1575-1642 (per-blur-pass copies at 1614,1633)`; `src/renderer/tr_backend.cpp:1305-1357 (gamma copy at 1326)`; `src/renderer/tr_image.cpp:2941,2984 (identityLight halving and LUT shift, i.e. lost precision)`; `src/renderer/tr_init.cpp:1080-1081 (glow size fixed 320x240, latched)`; `src/sdl/sdl_window.cpp:691,764-765 (MSAA through the window pixel format only)`

### RB-8 — Color-grading presets on top of the existing 3D-LUT gamma pass
*visual-wow · effort S · client-only-opt-in*

The postprocess gamma pass already samples a 64^3 3D LUT for every pixel every frame. Add an opt-in r_colorGrade (or r_lut) that loads a LUT image (e.g. gfx/lut/<name>.png laid out 512x512 or 4096x64). R_SetColorMappings would compose it with gamma and overbright, then upload it to tr.gammaLUTImage. Ship a few tasteful presets (e.g. 'cinematic', 'warm Bespin', 'cold Hoth', 'vivid'). The default empty value keeps today's identity-plus-gamma LUT bit-exact.

- **Player-visible effect:** One cvar gives instant film-like grades. Very visible in screenshots and videos for zero fps cost.
- **Expected gain:** 0 ms extra GPU cost, since the pass already runs with r_gammamethod 2. High confidence.
- **Risk:** Low. Only works with postprocess gamma; fall back silently otherwise. Watch competitive-visibility concerns (extreme grades), possibly with a server-side cap.
- **Evidence:** `src/renderer/tr_image.cpp:2914-2994 (LUT generation)`; `src/renderer/tr_backend.cpp:1305-1357 (LUT applied every frame)`; `src/renderer/tr_cmds.cpp:453-457`; `src/sdl/sdl_window.cpp:1019,1066-1079`

### RB-9 — GPU per-pixel dynamic lights (ARB_fragment_program), opt-in r_dlightMode
*visual-wow · effort M · client-only-opt-in*

Today each dlight touching a batch runs a CPU per-vertex projection plus an index filter, then an extra blended draw with the projected dlight texture. Lighting is therefore per-vertex and coarse on big world polygons, and its cost scales with lights x vertices: saber dlights from every player add up in big duels and FFAs. Add an ARB vp/fp dlight pass (the extension is already loaded and required for glow and gamma) that computes radial attenuation per pixel. Optionally add N-dot-L from the face normal. Keep GLS_DEPTHFUNC_EQUAL and the additive/modulate modes. Keep the current CPU path as mode 0 (default). Comparable: Quake3e r_dlightMode.

- **Player-visible effect:** Saber and blaster light pools look smooth and round on floors and walls instead of blotchy per-vertex triangles, which is very visible in dark maps and duels.
- **Expected gain:** Removes about 15 flops/vertex/light of CPU work (c_dlightVertexes can reach six figures per frame with many sabers near walls, unmeasured). Backend CPU in big fights -5-20%, at a small GPU fill cost. Low-medium confidence.
- **Risk:** Medium. Different look, so it stays opt-in. ARB programs are unavailable on some exotic stacks, so fall back to mode 0. Must handle portals, mirrors and clip planes. Works best combined with RB-5, so dlight passes can reuse VBO geometry.
- **Prerequisites:** RB-1; RB-5 recommended
- **Evidence:** `src/renderer/tr_shade.cpp:409-540 (NewProjectDlightTexture)`; `src/renderer/tr_shade.cpp:549-675 (ProjectDlightTexture, CPU loops and extra draws)`; `src/renderer/tr_shade.cpp:1295-1298`; `src/cgame/tr_types.h:7 (MAX_DLIGHTS 32 bitmask)`; `src/renderer/tr_init.cpp:517-571 (ARB vp/fp already loaded)`; `src/renderer/tr_backend.cpp:1448-1490 (pixel-shader helpers exist)`

### RB-10 — Collapse 3+ shader stages into one pass using 4+ texture units
*perf-3d · effort M · client-only-opt-in*

Generalize CollapseMultitexture beyond stages 0+1. When the blend chain is modulate/add with identity or constant colors, map stages onto TMUs 2-3 with GL_ARB_texture_env_combine, or onto an ARB fragment program. Typical targets: texture x lightmap + additive glow/detail, and light-styled lightmaps (each CGEN_LIGHTMAPn stage is currently an extra full pass). GL_SelectTexture must support more than 2 units.

- **Player-visible effect:** Fewer passes on detailed or styled-light maps, so somewhat higher fps there.
- **Expected gain:** -20-40% draw calls and blend bandwidth for shaders with 3+ stages; maybe 3-12% overall depending on the map. Low confidence; measure the stage histogram with RB-1 first.
- **Risk:** Medium. Single-pass combiners do not round to 8 bits between stages like multi-pass framebuffer blending does, so results can differ by about 1 LSB. Keep it opt-in, or ship only the combinations proven bit-exact. Interacts with r_lightmap and r_DynamicGlow stage filtering.
- **Prerequisites:** RB-1
- **Evidence:** `src/renderer/tr_shader.cpp:2683-2830 (collapse table and logic)`; `src/renderer/tr_shader.cpp:3329 (single collapse attempt)`; `src/renderer/tr_backend.cpp:52-77 (units 0/1 only)`; `src/renderer/tr_shade.cpp:856-870 (styled lightmap stages)`; `src/renderer/tr_init.cpp:435 (maxActiveTextures queried but unused for collapsing)`

### RB-11 — Frame pacing and input latency: bounded GPU queue via ARB_sync, documented adaptive vsync, optional precise limiter
*perf-3d · effort S · client-only-opt-in*

(a) Add r_maxFrameLatency (0 = driver default). After SwapBuffers, insert glFenceSync. Before submitting the next frame's backend commands, glClientWaitSync on the fence from N frames ago. This caps pre-rendered frames without the full pipeline drain of r_finish 1. (b) Document r_swapInterval -1 (SDL adaptive vsync) in CVARS.rst and report the result instead of a Com_DPrintf. (c) Optionally add an opt-in sub-millisecond limiter (SDL_GetPerformanceCounter) for refresh rates that do not divide 1000 (144/165/240). The default integer-ms limiter must stay as is, because usercmd msec cadence feeds JK2's framerate-dependent movement (the 125/333 fps conventions). Also stop glFinish on 2D-only frames.

- **Player-visible effect:** Lower and more consistent input lag with vsync, or when GPU-bound. Smoother motion on 144/165 Hz monitors for players who opt in.
- **Expected gain:** Up to 1-2 frames less latency when GPU-bound or with vsync (driver-dependent; medium confidence). No throughput gain; the limiter only matters for non-divisor refresh rates.
- **Risk:** Low for (a) and (b). (c) can change movement physics behavior if it changes the msec cadence, so it must stay opt-in and clearly labeled.
- **Prerequisites:** RB-1 (frametime log) to verify
- **Evidence:** `src/sdl/sdl_window.cpp:324-337,866,1006`; `src/renderer/tr_backend.cpp:430-436 (r_finish)`; `src/renderer/tr_backend.cpp:1268-1270 (glFinish when no 3D view)`; `src/qcommon/common.cpp:2872-2909 (integer-ms limiter with bias, busy-wait last ms)`; `src/qcommon/common.cpp:2826-2837 (ms-granularity Com_TimeVal)`; `src/sys/sys_main.cpp:94 (com_maxfps 125)`; `src/renderer/glext.h:1478 (PFNGLFENCESYNCPROC)`

### RB-12 — Graphics quality presets built from existing cvars (no renderer code)
*visual-wow · effort S · client-only-opt-in*

Ship exec-able presets (e.g. cfg/mv_classic.cfg, mv_high.cfg, mv_ultra.cfg) or a menu selector. 'High' and 'ultra' would set: r_picmip 0 (default 1 halves textures), r_textureMode GL_LINEAR_MIPMAP_LINEAR (default is bilinear with mip banding), r_ext_texture_filter_anisotropic 16, r_ext_multisample 4, r_DynamicGlow 1 with tuned passes, r_subdivisions and r_lodCurveError for smoother curves, r_lodbias -1. 'Classic' restores the current defaults.

- **Player-visible effect:** Visibly sharper textures, no mip banding, anti-aliased edges and saber glow, all from one choice.
- **Expected gain:** Pure visual. GPU cost is small on modern hardware (MSAA and glow are the main costs). High confidence that it is the cheapest visible upgrade.
- **Risk:** Very low. Some cvars are latched and need vid_restart. Presets must never force non-classic defaults.
- **Evidence:** `src/renderer/tr_init.cpp:1083 (r_picmip 1)`; `src/renderer/tr_init.cpp:1126 (r_textureMode GL_LINEAR_MIPMAP_NEAREST)`; `src/renderer/tr_init.cpp:1073 (aniso 2)`; `src/renderer/tr_init.cpp:1075-1081 (glow cvars)`; `src/sdl/sdl_window.cpp:1017 (r_ext_multisample 0)`; `src/renderer/tr_init.cpp:1097,1112-1113`

### RB-13 — Reintroduce a render thread (front-end/back-end overlap): not recommended now
*perf-3d · effort XL · client-only-transparent*

The SMP path has been removed, with vestiges left (r_smp never registered, smpFrame always 0, single backEndData, synchronous R_IssueRenderCommands). Restoring it would need double-buffered backEndData and drawsurfs, and thread-safe handling of the synchronous renderer queries cgame makes mid-frame (R_LerpTag, R_MarkFragments, Ghoul2 G2API state shared with the VM, RE_StretchRaw/cinematics, screenshots), plus GL-context ownership by the render thread. ioquake3 dropped its SMP path for being fragile. Get the same benefit with less risk by enabling driver-side threading (Mesa glthread, NVIDIA threaded optimization), which pays off once RB-5/RB-6 remove client arrays.

- **Player-visible effect:** Potentially higher fps on multi-core CPUs when CPU-bound.
- **Expected gain:** Theoretical up to about 1.3-1.7x on CPU-bound frames if VM+frontend and backend were balanced. Low confidence. Historically flaky.
- **Risk:** High: races with Ghoul2 and cgame renderer queries, hard-to-reproduce crashes.
- **Prerequisites:** RB-5; RB-6; Clear ownership audit of Ghoul2 data between VM traps and the backend
- **Evidence:** `src/renderer/tr_cmds.cpp:73-111 (synchronous execution)`; `src/renderer/tr_init.cpp:51-52,1306-1309`; `src/renderer/tr_local.h:22,1872-1882`; `src/renderer/tr_main.cpp:847`; `src/renderer/tr_world.cpp:163`

**Notes.** I could not measure anything: there is no GPU and there are no assets here. Every magnitude above is an estimate from code structure and from what comparable idTech3 forks did (Quake3e: r_vbo, r_dlightMode, r_fbo/r_bloom/r_renderScale; rend2: VBO plus GLSL; ioquake3 and likely OpenJK: SMP removed). Confidence is stated per item, and RB-1 should come first.

Constraint on how much fps gains matter: most JK2 players run com_maxfps 125 (or 250/333 for movement), and JK2 movement depends on the frame msec cadence. Throughput gains therefore matter mainly for low-end/iGPU machines, 240 Hz+ players, big fights (many Ghoul2 models, sabers emitting dlights, effects), 4K, and 1%-low frametime consistency. Pacing changes must keep the integer-msec cadence by default.

Suggested order (small, upstream-friendly steps): RB-1, then RB-2, RB-8 and RB-12 (cheap and immediately visible), then RB-4 and RB-3, then RB-7 (foundation for the visual items), then RB-5 (largest perf item, behind r_vbo), then RB-9, RB-10 and RB-11.

Minor findings, outside the opportunity list:
- r_ignoreFastPath defaults to 1 (tr_init.cpp:1098), so the two fast-path iterators are effectively dead code.
- RB_ShadowFinish is called twice per view (tr_backend.cpp:699-707). It is harmless because cg_shadows 2 is forced to 1 outside developer mode (438-441).
- GLimp_LogComment is an empty macro (tr_local.h:1491), so r_logFile does nothing.
- RE_StretchRaw calls glFinish per cinematic frame (tr_backend.cpp:774).

Note for the 'more load' areas: client-side entity count is bounded by the drawsurf sort key (MAX_ENTITIES 1023 in a 10-bit field, MAX_DLIGHTS 32 as a bitmask; tr_local.h:948-950, cgame/tr_types.h:7-8). Raising either needs a sort-key repack, which is renderer-internal and does not affect the VM ABI. MAX_POLYS/MAX_POLYVERTS are already adjustable via r_maxpolys/r_maxpolyverts.

Key files:
- /home/user/jk2mv/src/renderer/tr_shade.cpp
- /home/user/jk2mv/src/renderer/tr_backend.cpp
- /home/user/jk2mv/src/renderer/tr_surface.cpp
- /home/user/jk2mv/src/renderer/tr_shade_calc.cpp
- /home/user/jk2mv/src/renderer/tr_cmds.cpp
- /home/user/jk2mv/src/renderer/tr_init.cpp
- /home/user/jk2mv/src/renderer/tr_shader.cpp
- /home/user/jk2mv/src/renderer/tr_bsp.cpp
- /home/user/jk2mv/src/renderer/tr_local.h
- /home/user/jk2mv/src/qcommon/qfiles.h
- /home/user/jk2mv/src/sdl/sdl_window.cpp
- /home/user/jk2mv/src/qcommon/common.cpp

## Rendering frontend, scene and model CPU cost (tr_main, tr_world, tr_bsp, tr_light, tr_scene, tr_ghoul2 + src/ghoul2, tr_model, tr_mesh, tr_surfacesprites, tr_WorldEffects, tr_flares, tr_image)

**Current state.** Frame flow: RE_RenderScene (tr_scene.cpp:378-517) -> R_RenderView (tr_main.cpp:1350-1387) -> R_AddWorldSurfaces (tr_world.cpp:731-760: R_MarkLeaves is cached per cluster at 672-675, then a frustum + dlight BSP walk at 441-578), R_AddPolygonSurfaces (tr_scene.cpp:80-92, no culling), R_AddEntitySurfaces (tr_main.cpp:1156-1267), then a 4-pass radix sort (tr_main.cpp:1028-1065). Everything runs on one thread. The SMP render thread is only a leftover: R_SyncRenderThread/R_IssueRenderCommands run the backend synchronously (tr_cmds.cpp:72-111), and SMP_FRAMES/renderThreadActive are unused.

World traversal, sorting and lightmaps are already cheap: PVS caching, radix sort, a 2048x2048 lightmap atlas (tr_bsp.cpp:169-285), and backend batching that merges consecutive surfaces that share a shader.

Ghoul2 is the cost that grows with what is on screen:

(1) Skeleton. Per model and per new time, G2_ConstructGhoulSkeleton / R_AddGhoulSurfaces (tr_ghoul2.cpp:1615-2135) does all of the following again:
- rebuilds the used-bone list by walking the whole surface hierarchy (1457-1535, called at 1784/2025);
- runs G2_TransformBone recursively over every used bone (451-995). Each bone costs a linear override search (472, G2_bones.cpp:167-178), 2 to 5 compressed-quaternion decodes with 4 float divisions each (778-830, matcomp.c:221-292), 12-float matrix lerps, and a scalar 3x4 multiply (411-428);
- heap-copies the bone vector for r_ghoul2animsmooth (1824-1851);
- "unsquashes" ALL bones with 2 matrix multiplies and 4 sqrt per bone (1853-1878). The guard at 1859 tests the pointer, not the element, so it never skips anything;
- calls Z_Malloc/Z_Free (calloc plus zone bookkeeping, common.cpp:931) per entity and per model (1707, 1774, 1979, 2014).

This happens once per player per client frame even for players outside the view. The reason: the mvsdk cgame calls G2API_GetBoltMatrix for every player without culling (cg_players.c:5740, 6344, 6387), which triggers a full rebuild (G2_API.cpp:857-866), and SetBoneAngles/SetBoneAnim reset the cache (G2_API.cpp:386, 435, 562, 579). The same code is compiled into the dedicated server (src/CMakeLists.txt MVMPDED*), which pays it per saber player per server frame (w_saber.c:3468).

(2) Surface walk. For visible players, RenderSurfaces (1283-1403) does per surface:
- a LOD-list walk (G2_misc.cpp:1202-1225);
- a linear override search (G2_surfaces.cpp:52-64);
- a strcmp loop over up to 256 skin surfaces (1315-1327, tr_local.h:581);
- a heap `new CRenderableSurface` per drawsurf (1342, 1354, 1363).

(3) Skinning. The backend skins on the CPU per drawsurf per pass (RB_SurfaceGhoul, 2143-2298). It uses SSE2 on x86 only and the scalar path on ARM64. The glow pass (tr_backend.cpp:1055-1057), stencil/projection shadows and portals therefore skin the same surface again.

Rough cost model, from operation counts on a ~4 GHz core. Nothing can be measured here, so treat it as +/-2x:
- skeleton: about 15-20 us per humanoid per rebuild;
- surface walk: about 5 us;
- skinning: about 10-15 us per pass at LOD0 (~2k verts);
- backend per-vertex shading and client-array upload: another 15-30 us.

That gives about 50-70 us per visible player and 15-20 us per off-screen player. At 32 players that is about 1.5-2.5 ms per frame, roughly 20-30% of the 8 ms com_maxfps 125 budget, and the bottleneck at 250+ fps.

Other hazards that get worse with load:
- Dynamic lights: first-come cap of 32, no culling (tr_scene.cpp:291, 320). At exactly 32 lights, `1U<<32` is undefined and in practice zeroes all world dlights (tr_world.cpp:752-757).
- Entities: cap of 1023 refentities, and index 1022 aliases ENTITYNUM_WORLD (tr_scene.cpp:186, q_shared.h:1331, tr_backend.cpp:613).
- Polys: no culling, a 600/3000 cap, and one warning printed per rejected poly (tr_scene.cpp:80-92, 121-124).
- Weather: rain/snow can only be enabled with the r_we console command (tr_init.cpp:1221) and is drawn in immediate mode, one particle at a time (tr_WorldEffects.cpp:1680-1716, 1975-2045).
- Surface sprites: regenerated procedurally every frame (tr_surfacesprites.cpp:494-793).
- Flares: dead code. The r_flares cvar exists but RB_RenderFlares is commented out (tr_backend.cpp:712) and RB_SurfaceFlare is empty (tr_surface.cpp:1677-1694).
- Images: decoded one at a time at load, trying jpg/png/tga twice each (tr_image.cpp:2407-2460); upload uses GL_GENERATE_MIPMAP or CPU mips (844-983).

**Already exists (do not reinvent):**
- Radix sort of drawsurfs (tr_main.cpp:1028-1065); the 32-bit sort key layout is documented at tr_local.h:935-950
- Lightmap atlasing: all 128x128 lightmaps packed into 2048x2048 atlases (tr_bsp.cpp:169-305)
- SSE2 Ghoul2 CPU skinning with a precached transposed bone palette (tr_ghoul2.cpp:2199-2266), x86/x64 only
- Per-time skeleton cache via CGhoul2Info::mSkelFrameNum (tr_ghoul2.cpp:1766, G2_API.cpp:860) plus the GetBoltMatrix_NoReconstruct / NoRecNoRot syscall variants (G2_API.cpp:776-800), already used by mvsdk (cg_players.c:1825, 2447; w_saber.c:203, 294)
- PVS mark caching per view cluster (tr_world.cpp:672-675) and r_lockpvs/r_novis
- cl_autolodscale (default 1): lowers LOD scale from the total client count (cl_cgame.cpp:328-340, cl_main.cpp:2893), plus r_lodscale/r_lodbias/r_autolodscalevalue (tr_init.cpp:1112-1114, 1171)
- r_ghoul2animsmooth (.3) and r_ghoul2unsqashaftersmooth (1) (tr_init.cpp:1204-1205)
- r_openglMipMaps (GL_GENERATE_MIPMAP), r_ext_texture_filter_anisotropic, r_textureLODBias, r_picmip (default 1), r_textureMode, r_simpleMipMaps, PNG/JPG/TGA loaders, r_ext_compress_textures / r_ext_compress_lightmaps (S3TC) (tr_init.cpp:1066-1094, tr_image.cpp:733-983)
- Image and model binary caches that survive level changes (RE_RegisterImages_LevelLoadEnd tr_image.cpp:1093, CachedModels / r_modelpoolmegs tr_model.cpp:58-60, 1210)
- r_maxpolys / r_maxpolyverts cvars (tr_init.cpp:1194-1195)
- r_DynamicGlow* (ARB_fragment_program glow) and r_saberGlow (tr_init.cpp:1075-1081, 1233)
- r_newDLights projected/additive dlights (tr_init.cpp:1168, tr_light.cpp:21-60)
- r_flares, r_flareSize, r_flareFade cvars (registered, but the code path is dead)
- r_we console command for rain/snow/wind (tr_init.cpp:1221, tr_WorldEffects.cpp:2161-2252)
- r_surfaceSprites, r_surfaceWeather, r_wind* cvars (tr_init.cpp:1147-1156)
- r_speeds modes 1-7 and com_speeds (millisecond resolution) (tr_cmds.cpp:12-60, common.cpp:2990-3001); timedemo / cl_timedemo exist
- std::thread is already used in the engine (net_http.cpp:35, 254), so C++11 threading is acceptable
- r_convertModelBones / r_loadSkinsJKA JKA asset compatibility (tr_init.cpp:1197-1198, tr_ghoul2.cpp:2307-2361)

### FE-1 — Per-frame arena for Ghoul2 renderable surfaces and scratch buffers (also fixes a leak)
*perf-3d · effort S · client-only-transparent*

Replace the per-drawsurf heap `new CRenderableSurface` (tr_ghoul2.cpp:1342, 1354, 1363) and the conditional `delete` in RB_SurfaceGhoul (2155-2159) with a bump arena that is reset once per frame (not per scene, so the glow pass and portal views still find their surfaces). JKA/OpenJK use the same pattern with their AllocRS pool. Replace the Z_Malloc/Z_Free of modelList and boneUsedList (1707, 1774, 1979, 2014) with stack or thread_local scratch arrays, and the `oldBones` std::vector copy used for smoothing (1824-1828, 2066-2070) with a reusable buffer. Results are bit-exact. This also fixes real leaks. Today a surface is never freed when its shader hasGlow, r_DynamicGlow is 1 and the scene is RDF_NOWORLDMODEL, because the glow pass is skipped there (tr_backend.cpp:1040). Examples are UI/HUD Ghoul2 previews. Surfaces also leak when R_AddDrawSurfCmd drops the command (tr_cmds.cpp:162-165) or when the drawsurf ring wraps (tr_main.cpp:1078-1086). This change is required before any threading, because the zone allocator is global and not thread-safe.

- **Player-visible effect:** No visual change. Frame times are slightly steadier in crowded scenes, and memory no longer grows in menus that show glowing Ghoul2 models.
- **Expected gain:** About 1-3 us per visible player per frame: 3-4 zone allocations, about 25-50 new/delete pairs at roughly 50-100 cycles each, and a 3.7 KB vector copy. That is about 5-10% of Ghoul2 frontend time. Moderate confidence on the direction, low on the size. The leak fix is certain from the code.
- **Risk:** Low. The arena must outlive every view of the frame, including the glow pass and mirrors. Size it from MAX_DRAWSURFS and fall back to skipping the surface on overflow.
- **Evidence:** `src/renderer/tr_ghoul2.cpp:1342`; `src/renderer/tr_ghoul2.cpp:1354`; `src/renderer/tr_ghoul2.cpp:1363`; `src/renderer/tr_ghoul2.cpp:2155-2159`; `src/renderer/tr_backend.cpp:1040`; `src/renderer/tr_cmds.cpp:162-165`; `src/renderer/tr_main.cpp:1078-1086`; `src/renderer/tr_ghoul2.cpp:1707`; `src/renderer/tr_ghoul2.cpp:1774`; `src/renderer/tr_ghoul2.cpp:1824-1828`; `src/qcommon/common.cpp:931-950`

### FE-2 — Fix the 32-dlight shift UB that switches off all world dynamic lighting
*visual-wow · effort S · client-only-transparent*

tr_world.cpp:752-757 computes `(1U << num_dlights) - 1` and only takes the full-mask branch when num_dlights > MAX_DLIGHTS. RE_AddLightToScene and RE_AddDynamicLightToScene cap the count at MAX_DLIGHTS (tr_scene.cpp:291, 320), so the branch can never run, and exactly 32 lights hits the undefined shift. On x86/x64 and ARM the shift count is masked, so the mask becomes 0 and no world surface gets dlit that frame. Bmodels and entities are unaffected. In a 32-player saber fight this is easy to reach: each saber blade adds a dlight every frame (cg_players.c:3933), plus missiles (cg_ents.c:1893, 1919) and muzzle flashes (cg_weapons.c:725). Fix with `>=` and a full 0xFFFFFFFF mask. Ship it as a standalone bug-fix commit.

- **Player-visible effect:** In big fights, light from sabers and blasters on walls and floors stops flickering off.
- **Expected gain:** Correctness only, no performance change. High confidence that the UB exists. The 'mask becomes 0' outcome is standard hardware behaviour, but compilers are free to do worse.
- **Risk:** Very low. It restores the intended look and adds no new look.
- **Evidence:** `src/renderer/tr_world.cpp:752-757`; `src/renderer/tr_scene.cpp:291-292`; `src/renderer/tr_scene.cpp:320-321`; `src/cgame/tr_types.h:7`; `src/mvsdk/code/cgame/cg_players.c:3933`; `src/mvsdk/code/cgame/cg_ents.c:1893`; `src/mvsdk/code/cgame/cg_weapons.c:725`

### FE-3 — Cache per-model and per-instance lookups in the Ghoul2 hot path (bit-exact)
*perf-3d · effort M · client-only-transparent*

Five independent, bit-exact caches. (a) Used-bone list: build it once per (model, surface-override state, root surface) and reuse it, instead of walking the whole surface hierarchy on every rebuild (tr_ghoul2.cpp:1457-1535, 1784, 2025). Invalidate it with a version counter bumped by the G2API surface on/off, root and add/remove-surface calls. (b) Skin to shader table: resolve the shader index per (skin, model) once, instead of a strcmp loop per surface per frame (1315-1327; tr_mesh.cpp has the same pattern for MD3). (c) LOD tables: a per-model array of surface pointers per LOD, filled at load, to replace the LOD walk in G2_FindSurface (G2_misc.cpp:1202-1225). (d) Override index table: map bone number to override index once per skeleton build, replacing the per-bone linear G2_Find_Bone_In_List (tr_ghoul2.cpp:472, G2_bones.cpp:167-178) and per-surface G2_FindOverrideSurface (G2_surfaces.cpp:52-64). (e) Name and handle caches: keep the resolved qhandle and bone-name-to-index on CGhoul2Info, instead of RE_RegisterModel(fileName) string hashing on every rebuild or API call (tr_ghoul2.cpp:2005, G2_bones.cpp:470, G2_misc.cpp:497) and Q_stricmp in G2_Find_Bone for each SetBoneAngles (G2_bones.cpp:34-62, about 6 calls per player per frame from cg_players.c:2952-2962). VMs only hold opaque g2handle_t (sv_game_syscalls.h:602-683, cl_cgame.cpp:1168-1197), so new engine-side fields do not touch the mod ABI. The dedicated server benefits from (a), (d) and (e).

- **Player-visible effect:** None visually. Higher and steadier fps when many players are on screen, and lower Ghoul2 CPU on servers.
- **Expected gain:** Removes about 7-8 us of the roughly 20 us skeleton plus surface walk per player per frame: used-bone walk about 4 us, skin strcmp about 2-3 us, other lookups about 1 us. That is about 35-40% of Ghoul2 frontend CPU, or about 0.25 ms per frame with 32 visible players. Server-side Ghoul2 time drops about 25-30%. Basis: operation counts. Low-moderate confidence, +/-2x.
- **Risk:** Stale-cache bugs on surface on/off (dismemberment), skin change, model reload or vid_restart. Mitigate with explicit version counters and the FE-16 golden tests. Do not 'fix' the dead `if (!boneUsedList)` guard (tr_ghoul2.cpp:1859, 2102) along the way: unsquashing unused bones changes stale matrices and therefore bolts on bones not used by any surface.
- **Prerequisites:** FE-16 (golden tests recommended)
- **Evidence:** `src/renderer/tr_ghoul2.cpp:1457-1535`; `src/renderer/tr_ghoul2.cpp:1784`; `src/renderer/tr_ghoul2.cpp:1315-1327`; `src/renderer/tr_local.h:581`; `src/ghoul2/G2_misc.cpp:1202-1225`; `src/ghoul2/G2_bones.cpp:167-178`; `src/ghoul2/G2_surfaces.cpp:52-64`; `src/renderer/tr_ghoul2.cpp:2005`; `src/ghoul2/G2_bones.cpp:34-62`; `src/ghoul2/G2_bones.cpp:470`; `src/ghoul2/G2_API.cpp:42-50`; `src/mvsdk/code/cgame/cg_players.c:2952-2962`

### FE-4 — SIMD for Ghoul2 bone math, plus a NEON port of skinning
*perf-3d · effort M · client-only-transparent*

Vectorize Multiply_3x4Matrix (tr_ghoul2.cpp:411-428). Each output row is in2[i][0]*row0 + in2[i][1]*row1 + in2[i][2]*row2 + (0,0,0,in2[i][3]), which is 9 SSE ops instead of 63 scalar flops. Also vectorize the 12-float frame/blend/smooth lerps (781-785, 807-811, 832-847, 1843-1846) and the unsquash loop (1853-1878). Keep the scalar association order and compile these files with -ffp-contract=off so results stay bit-exact. That matters because the server uses the same code for saber bolts. Add a NEON path, or sse2neon, for the SSE2 skinning block (2199-2266): it is compiled only for id386/idx64 (q_shared.h:92-95), so ARM64 builds, which CMake supports (CMakeLists.txt:118-123), use the scalar loop (2267-2292). Optionally decompress the GLA bone pool at load using the same MC_UnCompressQuat, which is still bit-exact, but only after measuring the cache-footprint trade-off.

- **Player-visible effect:** None visually. More fps in crowded scenes, and a large skinning speed-up on ARM (Apple Silicon, Raspberry Pi, ARM Linux handhelds).
- **Expected gain:** Bone transform plus unsquash (about 9-10 us per player per rebuild) drops about 40-50%. About 0.15 ms per frame at 32 players on the client, plus the same relative saving on the server. ARM64 skinning runs about 2-3x faster. Basis: op counts and typical SSE vs scalar ratios. Low-moderate confidence.
- **Risk:** Silent loss of bit-exactness if the association changes or FMA contraction is enabled (GCC uses -ffp-contract=fast with -std=gnu++11; this project sets CXX_STANDARD 11 with extensions on). Verify with the FE-16 golden tests on both x86 and ARM.
- **Prerequisites:** FE-16
- **Evidence:** `src/renderer/tr_ghoul2.cpp:411-428`; `src/renderer/tr_ghoul2.cpp:781-785`; `src/renderer/tr_ghoul2.cpp:832-847`; `src/renderer/tr_ghoul2.cpp:1853-1878`; `src/renderer/matcomp.c:221-292`; `src/qcommon/q_shared.h:92-95`; `src/renderer/tr_ghoul2.cpp:2199`; `src/renderer/tr_ghoul2.cpp:2267-2292`; `CMakeLists.txt:118-123`; `src/CMakeLists.txt:499`

### FE-5 — Skin each Ghoul2 surface once per frame and reuse it across passes
*perf-3d · effort M · client-only-transparent*

Today every drawsurf that points at the same (mdxmSurface, bone list) is skinned again from scratch. That covers the main pass, the stencil shadow and projection shadow drawsurfs (tr_ghoul2.cpp:1336-1358), the dynamic-glow re-render (tr_backend.cpp:1055-1057; surfaces are deliberately kept alive for it at tr_ghoul2.cpp:2152-2159), and portal/mirror views (tr_main.cpp:939). Store the skinned xyz and normal in a per-frame cache keyed by the arena entry (FE-1), and have RB_SurfaceGhoul memcpy from it into tess. Result is bit-exact.

- **Player-visible effect:** Higher fps for players who use cg_shadows 2 or 3, or r_dynamicGlow with glowing player or custom models.
- **Expected gain:** Skinning work drops about 50% in those configurations, about 10-15 us per player per frame. Zero gain with the defaults (cg_shadows 1, glow off). High confidence on the mechanism, moderate on the size.
- **Risk:** Low. Cache memory is about 32 B/vertex/frame, roughly 2.5 MB at 32 players. Keyed lookups must not miss when LOD differs per view.
- **Prerequisites:** FE-1
- **Evidence:** `src/renderer/tr_ghoul2.cpp:1336-1367`; `src/renderer/tr_ghoul2.cpp:2152-2159`; `src/renderer/tr_backend.cpp:1055-1057`; `src/renderer/tr_ghoul2.cpp:2143-2298`; `src/renderer/tr_main.cpp:939`

### FE-6 — Worker pool for Ghoul2 skinning (and later skeletons) in the frontend
*perf-3d · effort L · client-only-transparent*

Add a small std::thread job pool; std::thread is already used in net_http.cpp. After R_SortDrawSurfs, gather the unique Ghoul2 renderable surfaces of the frame and skin them in parallel into the FE-5 cache. RB_SurfaceGhoul then waits per surface and copies, so skinning overlaps with world rendering in the synchronous backend (tr_cmds.cpp:72-111). Parallelizing skeletons is only worthwhile once they are built in the renderer instead of inside cgame syscalls (FE-7). Prerequisites: remove the Z_Malloc calls from the hot path (FE-1); make the globals worldMatrix/worldMatrixInv (tr_ghoul2.cpp:24-25), preTransEntMatrix (tr_main.cpp:184) and the RicksCrazyOnServer context flag (G2_API.cpp:39) per-call or not touched by workers; do not call RE_RegisterModel from workers. Join all jobs before RE_EndFrame returns, so cgame never sees half-updated bone lists.

- **Player-visible effect:** Higher fps in 16-32 player fights on multi-core CPUs.
- **Expected gain:** Skinning only: about 0.3-0.4 ms per frame saved at 32 visible players on 4 cores, about 4-8% of frame time at 125-250 fps. More (about 1 ms) if combined with FE-7 skeletons and backend shading. Low confidence. Amdahl-limited, because cgame and the GL submission stay serial.
- **Risk:** Threading bugs, and jitter on 2-core machines. Keep a cvar to force single-threaded and a serial fallback path.
- **Prerequisites:** FE-1; FE-5; FE-16
- **Evidence:** `src/renderer/tr_cmds.cpp:72-111`; `src/renderer/tr_local.h:22`; `src/qcommon/net_http.cpp:35`; `src/qcommon/net_http.cpp:254`; `src/renderer/tr_ghoul2.cpp:24-25`; `src/renderer/tr_main.cpp:184`; `src/ghoul2/G2_API.cpp:39`; `src/qcommon/common.cpp:931`

### FE-7 — Lazy bolt-chain evaluation: full skeleton only for rendered models
*server-load · effort L · client-only-transparent*

G2API_GetBoltMatrix rebuilds every bone of every model in the instance and every bolt surface (G2_API.cpp:857-866, tr_ghoul2.cpp:1922-2135, including ProcessModelBoltSurfaces at 2126). That happens for every player each client frame, visible or not (cg_players.c:5740, 6344, 6387, no culling), and for every saber player each server frame (w_saber.c:3468). Instead, evaluate only the bones on the root-to-bolt chain for the requested bolt (for example the about 11 bones up to rhand for the saber, against about 72), and let R_AddGhoulSurfaces build the full skeleton only after the cull test (1648). JKA's CBoneCache uses a similar lazy per-bone design. Per-bone results along a chain are identical to the full recursion. The catch is smoothing: it keeps per-bone state (mTempBoneList[].first) and on the server is keyed on tr.refdef.time (2081), so bones skipped for some frames would later smooth toward a stale pose. Gate it with a cvar: on for client-only Ghoul2 instances, off on the server until golden tests show bit-exact results for the queried bolts (including g_saberGhoul2Collision, which needs the full skeleton).

- **Player-visible effect:** No visual change. Lower CPU with many players in the snapshot but off-screen, and lower server CPU per saber player.
- **Expected gain:** About 70-80% less skeleton work per GetBoltMatrix on a cold skeleton (about 15 us down to 3-4 us). Server: 32 players at sv_fps 40 is about 1280 calls/s, so roughly 15 ms CPU saved per second (about 1.5% of a core, more at higher sv_fps). Client: about 12 us per off-screen player per frame. Moderate confidence on the relative gain, low on the absolute. A modest win for L effort.
- **Risk:** Medium. Smoothing and rebuild-count quirks are hard to reproduce bit-exactly. Any server-side difference changes saber bolt positions and therefore gameplay. Ship default-off on the server.
- **Prerequisites:** FE-3; FE-16
- **Evidence:** `src/ghoul2/G2_API.cpp:857-866`; `src/ghoul2/G2_API.cpp:386`; `src/ghoul2/G2_API.cpp:562`; `src/renderer/tr_ghoul2.cpp:1922-2135`; `src/renderer/tr_ghoul2.cpp:2081`; `src/renderer/tr_ghoul2.cpp:1648`; `src/mvsdk/code/cgame/cg_players.c:5740`; `src/mvsdk/code/cgame/cg_players.c:6344`; `src/mvsdk/code/game/w_saber.c:3468`; `src/CMakeLists.txt:107-200`

### FE-8 — GPU skinning for Ghoul2 with static per-surface vertex buffers
*perf-3d · effort XL · client-only-transparent*

Upload each mdxmSurface once: positions, normals, texcoords, up to 4 bone indices and weights pre-decoded from uiNmWeightsAndBoneIndexes (mdx_format.h:283-315). Skin in a vertex program with a per-surface palette. A surface references at most 32 bones (mdx_format.h:61-62, assert at tr_ghoul2.cpp:2204), so 32 x 3 vec4 = 96 parameters, which fits even the ARB_vertex_program minimum; GLSL 1.20 also works. Entity lighting (RB_CalcDiffuseColor) and environment texcoord generation have to move into the same program, so this depends on the backend VBO/shader modernization proposed for tr_shade. Keep the CPU path as fallback, and for FE-5 consumers such as stencil shadows.

- **Player-visible effect:** Large fps gains in crowded saber fights. The look stays the same: identical math up to float rounding.
- **Expected gain:** Removes CPU skinning (about 10-15 us per player per pass), per-vertex diffuse lighting and the client-array upload (about 15-30 us). That is about 50-70% of the per-visible-player cost, or about 1-1.5 ms per frame at 32 players. Basis: the cost model in current_state, plus OpenJK rend2's GPU-skinned Ghoul2. Low-moderate confidence.
- **Risk:** High. Driver variance; per-stage deforms (deformVertexes, tcGen environment) need shader equivalents or a CPU fallback; and pixel-level equality against the classic look has to be validated.
- **Prerequisites:** Backend VBO + GLSL/ARB_vp path (other area); FE-1; FE-16
- **Evidence:** `src/renderer/mdx_format.h:61-62`; `src/renderer/mdx_format.h:283-315`; `src/renderer/tr_ghoul2.cpp:2204`; `src/renderer/tr_ghoul2.cpp:2199-2298`; `src/renderer/tr_shade_calc.cpp:1104-1152`

### FE-9 — Dynamic-light staging, culling and priority (keep the 32 that matter)
*visual-wow · effort M · client-only-opt-in*

Lights are accepted first come, first served, up to 32 (tr_scene.cpp:285-325), with no frustum or PVS test. Off-screen sabers and missiles use up slots while lights in view get dropped. Stage up to about 256 requests per scene. At RE_RenderScene, cull them against the frustum and PVS (R_inPVS exists at tr_world.cpp:630), then keep the 32 with the highest screen contribution (radius / distance). The 32-bit surface dlightBits stay unchanged (tr_world.cpp:142-193, tr_types.h:7). Fewer useless lights also means less per-surface dlight projection in the backend and fewer per-entity loops (tr_light.cpp:518-531).

- **Player-visible effect:** In 32-player fights every saber and blaster bolt you can see lights its surroundings, instead of lights blinking out at random.
- **Expected gain:** A visible quality gain under load. Small CPU win from projecting fewer off-screen lights. Moderate confidence.
- **Risk:** It changes which lights render when there are more than 32, which is a look change, so default off behind a cvar (for example r_dlightPriority). Lights must still be handled per scene with r_firstSceneDlight.
- **Prerequisites:** FE-2
- **Evidence:** `src/renderer/tr_scene.cpp:285-325`; `src/renderer/tr_world.cpp:630-642`; `src/renderer/tr_world.cpp:142-193`; `src/renderer/tr_light.cpp:518-531`; `src/cgame/tr_types.h:7`; `src/mvsdk/code/cgame/cg_players.c:3933`

### FE-10 — Lift scene limits for effect-heavy fights: entity index fix, wider sort key, poly culling
*perf-3d · effort M · client-only-transparent*

Three fixes. (a) With 1023 refentities, index 1022 equals ENTITYNUM_WORLD (q_shared.h:1331), so the backend draws that entity with the world transform (tr_backend.cpp:613). ioquake3 fixed this with a dedicated REFENTITYNUM_WORLD; cap user entities below the world index. (b) The sort key packs 14 bits of shader, 10 of entity, 5 of fog and 2 of dlight into 32 bits (tr_local.h:935-950, tr_main.cpp:1083-1099); MAX_ENTITIES 1023 sits in tr_types.h:8 with the note 'can't be increased'. Move to a 64-bit key, sorting only the significant bytes, to allow about 4096 refentities. This is engine-internal, since cgame never reads the key. (c) R_AddPolygonSurfaces adds every poly without culling (tr_scene.cpp:80-92), and RE_AddPolyToScene prints one warning per rejected poly (121-124). Add a cheap bounds-vs-frustum reject using the bounds already computed for fog (145-150), and rate-limit the warning.

- **Player-visible effect:** No missing or misplaced effects in huge FFA/CTF brawls, and no console spam when limits are hit.
- **Expected gain:** Mostly robustness under load. The poly cull saves backend tessellation for off-screen marks and effects; the gain scales with r_maxpolys. The extra sort pass costs about 10-20 us per 5k drawsurfs. Moderate confidence.
- **Risk:** Low-medium. Every R_DecomposeSort user (backend, shadows, portals) has to follow the new key, and the radix sort scratch doubles.
- **Evidence:** `src/renderer/tr_scene.cpp:186`; `src/cgame/tr_types.h:8`; `src/qcommon/q_shared.h:1331`; `src/renderer/tr_backend.cpp:613`; `src/renderer/tr_local.h:935-950`; `src/renderer/tr_main.cpp:1083-1099`; `src/renderer/tr_scene.cpp:80-92`; `src/renderer/tr_scene.cpp:121-124`; `src/renderer/tr_local.h:1866-1867`

### FE-11 — Opt-in quaternion interpolation for Ghoul2 animation (smoother limbs, fewer flops)
*visual-wow · effort M · client-only-opt-in*

Bones are stored as compressed quaternions plus translation (mdx_format.h:119, matcomp.c:221-292). The engine decodes each frame to a 3x4 matrix and lerps the matrices (tr_ghoul2.cpp:778-786, 829-836). Lerping rotation matrices shrinks and skews limbs between 20 Hz keyframes, which is why the r_ghoul2unsqashaftersmooth workaround exists (1853-1878). With r_g2QuatLerp 1: decode only the quaternions, nlerp them (with the sign fix), and build one matrix per bone instead of two to three, keeping the translation lerp. Apply it to client rendering only; server and gameplay bolts keep the classic math.

- **Player-visible effect:** Visibly cleaner fast saber swings and acrobatics at high fps, and in slow-motion/timescale demo moviemaking: no shrinking elbows or knees between keyframes.
- **Expected gain:** About 20-30% less per-bone decode and lerp work for interpolated bones, plus higher animation quality. Moderate confidence on quality, low on perf.
- **Risk:** Changes the look (opt-in, default 0). The blend path (blendFrame/blendOldFrame) has three sources and needs care. Client-side bolts (saber trails, effects) move slightly when it is enabled.
- **Prerequisites:** FE-16
- **Evidence:** `src/renderer/mdx_format.h:119`; `src/renderer/matcomp.c:221-292`; `src/renderer/tr_ghoul2.cpp:778-786`; `src/renderer/tr_ghoul2.cpp:829-836`; `src/renderer/tr_ghoul2.cpp:1853-1878`; `src/renderer/tr_init.cpp:1204-1205`

### FE-12 — Bring back flares with non-stalling occlusion queries (map lights and dlights)
*visual-wow · effort M · client-only-opt-in*

r_flares is registered (tr_init.cpp:1116, default 0), but the feature is dead. RB_RenderFlares is commented out (tr_backend.cpp:712), RB_AddDlightFlares is never called (tr_flares.cpp:364), RB_SurfaceFlare is empty (tr_surface.cpp:1677-1694), yet map flare surfaces are still parsed (tr_bsp.cpp:627-651). The old test used a per-flare glReadPixels of depth (tr_flares.cpp:235), which stalls the GPU every time. Re-enable it with GL_ARB_occlusion_query (core in GL 1.5, fits the fixed-function pipeline), reading results one frame late, and add opt-in dlight flares so sabers, blaster bolts and explosions get a soft glare. Keep the fade logic (tr_flares.cpp:240-263).

- **Player-visible effect:** Optional cinematic glare on saber blades, blaster bolts, explosions and bright map lights.
- **Expected gain:** A visual feature with negligible cost: no CPU stalls, a few draws. Moderate confidence.
- **Risk:** Taste: flares can look cheesy, so keep them off by default and tune intensity. Interaction with portals and mirrors is noted in tr_flares.cpp:340-353.
- **Prerequisites:** FE-9 (for dlight flares)
- **Evidence:** `src/renderer/tr_init.cpp:1116`; `src/renderer/tr_backend.cpp:712`; `src/renderer/tr_flares.cpp:235`; `src/renderer/tr_flares.cpp:364`; `src/renderer/tr_surface.cpp:1677-1694`; `src/renderer/tr_bsp.cpp:627-651`

### FE-13 — Modernize the dormant weather system and allow per-map opt-in weather
*visual-wow · effort M · client-only-opt-in*

In JK2 MP, rain and snow can only be enabled with the r_we console command (tr_init.cpp:1221, tr_WorldEffects.cpp:2161-2252). Rain is a 20-unit camera-local cylinder (mSpread around tr_WorldEffects.cpp:1748-1750) drawn in immediate mode with 7 GL calls per drop (1975-2045). Snow uses qglVertex3fv per flake under GL_POINTS (1680-1716). Both call flrand per particle per frame (1954-1970, 1593-1660) and are frame-rate dependent. Switch to client vertex arrays (as SQuickSprite already does), run world-space particles with a larger spread, light them from the lightgrid (R_LightForPoint at tr_light.cpp:562), optionally add splashes, and add an opt-in cvar (for example r_weather 1) that runs a client-side maps/<map>.weather script of r_we commands at map load. JKA's tr_WorldEffects (OpenJK, GPL) is a close and much richer relative to port from.

- **Player-visible effect:** Rain and snow on outdoor maps (Bespin, Hoth-style custom maps), for players who enable it. Strong mood impact, no server or mod changes.
- **Expected gain:** Visual feature. Array rendering cuts driver calls about 10x, so 5-10x more particles fit in the same 0.1-0.3 ms. Moderate confidence.
- **Risk:** Particles leaking indoors without CONTENTS_OUTSIDE data (the snow code already samples CM_PointContents at 1543-1556), and overdraw cost on low-end GPUs. Strictly opt-in.
- **Evidence:** `src/renderer/tr_init.cpp:1221`; `src/renderer/tr_WorldEffects.cpp:1680-1716`; `src/renderer/tr_WorldEffects.cpp:1975-2045`; `src/renderer/tr_WorldEffects.cpp:1954-1970`; `src/renderer/tr_WorldEffects.cpp:2103-2130`; `src/renderer/tr_quicksprite.cpp:49-115`

### FE-14 — Cache surface-sprite placement and add an opt-in density multiplier
*perf-3d · effort M · client-only-transparent*

RB_DrawVerticalSurfaceSprites, RB_DrawOrientedSurfaceSprites and RB_DrawEffectSurfaceSprites regenerate every grass or vegetation sprite from scratch every frame (tr_surfacesprites.cpp:494-793, 881-1075, 1158-1392). Each frame repeats the barycentric loops (681-690), the randomchart sampling, the sin/cos wind sway per sprite (320-366) and the alpha fade. Placement (position, size, light, random seeds) only depends on static world geometry. Cache it per surface the first time the surface is drawn and animate only wind and alpha each frame. Batches flush every 250 sprites (tr_quicksprite.cpp:154-162, SHADER_MAX_VERTEXES=1000), so a bigger batch buffer helps too. With the cost reduced, an opt-in r_surfaceSpriteDensity cvar can raise density for lusher maps.

- **Player-visible effect:** Higher fps on maps with vegetation, and optionally denser grass and foliage.
- **Expected gain:** About 50-70% less surface-sprite CPU on maps that use it (roughly 0.2-0.5 ms per frame at about 10k sprites). Zero on maps without surface sprites, which is most stock JK2 MP maps. Low-moderate confidence.
- **Risk:** Cache memory per surface, and invalidation on vid_restart or map change. The density multiplier changes the look, so it stays opt-in.
- **Evidence:** `src/renderer/tr_surfacesprites.cpp:494-793`; `src/renderer/tr_surfacesprites.cpp:681-690`; `src/renderer/tr_surfacesprites.cpp:320-366`; `src/renderer/tr_quicksprite.cpp:154-162`; `src/renderer/tr_init.cpp:1147`

### FE-15 — 'Showcase' quality preset built only from existing cvars
*visual-wow · effort S · client-only-opt-in*

The classic defaults are deliberately low by modern standards: r_picmip 1 (tr_init.cpp:1083), r_textureMode GL_LINEAR_MIPMAP_NEAREST with visible mip banding (1126), anisotropy 2 (1073), r_subdivisions 4 for curves (1097), r_lodscale 5 (1171), cl_autolodscale 1 which lowers model LOD once 8+ clients are connected (cl_cgame.cpp:328-340), and r_DynamicGlow 0 (1075). Add an exec-able preset (for example a `preset showcase` command or a shipped cfg) that sets picmip 0, trilinear filtering, aniso 16, lower r_subdivisions, a higher r_lodCurveError, cl_autolodscale 0, glow on, and MSAA where available. 'classic' stays the default.

- **Player-visible effect:** One command makes textures sharp at distance, curves round, distant player models detailed, and saber glow on.
- **Expected gain:** Large visual gain for zero code risk. Costs GPU time, plus the CPU skinning of LOD0 models, which FE-3/4/5 offset. High confidence on visibility.
- **Risk:** Some latched cvars need vid_restart. Coordinate with the backend and visual areas so there is a single preset system.
- **Evidence:** `src/renderer/tr_init.cpp:1083`; `src/renderer/tr_init.cpp:1126`; `src/renderer/tr_init.cpp:1073`; `src/renderer/tr_init.cpp:1097`; `src/renderer/tr_init.cpp:1171`; `src/client/cl_cgame.cpp:328-340`; `src/client/cl_main.cpp:2893`

### FE-16 — Measurement first: microsecond phase profiler, plus synthetic Ghoul2 golden and benchmark tests
*tooling · effort M · client-only-transparent*

Frontend and backend timing uses millisecond counters only (tr_scene.cpp:389, 516; com_speeds in common.cpp:2990-3001), which is useless at 250+ fps, and r_speeds has no Ghoul2 counters (tr_cmds.cpp:12-60). Add a steady_clock or SDL_GetPerformanceCounter timer and an r_speeds 8 mode reporting: skeleton builds (count and us, split into cgame-syscall vs renderer), surface walk, sort, world walk, RB_SurfaceGhoul skinning, and dlights/entities/polys dropped. Optionally add a CMake option for Tracy zones. Add unit tests (only server tests exist in tests/unit) that generate an N-bone GLA/GLM in memory, extending the FakeGLAFile approach at tr_model.cpp:86-108, with random animations, overrides, blends and bolts. Capture bone, bolt and skinned-vertex outputs from today's code as golden data, assert bit-exactness after each refactor, and add Google Benchmark micro-benchmarks. Use timedemo (common.cpp:2601) on a recorded 32-player demo as the end-to-end benchmark.

- **Player-visible effect:** None directly. It makes every performance claim verifiable and keeps mods and gameplay bit-identical.
- **Expected gain:** Enabler. Without it, no Ghoul2 optimization can safely ship, because server bolts drive gameplay. High confidence on value.
- **Risk:** Low. Synthetic models may miss quirks of real assets, so also run the timedemo with retail assets locally before merging.
- **Evidence:** `src/renderer/tr_scene.cpp:389`; `src/renderer/tr_scene.cpp:516`; `src/renderer/tr_cmds.cpp:12-60`; `src/qcommon/common.cpp:2990-3001`; `src/renderer/tr_model.cpp:86-108`; `tests/unit/CMakeLists.txt`; `src/qcommon/common.cpp:2601`

### FE-17 — Release build flags: LTO, optional PGO, explicit FP contraction policy
*tooling · effort S · client-only-transparent*

Release builds use plain -O2, with -g1 and -fno-omit-frame-pointer on every configuration (CMakeLists.txt:257-262), no INTERPROCEDURAL_OPTIMIZATION, and -msse2 only for 32-bit x86 (252-253). Add a CMake option for LTO (CMAKE_INTERPROCEDURAL_OPTIMIZATION), and an optional PGO workflow trained with the FE-16 timedemo. Set -ffp-contract=off explicitly. GCC's default with -std=gnu++11 (CXX_STANDARD 11 with extensions on, src/CMakeLists.txt:499, 633) is 'fast', which on aarch64, or with -march flags that enable FMA, likely produces different float results than x86 for the same Ghoul2/server math.

- **Player-visible effect:** Free fps for everyone.
- **Expected gain:** Typically 5-15% CPU for LTO plus PGO on idTech3-style codebases (not measured here). Low-moderate confidence. Deterministic floats across platforms protect FE-3/FE-4 bit-exactness.
- **Risk:** Low. Longer builds, and LTO can expose latent UB.
- **Prerequisites:** FE-16 (to measure)
- **Evidence:** `CMakeLists.txt:252-262`; `src/CMakeLists.txt:499`; `src/CMakeLists.txt:633`

### FE-18 — Parallel image decoding at level load (and optional DDS for HD packs)
*perf-3d · effort L · client-only-transparent*

Image loading is fully serial. R_LoadImage tries jpg, png and tga, each twice (with and without JKA assets), for every image (tr_image.cpp:2407-2460). Decoding and the CPU lightscale/mip work in Upload32 (844-983) then run on the main thread before the GL upload. Collect the image requests issued during shader parsing at level load, decode them on worker threads (the decoders only read pak data, so FS access must be thread-safe or pre-read), and upload on the GL thread. Optionally add a DDS (BC1-BC7) loader so HD texture packs load fast and keep VRAM low; OpenJK's rend2 has a DDS loader. Packs are only used when the user installs them, and the classic look is unchanged.

- **Player-visible effect:** Faster map loads and vid_restart. HD texture packs become practical.
- **Expected gain:** About 1.5-3x faster image loading on 4+ cores. Image decoding is likely a large share of map load time with PNG/JPG assets. Low-moderate confidence; check with FE-16 timing.
- **Risk:** FS and zone thread-safety. Image upload order and caching (AllocatedImages map, tr_image.cpp:796-799) must stay deterministic.
- **Prerequisites:** FE-16
- **Evidence:** `src/renderer/tr_image.cpp:2407-2460`; `src/renderer/tr_image.cpp:844-983`; `src/renderer/tr_image.cpp:796-799`; `src/renderer/tr_image.cpp:1093`

**Notes.** Confidence: every timing estimate comes from operation counts on the code paths cited, assuming a ~4 GHz desktop core. Nothing was measured, since this environment has no GPU and no assets. Treat the per-player numbers as +/-2x until FE-16 exists.

Suggested order:
- now: FE-16, FE-2 and FE-1;
- next: FE-3 and FE-4 (both bit-exact, and they help the dedicated server too);
- then: FE-5 and FE-9;
- opt-in visual items: FE-11, FE-13, FE-12;
- last: FE-6, FE-7 and FE-8 (large, or dependent on the backend).

Multithreading has limited returns in my area. Most per-player skeleton work runs inside serial cgame syscalls, because SetBoneAngles invalidates the cache and the next GetBoltMatrix rebuilds. Only skinning can be cleanly parallelized until FE-7 moves skeleton builds into the renderer.

World traversal, the radix sort and lightmaps are already efficient. Merging BSP surfaces would gain little in this GL1 path, because the backend already batches consecutive surfaces that share a shader (tr_backend.cpp:592-603).

Bit-exactness: Ghoul2 code is shared with the dedicated server (src/CMakeLists.txt MVMPDEDRendererFiles includes tr_ghoul2.cpp and tr_model.cpp), and server saber bolts drive gameplay. Three quirks must be preserved:
- smoothing on the server is keyed on tr.refdef.time (tr_ghoul2.cpp:2081);
- the dead `if (!boneUsedList)` guard (1859, 2102) means unsquash runs on all bones, including stale ones;
- the bone-pool decode divides by 16383.0f, and replacing that with a reciprocal multiply would not be bit-exact.

Cross-area findings for other agents:
- the glow path calls qglFinish every frame (tr_backend.cpp:1059);
- RB_CalcDiffuseColor is scalar (tr_shade_calc.cpp:1104-1152);
- SHADER_MAX_VERTEXES is 1000 (qfiles.h:10). It splits Ghoul2 models into many draws, and the stencil-shadow path silently skips any batch of 500 or more vertices (tr_shadows.cpp:139, while RB_CHECKOVERFLOW only flushes at 1000), so cg_shadows 2 drops shadows on large models. Raising it is engine-only: only the SDK and tools copies of qfiles.h reference it.

Doc mismatch: CVARS.rst says r_printMissingModels defaults to 1, but the code registers 0 (tr_init.cpp:1235). r_picmip, r_lodscale, cl_autolodscale and r_ghoul2* are undocumented.

On a listen server, the client and server Ghoul2 tables are switched by the global RicksCrazyOnServer flag (G2_API.cpp:35-50), another reason to keep Ghoul2 work single-threaded until it is refactored.

The FE-1 leak is reachable in stock builds whenever a Ghoul2 surface with a glow stage is drawn in an RDF_NOWORLDMODEL scene with r_DynamicGlow 1, for example UI or HUD model previews from mods.

## Visual quality: what jk2mv already offers vs what is missing compared with modern idTech3 forks (ioquake3, Quake3e, OpenJK/rend2)

**Current state.** Renderer = fixed-function GL1.x with client vertex arrays. The only programmable parts are ARB_vertex_program/ARB_fragment_program assembly (or NV register combiners), used in exactly two places: dynamic glow and post-process gamma. There are no FBOs, no GLSL and no VBOs: a grep for framebuffer/CreateShader/GenBuffers in src/renderer and src/sdl finds nothing. The GL context is a default compatibility one (sdl_window.cpp:767-799 only sets a version if windowDesc asks for one; tr_init.cpp:644 does not).

WHAT EXISTS (cvar names, defaults and where they live):

1) Dynamic glow
- Cvars: r_DynamicGlow 0 (tr_init.cpp:1075). Value 2 is a debug view that shows only the glow (tr_backend.cpp:1694).
- Undocumented tunables: r_DynamicGlowPasses 5, Delta 0.8, Intensity 1.13, Soft 1, Width/Height 320x240 (latched) (tr_init.cpp:1076-1081).
- Requirements: texture_rectangle, ARB_vp, (NV combiners or ARB_fp) and 4 TMUs (tr_init.cpp:582-599).
- How it works (tr_backend.cpp:1040-1095):
  - copies the scene to a texture (1046), clears the color buffer, then walks the whole draw list again drawing only glow stages (1055-1057, skipped at 580-581 and tr_shade.cpp:1126-1127);
  - calls qglFinish() every frame (1059) and does 2 more full-frame glCopyTexSubImage2D (1065, 1083);
  - runs a 4-tap blur for 5 passes, each pass doing another copy (1575-1641), then an additive overlay (1673-1745);
  - glow textures are RGBA16 but filled from the 8-bit backbuffer (tr_image.cpp:2801-2847).
- Portal/mirror views get their own RC_DRAW_SURFS (tr_main.cpp:1107-1149), and the glow block is not gated on viewParms.isPortal, so glow runs once per view.
- Which stages glow: the JKA 'glow' stage keyword (tr_shader.cpp:1710, 2251), plus jk2mv's '.dynGlow' sidecar files listing shader:image pairs (tr_shader.cpp:3479-3500, 3600-3612, 4091-4146; assets/shaders/sabers.dynGlow). r_saberGlow 1 (latched) drops sabers.dynGlow (tr_init.cpp:1233, tr_shader.cpp:4130-4134).
- The menu exposes it (assets/ui/jk2mp/setup.menu_patch:797-806).

2) Gamma and overbright
- Cvars: r_gamma, clamped to 0.5-3 (tr_image.cpp:2950-2954). r_intensity. r_overBrightBits 1, clamped to at most 1; with it on, the scene is drawn at half intensity, identityLight = 0.5 (tr_image.cpp:2921-2942). r_ext_gamma_control.
- r_gammamethod 0/1/2, default 2 = post-process (sdl_window.cpp:1019):
  - each frame, RE_EndFrame (tr_cmds.cpp:453-457) runs after the UI and the console (cl_scrn.cpp:445-507);
  - it re-specifies a rectangle texture with glCopyTexImage2D of the whole backbuffer (tr_backend.cpp:1326);
  - it draws a fullscreen quad with an ARB vp/fp pair (tr_shader.cpp:4155-4194) that looks up a 64^3 RGB 3D LUT built on the CPU (tr_image.cpp:2958-3002, created at 3025-3037);
  - the LUT is rebuilt live when r_gamma changes (tr_cmds.cpp:386-390).
- Fallback order is post-process, then hardware ramp, then none (sdl_window.cpp:1067-1077, tr_shader.cpp:4305-4313). Shader-based gamma therefore already exists.

3) Texture filtering and quality
- r_ext_texture_filter_anisotropic 2 (tr_init.cpp:1073), applied per texture (tr_image.cpp:120-122, 976-978) and live (tr_cmds.cpp:371-375). r_ext_texture_filter_anisotropic_avail is set at init.
- r_textureMode GL_LINEAR_MIPMAP_NEAREST, i.e. bilinear (tr_init.cpp:1126).
- r_picmip 1 (tr_init.cpp:1083): half-resolution textures by default.
- Other quality cvars: r_textureLODBias (1232, live at tr_cmds.cpp:378-380), r_openglMipMaps (GL_GENERATE_MIPMAP, tr_image.cpp:902-907), r_simpleMipMaps, r_colorMipLevels, r_detailtextures, r_texturebits/r_texturebitslm, r_ext_compress_textures/lightmaps (S3TC, tr_init.cpp:289-330), r_subdivisions 4, r_lodCurveError 250, r_lodbias, r_lodscale.

4) Image loading
- Formats: JPG, then PNG, then TGA, each tried with and without JKA assets, so 6 lookups per image (tr_image.cpp:2407-2459).
- Size: any power-of-two size up to GL_MAX_TEXTURE_SIZE (tr_image.cpp:880). Non-power-of-two images are refused (2564-2570).
- jk2mv-specific custom _mipN mip chains (2576-2620).
- Lightmaps are packed into 2048 atlases (tr_bsp.cpp:168-222).
- Shader keywords uielementHD, nopicmip, noTC and texturemode (tr_shader.cpp:2332-2373). .shader_mv override files (tr_shader.cpp:3978).

5) Anti-aliasing
- r_ext_multisample 0/2/4/8/16 asks for a multisampled default framebuffer through SDL, with a fallback if that fails (sdl_window.cpp:691, 764-765, 816-822).
- No alpha-to-coverage: alpha test is plain GL alpha test (tr_backend.cpp:350-372).

6) Window and display
- r_mode -2 = desktop resolution (sdl_window.cpp:1008, 683-697). -1 = custom r_customwidth/height. The mode table stops at 2560x1600 (60-98).
- Other cvars: r_fullscreen (exclusive), r_noborder, r_centerWindow, r_displayRefresh, r_swapInterval (live, 330-336), r_highdpi 1 (581-584), r_savedWindows, r_colorbits/depthbits/stencilbits, r_stereo, r_allowsoftwaregl.
- VMs are told the window size while the renderer draws at the GL drawable size (cl_main.cpp:4096-4100). refdef and 2D coordinates are rescaled by vidWidth/winWidth (tr_scene.cpp:398-405). The VM-to-drawable decoupling that render scaling needs is therefore already there.

7) Widescreen, HUD and FOV
- VMs draw in a 640x480 virtual space. MVAPI level 3 trap_MVAPI_SetVirtualScreen lets a mod's cgame or ui opt into correct aspect (cl_cgame.cpp:641-647, 1311-1313; cl_ui.cpp:744-750, 1153; mvapi.h:145-147). The bundled jk2mvmenu uses it; legacy cgame HUDs are stretched.
- r_aspectratio and r_customaspect are registered (tr_init.cpp:1091-1092) but never read by engine code; the setup menu only uses ui_r_aspectratio as a mode filter.
- FOV belongs entirely to cgame: fov_x/fov_y are passed through unchanged (tr_scene.cpp:406-407, tr_main.cpp:494-497).

8) Fonts
- r_fontSharpness picks *_sharpN font variants by resolution (tr_font.cpp:698-722, 998-1012; up to 8 variants). Bundled sharp1-5 go up to 1024x1024 (assets/fonts).
- Console: r_consoleFont 0/1/2. Code New Roman and M+ 1M are 256x256 atlases with custom mips (assets/gfx/2d).
- Related cvars: con_scale, con_height, con_opacity, mv_coloredTextShadows (default 2), mv_nameShadows.

9) Other visuals
- Off by default: r_flares 0 (per-flare glReadPixels depth stall, tr_flares.cpp:235), r_drawSun 0.
- Sky: r_fastsky.
- Dynamic lights: r_dynamiclight 1, r_dlightBacks, r_newDLights 0. Both dlight paths add an extra pass with planar XY projection, linear Z falloff and per-vertex color (tr_shade.cpp:409-540, 549-670). Hard cap of 32 lights because surfaces carry a bitmask (tr_types.h:7).
- r_environmentMapping. r_surfaceSprites, r_surfaceWeather, r_wind*. The r_we world effects command (rain/snow/fog, tr_WorldEffects.cpp:2173-2241).
- cg_shadows (read by the renderer as r_shadows): 1 = blob, 3 = planar projection (tr_ghoul2.cpp:1349-1358). Stencil value 2 is forced back to 1 unless developer mode is on (tr_backend.cpp:438-441).
- Capture: screenshot (JPEG, r_screenshotJpegQuality) and screenshot_tga only, no PNG. AVI capture (cl_avi*).

Every frame with the default settings already pays for one full-frame copy plus one fullscreen ARB pass, because the default is post-process gamma.

**Already exists (do not reinvent):**
- Shader-based (post-process) gamma with a 64^3 3D LUT: r_gammamethod 2 is the default (sdl_window.cpp:1019; tr_image.cpp:2958-3037; tr_backend.cpp:1305-1355; tr_shader.cpp:4155-4194). Extend it; do not add a second gamma path.
- Overbright in windowed mode already works through post gamma (tr_image.cpp:2929-2931 only disables it for hardware gamma in a window).
- Dynamic glow / bloom-on-tagged-stages: r_DynamicGlow plus 6 tunables (tr_init.cpp:1075-1081), the JKA 'glow' stage keyword, the jk2mv .dynGlow sidecar files and r_saberGlow (tr_shader.cpp:3479-3500, 4091-4146).
- MSAA through the default framebuffer: r_ext_multisample 0/2/4/8/16 with fallback (sdl_window.cpp:691-822), exposed in the menu.
- Anisotropic filtering up to the driver max, applied live (tr_image.cpp:120-122; tr_cmds.cpp:371-375), and r_textureLODBias.
- r_openglMipMaps hardware mip generation and jk2mv custom _mipN mip-chain loading (tr_image.cpp:902-907, 2576-2620).
- HiDPI support: r_highdpi, plus decoupling of VM coordinates (window size) from the GL drawable size (cl_main.cpp:4096-4100; tr_scene.cpp:398-405). Reuse it for render scaling.
- Desktop-resolution mode r_mode -2, custom modes r_mode -1, borderless window, r_centerWindow, saved window positions, live r_swapInterval.
- High-resolution fonts: r_fontSharpness with *_sharpN variants up to 1024^2 (tr_font.cpp:698-722, 998-1012); r_consoleFont with two modern monospace fonts; con_scale/con_height/con_opacity.
- Widescreen UI through MVAPI trap_MVAPI_SetVirtualScreen (cl_cgame.cpp:641-647; cl_ui.cpp:744-750). Do not modify mvapi.h: forks must define their own API (mvapi.h:19-21).
- Shader keywords uielementHD / nopicmip / noTC / texturemode and .shader_mv override files (tr_shader.cpp:2332-2373, 3978).
- menu_patch: jk2mv can patch the retail menus from assetsmv.pk3 (files.cpp:1336-1339). Use it to expose new video options without shipping a UI module.
- assetsmv.pk3 is exempt from sv_pure (files.cpp:347-349). Built-in programs embedded as C strings (as for glow and gamma) also work on pure servers.
- World effects (rain/snow/fog, r_we), surface sprites, wind cvars, r_flares, r_drawSun, planar projected shadows (cg_shadows 3).
- Screenshot JPEG/TGA, levelshot, and AVI capture with a fixed frame rate (cl_aviFrameRate, cl_aviMotionJpeg*).

### VQ-1 — Graphics presets: Classic (default) / Enhanced / Ultra, via a cfg in assetsmv plus a menu_patch entry
*visual-wow · effort S · client-only-opt-in*

The engine defaults are conservative, 2003-era values: r_picmip 1, so textures load at half resolution (tr_init.cpp:1083); bilinear GL_LINEAR_MIPMAP_NEAREST with visible mip bands (1126); anisotropic filtering only 2x (1073); dynamic glow off (1075); MSAA off (sdl_window.cpp:1017); r_subdivisions 4 (tr_init.cpp:1097). Ship preset cfgs in assetsmv.pk3, which is exempt from sv_pure (files.cpp:347-349), with an 'r_preset' command or exec. Enhanced: picmip 0, trilinear, anisotropy 16 clamped to r_ext_texture_filter_anisotropic_avail, glow 1 with resolution-aware sizes, MSAA 4, r_subdivisions 2. Ultra: adds the later post effects. Add one 'Graphics preset' row to the existing setup.menu_patch (assets/ui/jk2mp/setup.menu_patch:640-806 already shows picmip, texture mode, MSAA and glow). Classic stays the default and changes nothing.

- **Player-visible effect:** Clearly sharper textures right away (4x the texels with picmip 0), no mip banding on floors, crisp oblique surfaces, smoother curves, glowing sabers. In side-by-side screenshots it reads as a remaster, for zero engine risk.
- **Expected gain:** Visual: large for its cost. picmip 1 to 0 doubles texture resolution on each axis, a well-known Q3-engine effect (high confidence). Cost: more texture memory (base JK2 assets are small: a few hundred MB at most at picmip 0, fine on any GPU from the last 10+ years) and slightly longer loads. Anisotropy 16x and trilinear cost very little on any GPU from the last 10+ years (high confidence).
- **Risk:** Very low. MSAA can fail on some drivers but already falls back (sdl_window.cpp:816-822). Latched cvars need vid_restart, which the preset command should print.
- **Evidence:** `src/renderer/tr_init.cpp:1073`; `src/renderer/tr_init.cpp:1075`; `src/renderer/tr_init.cpp:1083`; `src/renderer/tr_init.cpp:1097`; `src/renderer/tr_init.cpp:1126`; `src/sdl/sdl_window.cpp:1017`; `src/qcommon/files.cpp:347-349`; `src/qcommon/files.cpp:1336-1339`; `assets/ui/jk2mp/setup.menu_patch:640-806`

### VQ-2 — Color grading, saturation/contrast/vibrance and dithering folded into the existing post-gamma 3D LUT
*visual-wow · effort S · client-only-opt-in*

R_SetColorMappings already fills a 64^3 RGB LUT that the default post-process gamma samples every frame (tr_image.cpp:2958-3002; tr_backend.cpp:1305-1355). Compose a grade into that LUT on the CPU: new cvars r_colorGrade (built-in procedural presets such as classic/vivid/cinematic/teal-orange/noir, or a LUT image path), plus r_saturation, r_contrast, r_vibrance, r_tint. Rebuild live through the existing r_gamma->modified hook (tr_cmds.cpp:386-390). Optionally add a tiny noise texture to the ARB fragment program (tr_shader.cpp:4162-4169) for dithering against 8-bit banding in fog and sky gradients. Presets must be generated in code or shipped in assetsmv.pk3 so they also work on pure servers. Caveat: the pass runs after the HUD and console (cl_scrn.cpp:445-507), so subtle grades also tint the HUD. With VQ-4 the grade can move to the per-world-view hook.

- **Player-visible effect:** Instantly different moods: punchier Yavin greens, cold Bespin, cinematic teal/orange for frag movies. Very visible in screenshots and videos. Classic stays identical.
- **Expected gain:** GPU cost about 0: it is the same texture fetch already done each frame. CPU about 1-5 ms only when a cvar changes (262k LUT entries). High confidence because the infrastructure is already in place.
- **Risk:** Low. Only active when r_gammamethod 2 is available (tr_init.cpp:603-612). Users on the hardware-ramp fallback do not get it and the feature must say so. A per-map LUT from map pk3s would be blocked on pure servers.
- **Evidence:** `src/renderer/tr_image.cpp:2913-3007`; `src/renderer/tr_image.cpp:3025-3037`; `src/renderer/tr_backend.cpp:1305-1355`; `src/renderer/tr_shader.cpp:4155-4194`; `src/renderer/tr_cmds.cpp:386-390`; `src/renderer/tr_cmds.cpp:453-457`; `src/client/cl_scrn.cpp:445-507`

### VQ-3 — Dynamic glow: resolution-aware glow buffer, remove the per-frame glFinish, skip portal/mirror views
*visual-wow · effort S · client-only-opt-in*

Three problems in the current glow:
- The blur buffer is fixed at 320x240 (latched, tr_init.cpp:1080-1081, clamped at tr_image.cpp:2826-2833), whatever the resolution. At 1440p or 4K that is a 6-9x upscale, so saber halos look blocky and shimmer when the saber moves.
- qglFinish() runs every glowing frame (tr_backend.cpp:1059) and serializes CPU and GPU.
- Glow is gated only on RDF_NOWORLDMODEL (tr_backend.cpp:1040), so mirror/portal sub-views (each its own RC_DRAW_SURFS, tr_main.cpp:1107-1149) pay the full glow cost again: 3 full-frame copies, a second draw-list walk and 5 blur passes.
Fixes: (a) let r_DynamicGlowWidth/Height '0' mean auto (about vidHeight/3-4, keeping aspect), with more passes or a wider kernel. The opt-in default stays 320x240. (b) Drop the finish, keeping a cvar to restore it for old drivers. (c) Skip glow when viewParms.isPortal.

- **Player-visible effect:** Smooth, wide, stable saber and weapon halos at high resolutions, the iconic JK2 look done properly. Steadier framerate with glow on.
- **Expected gain:** Perf with glow on: removing the CPU/GPU sync typically restores pipelining. Estimate 10-30% fps at high framerates; maps with mirrors/portals roughly halve glow cost. Low-medium confidence: there is no GPU here, so measure with r_speeds/com_speeds. Visual: high, based on how JKA/OpenJK glow scales with buffer size.
- **Risk:** The glFinish was probably a workaround for 2003-era ATI texture_rectangle drivers (g_bTextureRectangleHack, tr_init.cpp:575-580), so keep an escape hatch. A larger glow buffer adds blur fill cost, around 0.1-0.3 ms at 1/4 resolution.
- **Evidence:** `src/renderer/tr_backend.cpp:1040-1095`; `src/renderer/tr_backend.cpp:1059`; `src/renderer/tr_backend.cpp:1575-1641`; `src/renderer/tr_init.cpp:1076-1081`; `src/renderer/tr_image.cpp:2801-2847`; `src/renderer/tr_main.cpp:1107-1149`; `src/renderer/tr_init.cpp:575-599`

### VQ-4 — Streamline the default post-gamma pass (copy into existing storage, skip it when it is the identity)
*perf-3d · effort S · client-only-transparent*

Every frame, with default settings, RB_GammaCorrection re-specifies the rectangle texture with qglCopyTexImage2D of the whole backbuffer (tr_backend.cpp:1326). It should use qglCopyTexSubImage2D into storage allocated once when the resolution changes (R_UpdateImages, tr_image.cpp:2905). It should also skip the pass entirely when r_gamma == 1 and tr.overbrightBits == 0 (identity LUT), unless a grade (VQ-2) is active. Once VQ-5 exists, the copy disappears because the scene texture is sampled directly.

- **Player-visible effect:** Nothing visual. Slightly higher and steadier fps for everyone on default settings, mostly on iGPUs at high resolution.
- **Expected gain:** About 0.1-0.5 ms per frame at 1440p-4K on integrated GPUs (one 15-33 MB copy, and possibly a driver reallocation each frame). Low-medium confidence: driver dependent, must be measured.
- **Risk:** Very low. Must keep the hardware-gamma screenshot path consistent (tr_cmds.cpp:542, 589).
- **Evidence:** `src/renderer/tr_backend.cpp:1305-1355`; `src/renderer/tr_backend.cpp:1326`; `src/renderer/tr_image.cpp:2905-2907`; `src/renderer/tr_cmds.cpp:453-457`

### VQ-5 — Infrastructure: FBO scene target plus a small built-in post-process program manager (ARB first, GLSL 1.20 optional)
*visual-wow · effort M · client-only-opt-in*

This is the prerequisite for HDR, bloom, render scale, FXAA, SSAO and soft particles, and it also deletes copies.
- Load EXT/ARB_framebuffer_object, EXT_framebuffer_blit and EXT_framebuffer_multisample in GLimp_InitExtensions (tr_init.cpp:380-620) and qgl.h.
- Create a color attachment (RGBA8 or RGBA16F) and a depth24_stencil8 attachment at vidWidth x vidHeight x r_renderScale; recreate them in R_UpdateImages (tr_image.cpp:2905).
- Bind at frame start (RE_BeginFrame / RB_DrawBuffer, tr_cmds.cpp:410-445). Resolve to the window in the existing end-of-frame gamma pass (tr_backend.cpp:1305), sampling the FBO texture instead of copying (1326).
- Glow then reads attachments instead of making 3+5 copies (tr_backend.cpp:1046, 1065, 1083, 1614, 1633).
- Turn MV_GammaGenerateProgram (tr_shader.cpp:4171-4194) into a table of built-in programs embedded as C strings so they survive sv_pure.
- Target GL 2.1 + EXT_fbo + (ARB asm or GLSL 1.20), the level the macOS legacy context supports.
- Gotchas: manual texture names (1024 + giTextureBindNum++, tr_image.cpp:1256, 2864-2866, 3029) are legal only in compatibility contexts; stereo (tr_cmds.cpp:426-433); r_drawBuffer GL_FRONT; stencil for r_measureOverdraw and dev shadows (tr_backend.cpp:457-460); RB_ReadPixels for screenshots and AVI (tr_backend.cpp:1373-1390).
Precedent: Quake3e's GL1 renderer reaches FBO, bloom, HDR, MSAA/SSAA and render scale with ARB programs (r_fbo), which shows this route fits a codebase like this one.

- **Player-visible effect:** None by itself (r_fbo 0 by default). It is the foundation for every 'wow' effect below and reduces glow cost when on.
- **Expected gain:** Enables VQ-6 to VQ-12. Removes 1 full-frame copy per frame with default gamma and 8 with glow. Medium confidence on effort, based on Quake3e's tr_arb/FBO code size (low thousands of lines including effects; the core FBO plumbing is a few hundred).
- **Risk:** Driver variance (old Intel/Mesa and macOS), interaction with MSAA (needs a multisampled FBO and a blit resolve, because CopyTexImage from a multisampled FBO is invalid), the stereo path, and screenshot/AVI ordering. Mitigation: keep the current copy path as the r_fbo 0 fallback.
- **Evidence:** `src/renderer/tr_init.cpp:380-620`; `src/renderer/tr_shader.cpp:4171-4194`; `src/renderer/tr_backend.cpp:1046`; `src/renderer/tr_backend.cpp:1065`; `src/renderer/tr_backend.cpp:1083`; `src/renderer/tr_backend.cpp:1326`; `src/renderer/tr_backend.cpp:1373-1390`; `src/renderer/tr_image.cpp:1256`; `src/renderer/tr_image.cpp:2864-2866`; `src/renderer/tr_cmds.cpp:410-445`; `src/sdl/sdl_window.cpp:767-799`

### VQ-6 — HDR scene buffer (RGBA16F) with tonemapping, plus modern threshold bloom (mip-chain / dual-Kawase)
*visual-wow · effort M · client-only-opt-in*

Today overbright renders the world at half intensity (identityLight 0.5, tr_image.cpp:2941-2942; overbright clamped to at most 1 at 2933-2939) into an 8-bit backbuffer, then the LUT doubles it (2981-2984). That leaves about 7 effective bits, so dark areas and fog band.
With a float scene FBO:
- precision is kept;
- additive effects (sabers, blaster bolts, explosions, dlights; GLS_SRCBLEND_ONE|GLS_DSTBLEND_ONE, tr_shade.cpp:664-668) accumulate above 1.0;
- a tonemapper (ACES/Hable/Reinhard plus optional auto-exposure) compresses the highlights;
- a threshold bloom built from a downsample chain picks up every bright pixel.
This complements the tag-based dynamic glow and can share its blur. The 'off' path must reduce exactly to the classic image.

- **Player-visible effect:** Sabers that really burn and bleed light, explosions with hot cores, no banding in dark corridors and fog. This is the 'mouth-watering' saber-duel screenshot and video.
- **Expected gain:** GPU about 0.3-1 ms at 1440p on mid-range GPUs for bloom plus tonemap; a float target doubles framebuffer bandwidth. Medium confidence, based on comparable implementations (Quake3e r_bloom/r_hdr, ioq3 rend2 r_hdr/r_toneMap). Visual impact very high.
- **Risk:** Changes the look heavily, so it must stay strictly opt-in. Old iGPUs are slow with float targets. Fixed-function texture-environment output is clamped per stage, so the HDR comes from blending and precision, not from each surface. Tuning is needed so maps do not wash out.
- **Prerequisites:** VQ-5
- **Evidence:** `src/renderer/tr_image.cpp:2921-2942`; `src/renderer/tr_image.cpp:2973-2985`; `src/renderer/tr_shade.cpp:664-668`; `src/renderer/tr_image.cpp:2810`; `src/renderer/tr_backend.cpp:1673-1745`

### VQ-7 — Render scale and supersampling (r_renderScale), supersampled screenshots and AVI
*visual-wow · effort S · client-only-opt-in*

Render the 3D scene into the FBO at k x the drawable size, then downsample with a filtered resolve. k > 1 gives SSAA for strong GPUs, screenshots and capture; k < 1 helps weak GPUs at 4K. The hard part already exists: VMs only ever see window coordinates (cl_main.cpp:4096-4100), and the renderer rescales refdef and 2D (tr_scene.cpp:398-405). So no cgame or ui change is needed, and the HUD can stay at native resolution. Add 'screenshot 2x/4x' and let AVI capture (cl_avi.cpp:203-206) use the supersampled resolve.

- **Player-visible effect:** Completely shimmer-free edges and textures, including alpha-tested foliage and specular shader aliasing that MSAA cannot fix. Pristine screenshots and frag-movie footage.
- **Expected gain:** 2x SSAA costs 4x the pixels. JK2's geometry and fill are light, so modern dGPUs are usually CPU-bound and SSAA is close to free there (medium confidence). Quake3e's r_renderScale/r_ext_supersample show the approach works.
- **Risk:** Large FBOs at 4K x 2 (memory). Mouse picking in UI 3D models and the cgame refdef rounding (tr_scene.cpp:396-405) need checking.
- **Prerequisites:** VQ-5
- **Evidence:** `src/client/cl_main.cpp:4096-4100`; `src/renderer/tr_scene.cpp:396-405`; `src/client/cl_avi.cpp:203-206`; `src/renderer/tr_backend.cpp:1373-1390`

### VQ-8 — Alpha-to-coverage for alpha-tested stages when MSAA is on
*visual-wow · effort S · client-only-opt-in*

MSAA (r_ext_multisample) only smooths geometric edges. Alpha-tested grates, fences, foliage and surface-sprite grass (GLS_ATEST_*, tr_backend.cpp:350-372; tr_shade.cpp:1144) stay jagged. When samples > 0 and r_ext_alphaToCoverage 1, enable GL_SAMPLE_ALPHA_TO_COVERAGE (glext.h:159) for those stages, with or without keeping the alpha test. No FBO needed.

- **Player-visible effect:** Soft, anti-aliased grass, trees and fences (Yavin, Bespin railings), especially visible in motion.
- **Expected gain:** About 0 ms. Visual medium. Comparable forks expose the same switch (Quake3e has an alpha-to-coverage cvar; medium confidence on the exact name).
- **Risk:** Slight look change at alpha thresholds (GE_80/GE_C0), so keep it opt-in. Some old drivers implement it poorly.
- **Evidence:** `src/renderer/tr_backend.cpp:350-372`; `src/renderer/tr_shade.cpp:1144`; `src/renderer/glext.h:159`; `src/sdl/sdl_window.cpp:691`

### VQ-9 — Per-pixel dynamic lights (saber light, blaster bolts, explosions), with a higher internal light cap
*visual-wow · effort M · client-only-opt-in*

Both dlight paths project a 2D texture in XY with linear falloff in Z and per-vertex colors, as an extra additive pass (tr_shade.cpp:409-540 and 549-670; the falloff is at 592-627). On coarse BSP tessellation that gives square or stretched blobs. Replace the color/texcoord generation with an ARB fragment program (existing infrastructure; Quake3e's per-pixel dlight mode was done the same way) or GLSL: spherical attenuation plus N.L per pixel, keeping the same pass and GLS_DEPTHFUNC_EQUAL state. The base cgame already adds a colored dlight for each saber blade and each bolt (in cgame, not in this repo; medium-high confidence), so no VM change is needed. MAX_DLIGHTS 32 comes from the per-surface bitmask (tr_types.h:7). Widening it internally to 64 bits is engine-only.

- **Player-visible effect:** Sabers sweep smooth colored light over walls and floors, bolts light up corridors as they fly, explosions flash the room. Dramatic in dark maps and in videos.
- **Expected gain:** Cost similar to today per light-surface pass, plus fragment math that is trivial on any GPU of the last 15 years. Visual high. Medium confidence.
- **Risk:** Entities (Ghoul2/MD3) also get vertex-lit dlights through entity lighting; matching them needs care. Overbright scaling must match the classic intensity. Keep opt-in through r_dlightMode.
- **Evidence:** `src/renderer/tr_shade.cpp:409-540`; `src/renderer/tr_shade.cpp:549-670`; `src/renderer/tr_shade.cpp:592-627`; `src/cgame/tr_types.h:7`; `src/renderer/tr_scene.cpp:312-350`; `src/renderer/tr_init.cpp:1123-1124`; `src/renderer/tr_init.cpp:1168`

### VQ-10 — FXAA (later SMAA) plus contrast-adaptive sharpening on the 3D view
*visual-wow · effort M · client-only-opt-in*

Add a post-AA and sharpen pass in the per-world-view hook where glow already runs (tr_backend.cpp:1040-1095), so HUD text is never blurred: the end-of-frame gamma pass runs after the HUD (cl_scrn.cpp:445-507). FXAA 3.11 in GLSL 1.20 is the pragmatic first step. SMAA (MIT; needs area/search textures and 3 passes) comes later. Sharpening pairs well with picmip 0 and anisotropic filtering. Without VQ-5 it could still be done with one copy, like glow does today.

- **Player-visible effect:** Cleaner edges with no MSAA cost, including shader aliasing; slightly crisper textures.
- **Expected gain:** About 0.2-0.6 ms at 1440p on mid-range or integrated GPUs (low-medium confidence). Visual medium, mostly seen in motion.
- **Risk:** FXAA softens fine texture detail (hence the sharpen option). Needs the GLSL path or a painful ARB port.
- **Prerequisites:** VQ-5
- **Evidence:** `src/renderer/tr_backend.cpp:1040-1095`; `src/client/cl_scrn.cpp:445-507`; `src/renderer/tr_cmds.cpp:453-457`

### VQ-11 — Cinematic capture: motion blur by sub-frame accumulation (and optional depth of field) for demos and AVI
*fun-feature · effort M · client-only-opt-in*

AVI capture already advances time in fixed steps (cl_main.cpp:2456-2462) and grabs each frame (cl_avi.cpp:203-206). Add r_aviMotionBlurFrames N: render N sub-frames per output frame at timescale/N and accumulate them into a float FBO before capture, the approach the q3mme/JK2-MME frag-movie tools made popular. An optional depth-based DoF can use the FBO depth attachment. Demo playback only, so there is zero gameplay impact.

- **Player-visible effect:** Saber swings and acrobatics with real cinematic motion blur. Frag movies and trailers look like a modern game, the ultimate 'mouth-watering' showcase.
- **Expected gain:** Capture runs N times slower than real time (offline, acceptable). No runtime cost when off. Very high impact for videos (high confidence on the effect, medium on effort).
- **Risk:** Sound sync while capturing (snd_dma.cpp:2337-2343 already handles fixed-rate capture). FX and particle timing at sub-frame steps.
- **Prerequisites:** VQ-5
- **Evidence:** `src/client/cl_main.cpp:2456-2462`; `src/client/cl_avi.cpp:203-206`; `src/client/cl_avi.cpp:598-615`; `src/client/snd_dma.cpp:2337-2343`

### VQ-12 — Soft particles for the engine-side FX system
*visual-wow · effort M · client-only-opt-in*

JK2's effects system (smoke, sparks, explosions, saber clash) lives in the engine (src/client/Fx*.cpp, MAX_EFFECTS 1800 at FxPrimitives.h:13-16), not in the VMs. With the FBO depth attachment, fade particle alpha near scene depth in a small fragment program for FX sprite shaders.

- **Player-visible effect:** Smoke and explosion sprites stop showing hard cut lines where they cross floors and walls. Volumetric-looking smoke.
- **Expected gain:** Small fragment cost for particles only. Visual medium. Medium confidence.
- **Risk:** Must not apply to UI/2D or to shaders that rely on the hard intersection. Opt-in.
- **Prerequisites:** VQ-5
- **Evidence:** `src/client/FxPrimitives.h:13-16`; `src/client/FxScheduler.cpp:115`

### VQ-13 — SSAO / contact shadows for vertex-lit models
*visual-wow · effort M · client-only-opt-in*

Players and models are lit per vertex from the light grid (RB_CalcDiffuseColor, tr_shade_calc.cpp:1104). Only the BSP has baked shadowing, so characters look like they float. Half-resolution SSAO computed from the FBO depth, blurred and applied to the 3D view before the HUD, grounds them.

- **Player-visible effect:** Characters and props sit in the world, with soft contact darkening in corners.
- **Expected gain:** About 1-3 ms at 1440p on mid-range GPUs at half resolution (medium confidence, based on rend2/ioq3 r_ssao-class costs). Visual medium.
- **Risk:** Halo and 'dirty' artifacts. It duplicates baked AO in lightmaps, so tune it to affect mostly dynamic objects. Opt-in.
- **Prerequisites:** VQ-5
- **Evidence:** `src/renderer/tr_shade_calc.cpp:1104`; `src/renderer/tr_backend.cpp:1040-1095`

### VQ-14 — High-resolution texture-pack enablement: NPOT resampling, pre-compressed DDS (BC1/3/5/7), faster lookups
*visual-wow · effort M · client-only-opt-in*

Already there: any power-of-two size up to GL_MAX_TEXTURE_SIZE (tr_image.cpp:880), custom mips (2576-2620), and nopicmip/noTC per shader (tr_shader.cpp:2332-2373).
Missing:
- Non-power-of-two images are refused outright (tr_image.cpp:2564-2570) instead of being resampled.
- No DDS/KTX loader. A 4K RGBA texture is about 85 MB with mips versus about 21 MB in BC7. S3TC detection exists (tr_init.cpp:289-330); BC7 needs ARB_texture_compression_bptc.
- Each image costs 6 filesystem probes (tr_image.cpp:2407-2459).
- Decoding is serial.
Hard constraint: sv_pure ignores pk3s the server does not have (files.cpp:343-366), so packs only load on non-pure servers. Do not weaken that, because texture swaps are the classic wallhack.

- **Player-visible effect:** With community packs, 2-4x sharper world and characters, and faster loads.
- **Expected gain:** Engine work only enables it; the visual gain depends on assets (community upscales exist for JK2/JKA; medium confidence). DDS cuts VRAM 4-8x and upload time a lot for large packs (high confidence).
- **Risk:** 32-bit builds are tight on address space with big packs. Pure servers make the feature invisible. Asset licensing is a matter for the pack authors.
- **Evidence:** `src/renderer/tr_image.cpp:880`; `src/renderer/tr_image.cpp:2407-2459`; `src/renderer/tr_image.cpp:2564-2570`; `src/renderer/tr_image.cpp:2576-2620`; `src/renderer/tr_init.cpp:289-330`; `src/qcommon/files.cpp:343-366`

### VQ-15 — 4K text polish: high-resolution console font atlases and an optional aspect-correct HUD for legacy cgame
*visual-wow · effort S · client-only-opt-in*

The console fonts are 256^2 atlases (assets/gfx/2d/code_new_roman.tga, mplus_1m_bold.tga plus _mip1-3), so con_scale > 1 at 4K is blurry. Ship 1024^2 variants (assets only) or route the console through the existing *_sharpN variant selection (tr_font.cpp:698-722).
HUD: legacy cgame draws 640x480 stretched unless the mod calls trap_MVAPI_SetVirtualScreen (cl_cgame.cpp:641-647). An opt-in client cvar could force a 4:3-correct virtual screen (cls.cgxadj/cgyadj) for non-MVAPI cgames: round icons and crosshair, pillarboxed HUD. Proper per-element anchoring needs MVSDK work.
Also drop or document the dead r_aspectratio/r_customaspect cvars (tr_init.cpp:1091-1092).

- **Player-visible effect:** A crisp console and timer at 4K. A non-stretched HUD on widescreen for old mods.
- **Expected gain:** No runtime cost. Visual low-medium (polish). High confidence.
- **Risk:** Forcing a virtual screen moves edge-anchored HUD items toward the centre, so some players will dislike it; keep it opt-in. Mods that already call SetVirtualScreen must take precedence.
- **Evidence:** `assets/gfx/2d/code_new_roman.tga`; `src/renderer/tr_font.cpp:698-722`; `src/renderer/tr_font.cpp:998-1012`; `src/client/cl_cgame.cpp:641-647`; `src/client/cl_cgame.cpp:1311-1313`; `src/renderer/tr_init.cpp:1091-1092`

### VQ-16 — Player shadow maps (soft, fairness-aware) as a new opt-in shadow mode
*visual-wow · effort L · client-only-opt-in*

Today: blob shadows (cg_shadows 1, drawn by cgame) or planar projection (3, tr_ghoul2.cpp:1349-1358). Stencil volumes (2) are forced off outside developer mode (tr_backend.cpp:438-441) because they leak information. A new mode would render a per-player depth map from the dominant light-grid direction (tr_light.cpp), sample it with PCF and apply it only to surfaces visible in the main view. Casters would be limited to entities inside the view frustum plus a small margin, so no extra information is revealed.

- **Player-visible effect:** Characters cast soft, correctly oriented shadows on floors and walls. Big realism jump in screenshots.
- **Expected gain:** GPU 0.5-2 ms. The CPU cost is the concern: Ghoul2 skinning happens per draw on the CPU (tr_ghoul2.cpp), so each shadow-casting model is skinned twice unless skinned vertices are cached. Medium-low confidence.
- **Risk:** Competitive fairness (shadows around corners). CPU cost with many players. Light-grid direction is ambiguous indoors. Must stay off by default and should probably be blockable by servers through a fork-specific configstring.
- **Prerequisites:** VQ-5
- **Evidence:** `src/renderer/tr_backend.cpp:438-441`; `src/renderer/tr_ghoul2.cpp:1337-1358`; `src/renderer/tr_mesh.cpp:360-372`; `src/renderer/tr_shadows.cpp:244`

### VQ-17 — Normal/specular/parallax mapping and PBR (a rend2-class renderer): defer, consider an optional separate renderer later
*visual-wow · effort XL · client-only-opt-in*

This needs a GLSL lighting path, tangent frames for BSP, MD3 and Ghoul2 GLM (skinned on the CPU in tr_ghoul2.cpp), and deluxemaps that JK2 maps do not ship. Above all it needs assets: JK2 has no _n/_s textures, and auto-generated normal maps look mediocre. ioq3 rend2 and OpenJK rd-rend2 took years and OpenJK's still has parity gaps (glow, weather, surface sprites). If wanted, port OpenJK rd-rend2 (GPLv2, JKA formats are close) as an optional second renderer, rather than growing the GL1 renderer.

- **Player-visible effect:** Bumpy, shiny materials and modern lighting, but only on content that has the maps.
- **Expected gain:** High visually where assets exist, near zero elsewhere. Very high cost. Medium confidence.
- **Risk:** Huge scope, regressions against the classic look, and mod shader compatibility (JK2 .shader semantics).
- **Prerequisites:** VQ-5
- **Evidence:** `src/renderer/tr_shade_calc.cpp:1104`; `src/renderer/tr_shader.cpp:2332-2373`; `src/renderer/tr_bsp.cpp:168-222`

### VQ-18 — Document visual cvars and fix doc/code default mismatches; add a 'r_postinfo' status line to gfxinfo
*tooling · effort S · client-only-transparent*

CVARS.rst has gaps and wrong defaults:
- r_dynamicGlow is listed as 0/1, but 2 exists as a debug view (tr_backend.cpp:1694), and Passes/Delta/Intensity/Soft/Width/Height are undocumented (tr_init.cpp:1076-1081).
- con_timestamps is documented as default 1, but the code uses 0 (cl_console.cpp:411).
- r_printMissingModels is documented as 1, but the code uses 0 (tr_init.cpp:1235).
- The mv_coloredTextShadows default of 2 is undocumented (cl_main.cpp:2951).
- r_aspectratio/r_customaspect do nothing.
Also extend gfxinfo (tr_init.cpp:983-990 already prints the gamma method) to report glow support, post-process availability and the active effects, so users can tell why a feature is inactive.

- **Player-visible effect:** Players and mod authors can discover and tune the visuals that already exist.
- **Expected gain:** No runtime cost. Fewer 'why is glow off?' support questions. High confidence.
- **Risk:** None.
- **Evidence:** `CVARS.rst`; `src/renderer/tr_init.cpp:1076-1081`; `src/renderer/tr_init.cpp:1235`; `src/renderer/tr_init.cpp:983-990`; `src/client/cl_console.cpp:411`; `src/client/cl_main.cpp:2951`; `src/renderer/tr_backend.cpp:1694`

**Notes.** Nothing could be measured: there is no GPU and there are no assets here. All costs are estimates from the code and from what comparable forks report, with a confidence given for each. Before claiming any gains, validate with r_speeds, com_speeds and a fixed timedemo on 2-3 GPU classes (iGPU, older dGPU, modern dGPU).

Suggested order:
1. The S items that need no new GL infrastructure: VQ-1 presets, VQ-2 LUT grading, VQ-3 glow fixes, VQ-4 gamma-pass streamlining, VQ-8 alpha-to-coverage, VQ-15, VQ-18. Together they give a visible jump in a few days of work, without touching the protocol or the VMs.
2. VQ-5, the FBO plus program-manager groundwork, done as its own reviewable PR behind r_fbo 0.
3. In order of 'wow' per effort: VQ-6 HDR + bloom, VQ-7 SSAA/render scale, VQ-9 per-pixel dlights (this one can be done earlier with ARB, no FBO), VQ-11 cinematic motion blur, VQ-10 FXAA, VQ-12 soft particles, VQ-13 SSAO.
4. VQ-16 player shadows and VQ-17 rend2-class materials are long-term.

Design constraints found in the code:
- Do not edit mvapi.h. It says forks must define their own API (mvapi.h:19-21), so any mod-facing visual hook, for example letting a mod tag glow or set a per-map LUT, needs a fork-specific extension.
- Every new effect must work on sv_pure servers. Put programs and presets in the binary as strings, like the existing glow and gamma programs (tr_shader.cpp:4155-4169), or in assetsmv.pk3 (files.cpp:347-349). Menu changes can ship as .menu_patch, which is only accepted from assetsmv (files.cpp:1336-1339).
- Target GL 2.1 + EXT_framebuffer_object + ARB programs or GLSL 1.20, which macOS legacy contexts and old Mesa support. Quake3e shows ARB programs are enough for FBO, bloom, HDR, SSAA and per-pixel dlights in a GL1-class renderer.
- Manual texture names (1024 + giTextureBindNum) only work in compatibility contexts, which matters if a core-profile move is ever considered.
- The VM sees winWidth/winHeight while the GL drawable is vidWidth/vidHeight. This is what makes render scale and SSAA cheap to add.

Deliberately not proposed for the engine:
- Hor+ widescreen FOV. FOV is owned by cgame (tr_scene.cpp:406-407), and widening it beyond cg_fov limits gives a competitive edge. If wanted, it belongs in MVSDK's cgame, like OpenJK's cg_fovAspectAdjust.
- Saber trail geometry (segment count and smoothness at high fps). It is generated by cgame; the engine only draws RT_SABER_GLOW sprites and lines (tr_surface.cpp:455-479). The engine can still make sabers look far better through VQ-3, VQ-6, VQ-8 and VQ-9 without any VM change.

Side findings:
- Glow runs a full extra pass for every mirror or portal view (no isPortal gate).
- RB_SwapBuffers calls glFinish whenever no 3D view set finishCalled, i.e. in menus (tr_backend.cpp:1268-1270).
- r_flares stalls on glReadPixels for each flare (tr_flares.cpp:235). Occlusion queries would let flares be enabled in a preset; this is perf-side.

All opportunities above leave the protocol, VM ABI, server and default look unchanged.

## Server performance and load capacity (src/server, qcommon cm_*/msg/net_chan/huffman/vm, limits in q_shared.h/qcommon.h/server.h)

**Current state.** FRAME STRUCTURE. A dedicated server sleeps in Com_Frame until SV_FrameMsec() (common.cpp:2872, 2902-2909). Packets are handled as they arrive inside NET_Sleep->NET_Event->SV_PacketEvent (net_ip.cpp:1040-1075, common.cpp:2301-2318, sv_main_packets.h:14-69). SV_Frame (sv_main_frame.h:59-176) does these steps: count non-bot players for hibernation (83-97), BotFrame (99/142), update serverinfo/systeminfo configstrings when cvars change (124-131), SV_CalcPings (32x32 loop, sv_main_clients.h:14-60), then 'while (timeResidual >= frameMsec) VM_Call(GAME_RUN_FRAME)' (150-158, frameMsec = 1000/sv_fps at :79), then SV_CheckTimeouts, SV_SendClientMessages (:168) and heartbeat. Everything runs on one thread. The only existing thread is the HTTP download server (net_http.cpp:3,35), which shows std::thread is already accepted in the codebase.

PER-SECOND CPU MODEL (inferred from the code, not measured):
(a) Player movement: sum over clients of usercmds/s x Pmove cost. Each new usercmd calls VM GAME_CLIENT_THINK (sv_client_usercmd.h:45, 124-131). This does not depend on sv_fps. It scales with each client's com_maxfps/pmove_fixed (up to 32 cmds per packet, :77-80; MAX_PACKET_USERCMDS qcommon.h:164), and bg_pmove.c/bg_slidemove.c have 28 trace/pointcontents call sites. For human players this is most likely the largest cost.
(b) sv_fps x GAME_RUN_FRAME. This is G_RunFrame over all entities: thinks, missile traces, and per client WP_SaberPositionUpdate with a ghoul2 bolt matrix plus saber traces (mvsdk g_main.c:2709, w_saber.c:3468). Add the bot AI once per SV_Frame.
(c) The snapshot pipeline, once per snapshot built. Humans get min(sv_fps, snaps, rate) snapshots/s. Bots get one every frame, because SV_SendClientSnapshot returns before nextSnapshotTime is updated (sv_snapshot_sender.h:104-110). LAN clients also get one every frame (:62-63). SV_AddEntitiesVisibleFromPoint is O(sv.num_entities) per snapshot, with cheap per-entity tests: linked, MVAPI filters, svFlags, area connectivity, cluster PVS bits (sv_snapshot_entities.h:92-221). Then a qsort runs on every snapshot even when no portal was seen (sv_snapshot_builder.h:82), and ~296-byte entityState_t copies go into a shared ring (:93-105). Delta encoding writes field by field (sv_snapshot_delta.h, msg.cpp:1001-1120), and every byte is Huffman-encoded by a recursive walk up the parent pointers of the tree (msg.cpp:169-176, huffman.cpp:270-301), with global state 'bloc' (huffman.cpp:9).
(d) Traces. SV_Trace = CM_BoxTrace on the world BSP + SV_AreaEntities over a 16-leaf 2D sector tree (sv_world_sectors.h:14-81, splits on X/Y only) + CM_TransformedBoxTrace per candidate (sv_world_trace.h:91-258). The entity search box covers the full start-to-end move even though the comment says it is clipped to the world hit (sv_world_trace.h:240-252). Entities that cross a split plane stay on interior nodes and are scanned by every query that passes through them (sv_world_linking.h:302-319, sv_world_area.h:361-394). Unlink is a linear scan of a singly linked list (sv_world_linking.h:135-147). The collision code uses global state (cm.checkcount, cm_trace.cpp:1137), so it is not thread-safe.

Order-of-magnitude estimate (low confidence, nothing measured here): the engine-side parts (c)+(d) outside the VM are a minority of the frame. The game VM (Pmove + G_RunFrame + bot AI, plus the traces it calls) dominates. At 32 players a modern core is probably far from saturated: idTech3 servers ran these loads on ~1 GHz CPUs. In practice, multi-core scale-out today means running several server processes per machine.

NETWORK. MAX_PACKETLEN 1400 / FRAGMENT_SIZE 1300 (net_chan.cpp:26-27). Only the first fragment is sent right away (net_chan.cpp:148-157). Later fragments go out only from SV_SendClientMessages, i.e. at most once per server frame (sv_snapshot_sender.h:188-192). The rate is enforced per message with sizes capped at 1500+48 bytes (:17-27, 66-76). Every configstring change becomes an immediate reliable command to every client (sv_init_configstrings.h:37-77). More than 128 unacknowledged reliable commands drops the client (sv_main_messaging.h:70-79). SV_ReplacePendingServerCommands exists but is never called (sv_main_messaging.h:40-58). The snapshot entity ring holds sv_maxclients x PACKET_BACKUP x 64 entries (sv_init_clients.h:69,141, allocated with new[] at sv_init_mapload.h:165): 65,536 x 296 B = 19.4 MB at 32 slots. When an old frame's entities have rolled off, the client gets a full non-delta snapshot (sv_snapshot_writer.h:33-41).

LIMITS. Fixed by the protocol or the VM ABI:
- MAX_CLIENTS 32 (q_shared.h:1321). The cgame/game arrays depend on it, so do broadcastClients[2] (g_public.h:68) and the MVAPI snapshotIgnore/Enforce[32] (mvapi.h:102-106). SV_BoundMaxClients clamps to it (sv_init_clients.h:38-49).
- MAX_GENTITIES 1024 / GENTITYNUM_BITS 10 (q_shared.h:1324-1325; 10-bit entity numbers on the wire, game VM entity array).
- MAX_ENTITIES_IN_SNAPSHOT 256 on the client side (cg_public.h:12; anything beyond is truncated at cl_cgame.cpp:158-160).
- MAX_CONFIGSTRINGS 1400 and MAX_GAMESTATE_CHARS 16000 (gameState_t is copied into the cgame, q_shared.h:1348-1362).
- MAX_MODELS/MAX_SOUNDS 256 (8-bit fields).
- MAX_MSGLEN 16384 (qcommon.h:206), PACKET_BACKUP 32 (qcommon.h:160), MAX_RELIABLE_COMMANDS 128 (qcommon.h:168), MAX_PACKETLEN 1400.

Server-internal, safe to raise: the snapshot ring multiplier (64); MAX_SNAPSHOT_ENTITIES 1024 (sv_snapshot_entities.h:7, already >= MAX_GENTITIES); AREA_DEPTH/AREA_NODES; MAX_CHALLENGES 2048; MAX_TOTAL_ENT_LEAFS/MAX_ENT_CLUSTERS.

Admin-tunable cvars: sv_fps (default 20, CVAR_TEMP, no upper clamp, sv_init_lifecycle.h:116), sv_maxRate 90000 / sv_minRate 1000 (:62-63, clamped in SV_ClientRate sv_client_usercmd.h:196-209), sv_maxSnaps 30 clamped to sv_fps (sv_client_usercmd.h:211-219). Client defaults are rate 50000, snaps 30, cl_maxpackets 60 (cl_main.cpp:2881, 2922-2923).

WHAT 'MORE LOAD' CAN MEAN UNDER THE COMPATIBILITY CONSTRAINT:
- 32 slots in total, bots and spectators included.
- More action per slot: up to 1024 entities, but at most 256 seen per client.
- A higher tick: sv_fps 30-40, possibly 60. G_RunFrame and snapshot cost grow linearly; Pmove cost does not change. This needs a larger ring and rate >= ~40 x (snapshot size + 48) B/s.
- Bots up to 31. They cost a snapshot build every frame and consume ring space.
- More viewers only through new server-only infrastructure (server-side demos or a relay).
- More robustness under spikes: reliable-command overflow, oversize snapshots that sv_dynamicSnapshots handles by dropping all entities from that snapshot (sv_snapshot_sender.h:134-143).

**Already exists (do not reinvent):**
- sv_dynamicSnapshots (default 1): reliable commands are cut at the message limit and the snapshot is dropped on overflow instead of overflowing (sv_snapshot_writer.h:113-116, sv_snapshot_sender.h:134-155, CVARS.rst:638)
- sv_hibernateFps (default 4 in code): sleeps longer when no humans are connected (sv_main_frame.h:13-29, 83-97; common.cpp:2872). Joining or console input disables it (sv_client_connect.h:221-223, con_tty.cpp:370). Note it still simulates every game frame, just in batches
- sv_minSnaps / sv_maxSnaps / sv_enforceSnaps snapshot-rate control (sv_client_usercmd.h:211-229)
- sv_minRate / sv_maxRate per-client rate clamping (sv_client_usercmd.h:196-209). sv_minRate can already force low-rate vanilla clients up
- Leaky-bucket rate limiting for out-of-band packets (sv_maxOOBRate): per IP via std::map buckets plus per-command global buckets, with an IP whitelist (sv_autoWhitelist, whitelistip command, ipwhitelist.dat) that gets 2x burst (sv_main_ratelimit.h, sv_main_connectionless.h:298-335)
- sv_floodProtect leaky bucket for client commands (sv_client_commands.h:255-265)
- sv_pingFix: ping measured with Sys_Milliseconds, not sv_fps-quantized (sv_snapshot_sender.h:50, sv_client_usercmd.h:94-95)
- mv_httpdownloads / mv_httpserverport: HTTP download server that takes downloads off the netchan (sv_init_mapload.h:381-382, net_http.cpp)
- MVAPI per-entity snapshot filtering: snapshotIgnore/snapshotEnforce per client, MVF_NOSPEC/MVF_SPECONLY (sv_snapshot_entities.h:96-121, mvapi.h:99-106); GAME_MVAPI_PLAYERSNAPSHOT per-client VM hook (sv_snapshot_sender.h:195-205); submodel bypass; per-fix opt-outs through MVAPI_CONTROL_FIXES
- mv_fixturretcrash: caps EV_SABER_BLOCK temp entities at 100/s (sv_world_linking.h:174-186)
- LAN/loopback clients get a snapshot every server frame (sv_snapshot_sender.h:62-63); LAN rate is forced to 99999 (sv_client_userinfo.h:25-27)
- Packets are processed immediately while sleeping (NET_Sleep -> NET_Event, net_ip.cpp:1040-1075); com_busyWait for frame-time precision (common.cpp:2903-2909)
- Automatic map restart before svs.time or snapshot-ring counter wraparound (sv_main_frame.h:104-115)
- com_speeds (millisecond resolution, time_game) and the c_traces/c_brush_traces/c_patch_traces/c_pointcontents counters (common.cpp:2990-3016); 'sectorlist' command (sv_world_sectors.h:25-39)
- std::thread already in use (net_http.cpp), so C++11 threading is an accepted precedent
- gtest unit-test scaffolding for every sv_* module (tests/unit/*), though see the notes: the tests re-implement the functions instead of linking the engine code

### SV-1 — Server profiler with microsecond timers and per-phase counters, plus a load-test harness that runs the real code
*tooling · effort M · server-only-transparent*

Add an opt-in 'sv_speeds' cvar or 'svprofile' command that times each phase with a microsecond clock: packet processing including GAME_CLIENT_THINK (with per-client totals), GAME_RUN_FRAME, BotFrame, snapshot build, delta+Huffman encode, netchan send. Report per-frame and 1-second aggregates: average and max snapshot entities, bytes per client, rate-delayed snapshots, fragments sent, fallbacks to full snapshots ('out of date entities'), reliable-command queue high-water mark, traces and area queries per frame (reuse c_traces). Add per-client CPU (Pmove time) to 'status'. Pair this with a benchmark/test target that links the real sv_*.h/cm_*.cpp/msg.cpp code (headers included in the test TU with stubs) and replays recorded or synthetic usercmd and entity loads, so every later optimization can be measured and checked for identical output.

- **Player-visible effect:** None directly. Admins see where the server spends its time and why players lag (rate-delayed, fragmented or full snapshots).
- **Expected gain:** No gain by itself. It is the prerequisite for ranking SV-2..SV-10, whose estimates are currently ±3x guesses because nothing can be measured in this environment. High confidence it is needed.
- **Risk:** Very low: opt-in counters. Timer calls in the hot path cost a few ns each when disabled behind a flag.
- **Evidence:** `src/qcommon/common.cpp:2301-2318 (com_speeds uses Sys_Milliseconds; a server phase takes well under 1 ms)`; `src/server/sv_main_frame.h:146-162 (only time_game is measured)`; `src/qcommon/cm_load.cpp:34-35 and common.cpp:3009-3016 (trace counters already exist)`; `tests/unit/test_sv_world_area.cpp:61-107 (the test re-implements SV_AreaEntities_r instead of including the engine code)`; `no Sys_Microseconds or other high-resolution timer in src (grep)`

### SV-2 — Table-driven static Huffman encode/decode (bit-identical output)
*server-load · effort S · server-only-transparent*

The netchan Huffman tree is built once from msg_hData in MSG_initHuffman and never adapts afterwards. Yet every byte of every snapshot and gamestate is encoded by send(), which recurses up parent pointers one bit at a time through add_bit with the global 'bloc'. Decoding walks the tree bit by bit. Precompute per-symbol code and length tables at init time (Quake3e did this with static tables; check its huffman_static.c) and write each symbol with one or two 32-bit OR/shift operations. Decode with an 8-11 bit lookup table. The output stays bit-identical, so the wire format is unchanged. Clients also get faster snapshot parsing. This also removes the shared static 'bloc', which SV-8 needs.

- **Player-visible effect:** None directly. Frees CPU for higher sv_fps and snaps, and slightly lowers send latency.
- **Expected gain:** Encoding per byte should be ~5-10x faster: tree depth is 6-12 levels with a function call and branches per bit, versus one table lookup. Assuming Huffman takes ~20-40% of the snapshot phase, the whole server saves ~3-10% CPU at 32 players and 30-40 snaps. Medium confidence on the micro speedup (Quake3e precedent); low confidence on its share of total CPU until SV-1 measures it.
- **Risk:** Low with a golden test (random messages encoded by the old and new paths must be bit-identical, and so must decoding). Keep the adaptive Huff_Compress/Decompress used by other paths untouched.
- **Prerequisites:** SV-1 (golden/bench harness)
- **Evidence:** `src/qcommon/msg.cpp:157-177 (per-byte Huff_offsetTransmit, per-bit Huff_putBit)`; `src/qcommon/msg.cpp:213-231 (per-bit Huff_getBit / Huff_offsetReceive tree walk)`; `src/qcommon/huffman.cpp:9 (static int bloc), 270-301 (recursive send())`; `src/qcommon/msg.cpp:2592-2610 (tree built once from msg_hData)`

### SV-3 — Size the snapshot entity ring for high tick rates and bots (power-of-two, cvar-scaled)
*server-load · effort S · server-only-transparent*

svs.numSnapshotEntities = maxclients x 32 x 64 assumes an average of 64 visible entities per snapshot. Every built snapshot uses up entries, including one per bot per server frame. Under heavy load, the frame a client deltas from rolls off the ring and the client gets a full snapshot from baselines, which is several KB instead of a few hundred bytes. That pushes it into rate delay or fragmentation exactly when the action peaks. Make the multiplier a latched cvar (e.g. sv_snapshotEntityBudget, default 256 = MAX_ENTITIES_IN_SNAPSHOT), round the ring up to a power of two, and replace the per-entity '%' with a mask in the builder and the delta emitter.

- **Player-visible effect:** Fewer bandwidth spikes and stutters in big fights or on servers with sv_fps 40 or many bots.
- **Expected gain:** Ring history ~= 65,536 / (sum of visible entities over snapshots built per frame). Example: 32 clients/bots seeing ~200 entities at sv_fps 40 -> 6,400 per frame -> ~10 frames (~250 ms). Players above ~250 ms ping or with bursty loss get non-delta snapshots. Multiplier 256 gives ~1 s. Memory 19.4 MB -> 77.6 MB at 32 slots (entityState_t = 74 ints = 296 B; new[] allocation, not hunk). High confidence on the mechanism, medium on how often it happens today at sv_fps 20.
- **Risk:** Low. Only memory grows. Keep the wraparound restart in sv_main_frame.h:111-115 consistent.
- **Evidence:** `src/server/sv_init_clients.h:69, 141 (x64 multiplier)`; `src/server/sv_init_mapload.h:165 (allocation)`; `src/server/sv_snapshot_writer.h:33-41 (fallback to non-delta when entities rolled off)`; `src/server/sv_snapshot_sender.h:104-110 (bots build a snapshot every frame and never set nextSnapshotTime)`; `src/server/sv_snapshot_builder.h:96, sv_snapshot_delta.h:35,42 (modulo per entity)`

### SV-4 — Per-client snapshot entity budget with priority and graceful degradation instead of dropped snapshots
*server-load · effort M · server-only-transparent*

The cgame only accepts 256 entities. The client silently truncates the rest by highest entity number, after the server has already paid to send them. When a snapshot overflows MAX_MSGLEN, sv_dynamicSnapshots sends the message without any entity data, so the client freezes. Proposal: when more than 256 entities are visible, keep the client itself, players, SVF_BROADCAST and enforced entities, then the nearest ones, and drop the rest from this snapshot. Also add a byte budget, derived from rate and snapshotMsec, so the message never exceeds 16 KB. Low-priority entities whose delta does not fit are 'frozen': no delta is written and their old state is stored in the new ring frame, which the delta protocol already treats as unchanged. Behind an opt-in cvar, because it changes which entities appear under overload.

- **Player-visible effect:** In very crowded scenes (many missiles, effects, mod-spawned entities), distant clutter is dropped instead of whole snapshots, so no more freezes or 'read past end of server message' drops.
- **Expected gain:** No effect in normal play. Under overload, removes total snapshot loss and cuts bandwidth by the share of entities above 256 that the client would have discarded anyway. Medium confidence. Frequency depends on mods and maps; SV-1 counters will show it.
- **Risk:** Medium: priority bugs could hide relevant entities, and freezing must store exactly what the client holds. Opt-in, default off, so the classic behavior is the default.
- **Prerequisites:** SV-1
- **Evidence:** `src/client/cl_cgame.cpp:158-160 (truncate to MAX_ENTITIES_IN_SNAPSHOT 256)`; `src/cgame/cg_public.h:12`; `src/server/sv_snapshot_entities.h:7, 47-53 (server collects up to 1024)`; `src/server/sv_snapshot_sender.h:134-143 (on overflow, send the message without entities)`; `src/server/sv_snapshot_delta.h:52-58 (unchanged entities are not written)`

### SV-5 — Send queued fragments as soon as the rate allows (do not wait for the next server frame)
*server-load · effort M · server-only-transparent*

Messages of 1300 bytes or more are fragmented. Netchan_Transmit sends only the first fragment. The rest go out from SV_SendClientMessages, at most once per server frame, gated by nextSnapshotTime. A 16 KB gamestate (13 fragments) therefore takes at least 13 frames (~650 ms at sv_fps 20), and a 2-3 KB snapshot in a busy fight arrives 1-2 frames late. Adopt ioquake3's model: a per-client queue of outgoing messages/fragments, plus an SV_SendQueuedPackets() called from Com_Frame that returns the time until the next send, which bounds the NET_Sleep timeout.

- **Player-visible effect:** Faster connect and map changes, and less extra latency or hitching on large snapshots with many players.
- **Expected gain:** Each extra fragment arrives 1000/sv_fps ms sooner (50 ms at sv_fps 20), limited only by rate. Gamestate delivery drops from >=650 ms to ~180 ms at 90 KB/s. High confidence on the mechanism (ioq3 precedent); real impact depends on how many snapshots exceed 1300 bytes (SV-1).
- **Risk:** Low-medium: preserve the [ISM] flush-before-gamestate logic (sv_snapshot_sender.h:42-46, sv_client_gamestate.h:24-28) and pacing for CS_CONNECTED/downloading clients.
- **Evidence:** `src/qcommon/net_chan.cpp:26-27, 148-157`; `src/server/sv_snapshot_sender.h:178-192`; `src/qcommon/common.cpp:2869-2909 (sleep bounded only by SV_FrameMsec)`

### SV-6 — Coalesce configstring updates per frame (opt-in) to stop reliable-command overflow
*server-load · effort M · server-only-transparent*

Every SV_SetConfigstring immediately queues one 'cs' reliable command per client (bcs0/1/2 chunks for long strings). A mod that updates scores, CS_PLAYERS or serverinfo several times per frame fills the 128-slot reliable ring and drops clients with 'Server command overflow'. Mark the configstring dirty per client and emit only its latest value once, just before that client's next snapshot. This extends ioq3's csUpdated[] mechanism, which ioq3 uses only for CS_PRIMED clients. Either remove the dead SV_ReplacePendingServerCommands or reuse its idea. Make it a cvar, because it reorders 'cs' commands after other server commands issued in the same frame.

- **Player-visible effect:** Fewer disconnects on busy modded servers. A little less bandwidth.
- **Expected gain:** Reliable commands per frame for configstring traffic drop from (#updates) to (#distinct configstrings changed). Large for spammy mods, none for vanilla. Medium confidence.
- **Risk:** Medium for mods that send a server command that depends on a configstring set in the same frame. That is why it is opt-in and default off.
- **Evidence:** `src/server/sv_init_configstrings.h:37-77`; `src/server/sv_main_messaging.h:40-58 (SV_ReplacePendingServerCommands never called), 66-80 (overflow drop)`; `src/qcommon/qcommon.h:168 (MAX_RELIABLE_COMMANDS 128)`; `src/server/sv_main_frame.h:124-131 (serverinfo configstring rewritten whenever a SERVERINFO cvar changes)`

### SV-7 — Faster world queries: clip the trace's entity box to the world hit, doubly linked sector lists, optional deeper tree
*server-load · effort S · server-only-transparent*

(1) SV_Trace already knows the world fraction, but searches entities over the full start-to-end box (the comment says it clips, the code does not). Using the world trace endpos plus the existing ±1 padding gives identical results, because entities beyond the world hit cannot beat it under the strict '<'. This mostly helps long line-of-sight and hitscan traces from bots and the disruptor. (2) Store a prev pointer in svEntity_t (server-private, not ABI) so SV_UnlinkEntity is O(1) instead of a list scan on every relink. (3) Optionally make AREA_DEPTH configurable or split on Z for large or vertical maps. This changes the order in which SV_AreaEntities returns entities, which affects touch order and tie-breaking, so it should stay behind a flag.

- **Player-visible effect:** None.
- **Expected gain:** Small on typical maps (<2% CPU, guess). Up to ~2x faster area queries on maps or mods with 500+ linked entities and on long traces. Low-medium confidence; profile with SV-1 first.
- **Risk:** (1) and (2) low (identical results). (3) medium (result order), so behind a flag.
- **Prerequisites:** SV-1
- **Evidence:** `src/server/sv_world_trace.h:240-255 (comment versus full-length box)`; `src/server/sv_world_linking.h:135-147 (linear unlink), 302-319 (linked at first crossing node)`; `src/server/sv_world_sectors.h:14-15, 56-81 (depth 4, 16 leaves, X/Y only)`; `src/server/sv_world_area.h:361-394`

### SV-8 — Build and encode client snapshots in parallel after GAME_RUN_FRAME
*server-load · effort L · server-only-transparent*

Once the game frame has run, each client's snapshot build, delta encode and netchan send only read entity state. Spread SV_SendClientSnapshot over a std::thread pool. Prerequisites: SV-2 (removes the static Huffman 'bloc'); make msg.cpp's debug globals (gLastField, fieldIndex, oldsize) thread-local; replace the shared svEnt->snapshotCounter de-duplication with a per-task 1024-bit set; reserve ring ranges atomically (or per client); keep downloads and Com_Printf serial; fall back to serial when sv.vmPlayerSnapshots is set, because the mod's per-client callback can mutate entities. The game VM itself stays serial: syscalls and CM use global state.

- **Player-visible effect:** Steadier frame times and lower send latency on full servers with high snaps.
- **Expected gain:** The snapshot phase shrinks by roughly the thread count. Overall frame time maybe -10 to -30% when that phase is large (32 players, 30-40 snaps, many entities), near 0% for VM-bound loads. Low confidence until SV-1 shows the share of the snapshot phase.
- **Risk:** Medium-high: data races, and nondeterministic ordering of sends (harmless on the wire). Needs TSAN runs and a kill switch cvar.
- **Prerequisites:** SV-1; SV-2
- **Evidence:** `src/server/sv_snapshot_sender.h:172-206`; `src/server/sv_snapshot_entities.h:42-46, 151-153 (shared snapshotCounter)`; `src/server/sv_snapshot_builder.h:93-105 (shared ring cursor)`; `src/qcommon/msg.cpp:40-43 (globals), huffman.cpp:9`; `src/qcommon/cm_trace.cpp:1137 (global checkcount: CM not thread-safe)`; `src/qcommon/net_http.cpp:3,35 (std::thread precedent)`

### SV-9 — Snapshot-builder micro-optimizations, including visibility shared across clients
*server-load · effort M · server-only-transparent*

Low-risk bundle: (1) skip the qsort when no SVF_PORTAL was traversed (entities are already added in increasing order, and duplicates only come through portals); (2) build once per frame a compact list of candidate entities (linked and not SVF_NOCLIENT) instead of scanning up to sv.num_entities, the high-water mark, for every client; (3) group clients by (cluster, area) per frame and compute the PVS/area pass mask once per group, then apply the per-client filters (SINGLECLIENT, broadcastClients, MVAPI ignore/enforce/spec flags). Item (3) is what 'per-frame PVS caching' amounts to here, and the code suggests its value is small.

- **Player-visible effect:** None.
- **Expected gain:** The visibility loop is ~10-30 ns per entity, so 32 clients x 1024 slots x 20 snaps ~= 7-20 ms of CPU per second (~1-2% of a core). Bots add one pass each per frame. The qsort costs ~5-10 us per snapshot. Total saving ~1-3% of server CPU. Medium confidence that the gain is small; worth doing only alongside SV-8 or if profiling shows otherwise.
- **Risk:** Low for (1) and (2). Medium for (3): portal recursion and the MVAPI filters must keep exactly the same semantics.
- **Prerequisites:** SV-1
- **Evidence:** `src/server/sv_snapshot_builder.h:82-83 (qsort on every snapshot)`; `src/server/sv_snapshot_entities.h:76-221`; `src/server/sv_snapshot_sender.h:104-110 (bots every frame)`

### SV-10 — Faster QVM JIT for the game module (port Quake3e's optimizing x86/x64 compiler)
*server-load · effort XL · server-only-transparent*

vm_x86.cpp is ioquake3's 2011-era single-pass JIT (opStack in ebx). For mods shipped as .qvm (vm_game 2, the default), the game VM (Pmove per usercmd, G_RunFrame, bot AI) is very likely the largest share of server CPU. Quake3e rewrote this compiler with register caching, constant folding and fused compares, and keeps the same bytecode ABI. A port must keep jk2mv's VM contract: dataMask, the VM_ArgPtr/VM_ArgArray/VM_ArgString checks (vm.cpp:852-916), the MVAPI/1.02 struct conversion paths and the ARM fallback. The cgame/ui QVMs on the client benefit too. Native .so/.dll mods gain nothing.

- **Player-visible effect:** None directly. Leaves headroom for sv_fps 40-60, more bots, heavier mods.
- **Expected gain:** Quake3e reports noticeably faster QVM execution than ioq3's JIT (order 1.5-2x on VM-bound code; my recollection, low confidence). If the VM is ~50-70% of server CPU, the server would save ~20-40% CPU for QVM mods. Low confidence; needs SV-1 numbers and per-mod checks.
- **Risk:** Medium-high: JIT bugs crash or corrupt mods silently. Needs a QVM conformance suite (run the mvsdk game QVM in interpreter versus JIT and diff the state) and a cvar to fall back to the old JIT.
- **Prerequisites:** SV-1
- **Evidence:** `src/qcommon/vm_x86.cpp:1-80 (ioq3-derived JIT; register layout comment at 48-59)`; `src/qcommon/vm.cpp:76-77 (vm_game/vm_cgame default 2 = compiled)`; `src/server/sv_client_usercmd.h:45 and sv_main_frame.h:156 (VM entry points per usercmd and per frame)`

### SV-11 — Server-side demo recording (svrecord / auto-record every match per player)
*fun-feature · effort M · server-only-transparent*

Tap each client's snapshot message in SV_SendClientSnapshot (before SV_Netchan_Encode XORs it) and write it as a standard client demo (.dm_15/.dm_16: gamestate first, then snapshot messages), so any vanilla or jk2mv client can play it back. Add 'svrecord <client|all>' / 'svstoprecord' and an 'sv_autoDemo' cvar to auto-record matches. Buffer the writes and flush on a helper thread so file IO never blocks the frame. OpenJK (JKA) ships a comparable feature (svrecord/sv_autoDemo).

- **Player-visible effect:** Every duel or match can be replayed from the victim's or winner's point of view: highlight reels, tournament VODs, cheat reviews. A strong 'show-off' feature at very low cost.
- **Expected gain:** Adds features rather than speed. Cost ~= one memcpy plus a buffered write per sent snapshot (~30 KB/s per recorded client at 30 snaps), well under 5% of server CPU. Medium-high confidence.
- **Risk:** Low-medium: disk usage (needs rotation limits) and privacy (announce recording via a serverinfo flag). Bots are skipped or recorded through the built snapshot.
- **Evidence:** `src/server/sv_snapshot_sender.h:98-165 (per-client message assembled here)`; `src/server/sv_netchan_transmit.h:20-24 (encode happens right before transmit)`; `src/server/sv_client_gamestate.h:18-67 (gamestate layout to replay at demo start)`; `no server-demo code in src (grep for svrecord/sv_demo/SV_Record finds nothing)`

### SV-12 — Relay/broadcast mode ('MVTV') so spectators can go beyond the 32 slots
*fun-feature · effort XL · server-only-transparent*

A jk2mv dedicated server mode that connects to a game server as a single spectator client (one slot), keeps its incoming snapshots and server commands, and re-serves them to N vanilla clients connected to the relay. Viewers get the stream exactly as in demo playback (the relay assigns them the relay's clientNum and rewrites gamestate, sequences and deltas per viewer), with optional delay against ghosting. The relay could later switch the followed player through console commands sent upstream. Precedents: QuakeWorld QTV/MVD and ETPro's ETTV. The game server only sees one more client; players and mods are unaffected.

- **Player-visible effect:** Hundreds of viewers for tournaments or events without taking player slots, a real esports-style capability for a 2002 game.
- **Expected gain:** Viewer capacity goes from <=31 to hundreds per relay (bandwidth-bound: ~30-50 KB/s per viewer). The game server pays the cost of one client. Medium confidence on feasibility (it is demo playback over the network), low on effort accuracy.
- **Risk:** High: per-viewer delta/sequence bookkeeping, reliable-command forwarding, map changes and pure checks. Should live as a separate server mode or binary.
- **Prerequisites:** SV-11 (shares the demo/message plumbing)
- **Evidence:** `src/qcommon/q_shared.h:1321 (MAX_CLIENTS 32)`; `src/api/mvapi.h:102-106 (MVAPI per-client arrays sized 32)`; `src/server/sv_init_clients.h:38-49 (SV_BoundMaxClients clamp)`; `src/client/cl_parse.cpp (client already parses the full snapshot stream; reusable for the upstream link)`

### SV-13 — Hardening against abusive load: optional usercmd rate cap, per-client CPU accounting, throttled 'disconnect' replies
*server-load · effort S · server-only-transparent*

A client can send up to 32 usercmds per packet with serverTime advancing 1 ms, which triggers up to ~1000 GAME_CLIENT_THINK/Pmove runs per second, about 8x a 125 fps player. mv_blockspeedhack only zeroes the roll angle. Add: (a) per-client accounting of usercmds/s and time spent in GAME_CLIENT_THINK, exposed in status and the profiler; (b) an opt-in sv_maxUsercmdRate that drops or merges commands above e.g. 500/s. The default stays off, because JK2 jump physics depend on fps, and the cap would be set well above legitimate 125-333 fps. (c) Rate-limit the out-of-band 'disconnect' sent to every unknown sequenced packet with the existing SVC_RateLimitAddress, which removes a free reflection vector.

- **Player-visible effect:** Servers stay smooth when a misbehaving or malicious client floods commands.
- **Expected gain:** Caps worst-case per-client Pmove CPU at a configurable level (from ~1000/s); no effect on normal players. High confidence on the mechanism, unknown how often it is abused in the wild.
- **Risk:** Low with the default off. If set too low, it would change movement for high-fps players.
- **Prerequisites:** SV-1 (for the per-client numbers)
- **Evidence:** `src/server/sv_client_usercmd.h:72-80, 118-131, 45`; `src/server/sv_game_communication.h:71 (mv_blockspeedhack only clears ROLL)`; `src/server/sv_main_packets.h:66-69 (unthrottled OOB disconnect)`; `src/server/sv_main_ratelimit.h:84-88 (SVC_RateLimitAddress available)`

### SV-14 — Tick/rate presets plus sv_fps clamp and fixes to hibernation docs and behavior
*tooling · effort S · server-only-transparent*

(1) Clamp sv_fps to a sane range, e.g. 10..125. Above 1000, 1000/sv_fps is 0 and 'while (sv.timeResidual >= frameMsec)' loops forever, hanging the server. (2) Document or ship an opt-in 'competitive' preset: sv_fps 40 (or 30), sv_maxSnaps = sv_fps, sv_enforceSnaps 1, sv_minRate 25000-50000 so vanilla 1.02/1.04 clients with low default rates are not rate-starved, and sv_maxRate >= ~60000, since 40 snaps x (~1-1.5 KB + 48 B) must fit the rate or SV_RateMsec delays snapshots. Combine with SV-3 and list the cost: ~1.6-2x server CPU for G_RunFrame and snapshots (Pmove unchanged) and ~1.5-1.8x upstream bandwidth. Note that mods may assume 50 ms frames, so test per mod. (3) CVARS.rst documents sv_hibernateTime (not implemented) and sv_hibernateFps default 5 (the code uses 4). Either implement sv_hibernateTime as documented (delay before hibernating) or fix the docs.

- **Player-visible effect:** With the preset, smoother hit registration and movement (40 updates/s instead of 20). The default stays classic.
- **Expected gain:** Interpolation and latency granularity drop from 50 ms to 25 ms. The hang footgun is removed. High confidence on the mechanics. CPU and bandwidth multipliers from the code, medium confidence.
- **Risk:** Low. The preset is opt-in; a clamp could break someone running sv_fps >125 on purpose (allow up to 1000 if needed).
- **Prerequisites:** SV-3
- **Evidence:** `src/server/sv_main_frame.h:74-79, 150-158 (no upper clamp; frameMsec can be 0)`; `src/server/sv_init_lifecycle.h:60-64, 107, 116`; `src/server/sv_client_usercmd.h:196-219`; `src/server/sv_snapshot_sender.h:17-27, 66-76 (rate math)`; `CVARS.rst:565-583 (sv_hibernateTime / default 5) vs sv_init_lifecycle.h:107 (default 4, no hibernateTime)`; `src/client/cl_main.cpp:2922-2923 (jk2mv client defaults rate 50000, snaps 30)`

### SV-15 — Engine-provided entity history ('rewind') service for mod-side lag compensation (fork API)
*fun-feature · effort L · needs-vm-or-mod-changes*

Keep a short ring of per-entity origins and bboxes per server frame (~1 s), and expose a fork-specific syscall range, not an edit to mvapi.h (which forbids it), e.g. trap_JKMV_TraceRewound(time, ...), which traces against entities as they were at a client's view time. Mods that adopt it get unlagged hitscan (disruptor, bryar) without each reimplementing history. Engine-side cost is a memcpy of linked entity boxes per frame.

- **Player-visible effect:** Hits land where you aimed even at 100+ ms ping, in mods that use the API.
- **Expected gain:** Only gameplay feel, and only for adopting mods. Engine cost <1% CPU (1024 x ~40 B per frame). Medium confidence on cost, low on adoption.
- **Risk:** Medium: API design and versioning; mods must opt in, and vanilla behavior is unchanged.
- **Evidence:** `src/api/mvapi.h:17-22 (forks must define their own API)`; `src/server/sv_world_trace.h:209-258 (trace path to wrap)`; `src/server/sv_game_syscalls.h:704-748 (MVAPI-level syscall dispatch pattern to mirror)`

**Notes.** Confidence and method: read-only static analysis. There is no GPU, no assets and no running server here, so every CPU share above is inferred from loop structure and per-operation costs, and from idTech3 history (32-player servers ran on ~1 GHz CPUs). Treat any percentage as ±3x until SV-1 exists. Most likely cost ranking for 32 humans: game VM (Pmove per usercmd, then G_RunFrame/saber/bots) > Huffman+delta encoding > traces' entity phase > visibility pass > bookkeeping. Recommended order: SV-1 -> SV-3 + SV-14 (cheap and immediate) -> SV-2 -> SV-5 -> SV-4/SV-6 (opt-in) -> SV-11 (the 'mouth-watering' server feature) -> SV-13 -> SV-7/SV-9 -> SV-8 -> SV-10/SV-12/SV-15.

Hard answers:
- Players are capped at 32 by protocol and ABI (cgame/game arrays, MVAPI [32] arrays, CS_PLAYERS range). Bots and spectators count against those 32.
- Entities are capped at 1024 (10-bit numbers). Each client sees at most 256 (cgame snapshot_t).
- MAX_CONFIGSTRINGS 1400 / 16000 gamestate chars, MAX_MSGLEN 16384, PACKET_BACKUP 32, MAX_RELIABLE_COMMANDS 128, 256 models/sounds and MAX_PACKETLEN 1400 must not change.
- sv_fps 30-40 is the realistic 'more load' lever. Do it with SV-3 and adequate rates; mods may assume 50 ms frames, so verify per mod.
- Multithreading the game simulation is impossible without mod changes (synchronous VM syscalls, global CM state). Only the post-frame snapshot phase (SV-8) and off-thread IO (SV-11) can be parallelized. Running several server processes per host remains the multi-core story.

Upstream quirks found, left as they are because changing them would alter behavior:
(1) In sv_snapshot_entities.h:194-202 the overflow-cluster test 'if (l == svEnt->lastCluster) continue;' is off by one (inherited from Q3/ioq3). Entities touching more than 16 clusters are treated as visible unless only their last cluster is visible, which over-sends large entities.
(2) SV_PointContents computes 'angles' but passes hit->s.origin/s.angles (sv_world_trace.h:286-291).
(3) sv_fps > 1000 hangs the server (see SV-14).
(4) Hibernation still runs every game frame (sv_main_frame.h:150-158), only batched, so its CPU saving is mostly fewer wakeups.

Test caveat: tests/unit/* re-implement copies of the server functions (e.g. test_sv_world_area.cpp:61-107) instead of linking src/server, so they do not guard the real code against perf refactors. SV-1 should add tests and benchmarks that include the real headers.

Doc drift: CVARS.rst:565-583 (sv_hibernateTime, sv_hibernateFps default 5) does not match the code (sv_init_lifecycle.h:107, default 4, no sv_hibernateTime).

Key files: /home/user/jk2mv/src/server/sv_main_frame.h, /home/user/jk2mv/src/server/sv_snapshot_entities.h, /home/user/jk2mv/src/server/sv_snapshot_builder.h, /home/user/jk2mv/src/server/sv_snapshot_sender.h, /home/user/jk2mv/src/server/sv_snapshot_writer.h, /home/user/jk2mv/src/server/sv_snapshot_delta.h, /home/user/jk2mv/src/server/sv_init_clients.h, /home/user/jk2mv/src/server/sv_init_configstrings.h, /home/user/jk2mv/src/server/sv_main_messaging.h, /home/user/jk2mv/src/server/sv_world_trace.h, /home/user/jk2mv/src/server/sv_world_linking.h, /home/user/jk2mv/src/server/sv_world_sectors.h, /home/user/jk2mv/src/server/sv_client_usercmd.h, /home/user/jk2mv/src/qcommon/msg.cpp, /home/user/jk2mv/src/qcommon/huffman.cpp, /home/user/jk2mv/src/qcommon/net_chan.cpp, /home/user/jk2mv/src/qcommon/common.cpp, /home/user/jk2mv/src/qcommon/vm_x86.cpp.

## Engine-side features players see or feel (demos, camera, video and screenshot capture, HUD overlays, input and window, console and chat, server browser, visual presets), without changing the protocol or the VM ABI

**Current state.** What players get today is the standard idTech3/JK2 feature set plus the jk2mv quality-of-life additions. Upstream mvdevs/jk2mv (HEAD 7d60145, June 2025) registers the same cvars as this fork, so none of the features below exist upstream either.

DEMOS. Playback is linear only.
- CL_ReadDemoMessage reads the file in order (src/client/cl_main.cpp:499-545).
- CL_PlayDemo_f opens dm_15 and dm_16 files but throws away the file length that FS_FOpenFileRead returns (cl_main.cpp:562-650, at :588 and :606).
- There is no seek, rewind, free camera or timeline.
- Time control only exists as raw cvars:
  - timescale is CVAR_CHEAT but can be changed during demo playback (src/qcommon/cvar.cpp:422);
  - timescale 0 or opening the ESC menu pauses (src/qcommon/common.cpp:2819-2821);
  - cl_freezeDemo freezes cl.serverTime (src/client/cl_cgame.cpp:1609).
- Frame time (msec) is an int, so very low timescales get quantized (common.cpp:2774-2790).
- Rewinding is blocked twice:
  - backwards time is a fatal error (cl_cgame.cpp:1601-1602);
  - the world map cannot be loaded a second time (src/renderer/tr_bsp.cpp:2016-2017).
- In demos, the cgame tolerates reliable commands that were cycled out (cl_cgame.cpp:475-480). The buffers allow up to PACKET_BACKUP 32 snapshots and MAX_RELIABLE_COMMANDS 128 commands between cgame calls (src/qcommon/qcommon.h:160,168).
- cl_autoDemo keeps a rolling LastDemo; saveDemo and saveDemoLast write the saveDemo timestamps into the file name (src/client/cl_demos_auto.cpp:128-238).

VIDEO.
- The video command only works during demo playback (cl_main.cpp:136-145).
- Capture runs at a fixed timestep and carries the leftover fraction from frame to frame (cl_main.cpp:2455-2469).
- Output is AVI with MJPEG (quality 90) or raw BGR video, plus PCM audio. Files split at 2 GB. The writer handles FIFO output on non-MSVC builds (src/client/cl_avi.cpp:368-500; src/qcommon/files.cpp:1122-1140).
- Each frame is a synchronous glReadPixels (src/renderer/tr_backend.cpp:1366-1388), then in-process JPEG encoding (src/renderer/tr_cmds.cpp:555-600). The Windows bundle ships libjpeg 9a, not libjpeg-turbo (libs/jpeg-9a).
- Audio is mixed per video frame (src/client/snd_dma.cpp:2337-2348).
- Bug: the audio-capture check tests a cvar named s_backend that does not exist (cl_avi.cpp:433; also present upstream). jk2mv uses s_UseOpenAL instead (snd_dma.cpp:329).

SCREENSHOTS.
- JPEG (r_screenshotJpegQuality 95), TGA and levelshot only (src/renderer/tr_init.cpp:783-884).
- No PNG writer, even though libpng is already linked for loading textures (tr_image.cpp:2206-2426).
- The renderer uses no FBO and no PBO anywhere.

ENGINE OVERLAYS.
- Only the demo-recording indicator (src/client/cl_scrn.cpp:305-349, drawn at :489) and the cheat-protected debuggraph/timegraph (cl_scrn.cpp:403-407).
- cl_framerate only prints to the console (cl_main.cpp:2484-2495).

VIEW HOOKS. Everything an engine-side camera would need is already in place:
- the cgame refdef reaches the renderer unchanged (cl_cgame.cpp:855-857 to src/renderer/tr_scene.cpp:390-500);
- the sound listener goes through CG_S_RESPATIALIZE (cl_cgame.cpp:795);
- the FX system's view comes from CG_FX_ADJUST_TIME (cl_cgame.cpp:1067, src/client/FxSystem.cpp:48);
- the renderer filters RF_FIRST_PERSON and RF_THIRD_PERSON entities (tr_main.cpp:1179,1198) and applies an areamask (tr_scene.cpp:419-437);
- usercmds are still generated during demo playback (src/client/cl_input.cpp:1002-1003).

POST-PROCESSING. The default r_gammamethod 2 (src/sdl/sdl_window.cpp:1019) already runs a full-screen ARB program every frame that samples a 64^3 3D LUT (tr_backend.cpp:1305-1352). The LUT is filled in tr_image.cpp:2973-3001 and the pass is issued at tr_cmds.cpp:452-456. That is a free slot for color grading.

VISUAL DEFAULTS are conservative:
- r_picmip 1, bilinear filtering, anisotropic 2;
- dynamic glow off, with a fixed 320x240 glow buffer;
- MSAA off (tr_init.cpp:1073-1126; sdl_window.cpp:1017).
Two traps for presets:
- r_flares does nothing: the RB_RenderFlares call is commented out (tr_backend.cpp:712);
- cg_shadows 2 is forced back to 1 outside developer mode (tr_backend.cpp:438-441).

INPUT AND WINDOW.
- Raw mouse via in_mouse 2/3 (src/sdl/sdl_input.cpp:477-505).
- Joysticks use the legacy SDL_Joystick API with a fixed axis mapping (sdl_input.cpp:398-456, 1037-1083; cl_input.cpp:482-512).
- The bundled Windows SDL is 2.0.9 (libs/SDL2/include/SDL_version.h:61-62).
- Fullscreen is exclusive only (sdl_window.cpp:668-679), with a live toggle (sdl_window.cpp:340-362). Borderless needs r_noborder plus r_mode -2.

CONSOLE. Emacs-style editing, undo, paste and completion (src/client/cl_keys.cpp:471-801). Command history is in memory only (cl_keys.cpp:1199-1204).

UI.
- The main menu runs jk2mvmenu, which the engine ships and builds from the mvsdk submodule (src/client/cl_ui.cpp:1217-1238).
- The engine patches the original menus with hash-checked .menu_patch files, loaded only from assetsmv.pk3 (src/qcommon/files.cpp:1338-1341, src/botlib/l_script.cpp:1460-1484). This is how the MV Options page got into the retail menus without VM changes (assets/ui/jk2mp/setup.menu_patch:896-1127).

VM API. mvapi.h tells forks not to modify it and to define their own API (src/api/mvapi.h:18-22; MV_APILEVEL 3 at :31). Level-4 syscall handlers exist but can never be reached (cl_cgame.cpp:1322, cl_ui.cpp:1163); upstream is the same.

**Already exists (do not reinvent):**
- cl_autoDemo / cl_autoDemoFormat with saveDemo and saveDemoLast commands, rolling LastDemo and %t timestamp tokens (src/client/cl_demos_auto.cpp:28-238, CVARS.rst)
- cl_drawRecording 0/1/2 recording indicator (src/client/cl_scrn.cpp:305-349)
- video / stopvideo AVI recording from demos with cl_aviFrameRate, cl_aviMotionJpeg, cl_aviMotionJpegQuality, cl_forceavidemo, PCM audio, 2 GB auto-split and FIFO output on Unix (src/client/cl_avi.cpp, src/qcommon/files.cpp:1122)
- Demo pause via ESC menu or timescale 0 (src/qcommon/common.cpp:2819), timescale editable during demos despite CVAR_CHEAT (src/qcommon/cvar.cpp:422), cl_freezeDemo (src/client/cl_cgame.cpp:1609), demo name completion (cl_main.cpp:2976)
- timedemo 1 benchmark (average fps only) and activeAction scripting (src/client/cl_main.cpp:478-492, cl_cgame.cpp:1552-1558)
- screenshot [silent|levelshot|name] (JPEG, r_screenshotJpegQuality 95) and screenshot_tga (src/renderer/tr_init.cpp:783-884)
- Raw mouse input in_mouse 2/3, m_filter, cl_mouseAccel (src/sdl/sdl_input.cpp:477-505)
- Legacy joystick support in_joystick, in_joystickNo, in_joystickUseAnalog, joy_threshold with JOY0-31/AUX0-31 key codes remapped per game version (src/sdl/sdl_input.cpp:398-456, src/client/cl_keys.cpp:2180-2215)
- r_mode -2 desktop resolution, r_customwidth/height, r_noborder, r_centerWindow, live r_fullscreen toggle, minimize command and Windows-key minimizer, r_highdpi (src/sdl/sdl_window.cpp)
- Post-processing gamma r_gammamethod 2 with a 64^3 3D LUT (src/renderer/tr_backend.cpp:1305-1352)
- r_dynamicGlow plus r_DynamicGlowPasses/Delta/Intensity/Soft/Width/Height, r_saberGlow, r_ext_multisample, r_ext_texture_filter_anisotropic, r_textureLODBias, r_openglMipMaps, r_fontSharpness, r_consoleFont, con_scale/con_height/con_opacity/con_timestamps
- r_we weather command (snow/rain/fog/wind) that works client-side on any map (src/renderer/tr_WorldEffects.cpp:2173-2253)
- cgame-side HUD and camera options in base/MVSDK cgame: cg_drawFPS, cg_lagometer, cg_drawTimer, cg_thirdPerson*, cg_smoothCamera, cg_widescreen, cg_fovAspectAdjust (MVSDK only; src/mvsdk/code/cgame/cg_view.c:1064) and the DF_FIXED_FOV dmflag
- MVAPI virtual screen for widescreen HUDs and MVAPI_CONTROL_FIXES (src/client/cl_cgame.cpp:1295-1330)
- HTTP downloads with permission popup (mv_httpdownloads, mv_httpserverport, mv_allowDownload)
- Multi-master server browser (sv_master1-8, globalservers), mv_slowrefresh, MVSORT_CLIENTS_NOBOTS sorting
- Hash-checked .menu_patch mechanism from assetsmv.pk3 and the MV Options page (autodemo, demo keys, glow, console font) in the retail setup menus (assets/ui/jk2mp/setup.menu_patch)
- Console editing: emacs keys, undo/redo, yank, clipboard paste, command and cvar completion, 128-line history (src/client/cl_keys.cpp:471-801)
- s_muteWhenUnfocused/Minimized and com_maxfpsUnfocused/Minimized
- mv_* server-side exploit fixes and mv_coloredTextShadows / mv_nameShadows

### PX-1 — One-command graphics presets (classic / modern / competitive / movie) built only from existing cvars
*visual-wow · effort S · client-only-opt-in*

Add an engine command `preset <name>`. It applies a table of existing cvars and does a single vid_restart. Add a 'Graphics preset' item to MV Options with the same assetsmv menu_patch mechanism already used for setup.menu.
- modern: r_picmip 0 (default 1), r_textureMode GL_LINEAR_MIPMAP_LINEAR (default bilinear), r_ext_texture_filter_anisotropic 16 (default 2), r_ext_multisample 4, r_dynamicGlow 1 with r_DynamicGlowWidth/Height set to about vidWidth/4 x vidHeight/4 (default is a fixed 4:3 320x240 even on 16:9), r_subdivisions 1 plus r_lodCurveError 10000 for smoother curved surfaces, r_texturebits 32, s_khz 44.
- competitive: in_mouse 2, glow off, r_swapInterval 0.
- movie: modern plus cl_aviFrameRate 60 and cl_aviMotionJpegQuality 95.
- classic: Cvar_Reset on exactly the same list, so the defaults never change.
Deliberately left out:
- com_maxfps: 125 is tied to JK2 movement physics.
- snaps, rate, cl_timeNudge: network settings, not visuals.
- r_flares: it does nothing, because the RB_RenderFlares call is commented out.
- cg_shadows 2: the renderer forces it back to 1 outside developer mode because stencil shadows show through walls.
- cg_shadows 3 (planar shadows): a candidate, but check it visually first.
The command prints the list of changes it made.

- **Player-visible effect:** One click gives a 'remastered' look: full-resolution textures, trilinear + 16x anisotropic filtering, anti-aliased edges, smoother curves, glowing sabers. 'Classic' brings back the retail look.
- **Expected gain:** Large visible change with almost no code risk, since every cvar already exists and ships today. Performance: the GL1 path is CPU/draw-call bound on any GPU from the last ~10 years, so anisotropic filtering, trilinear and picmip 0 should be close to free. MSAA 4x plus 5 glow passes might cost 5-20% fps on integrated GPUs. These are estimates from fixed-function fill cost (medium confidence, not measured).
- **Risk:** Low. Values need a check on weak GPUs. Choosing a preset overwrites the user's own settings for those cvars, which is intended; print what changed.
- **Evidence:** `src/renderer/tr_init.cpp:1073 (anisotropic default 2), :1075 (r_DynamicGlow 0), :1080-1081 (glow buffer 320x240, latched), :1083 (r_picmip 1), :1097 (r_subdivisions 4), :1112 (r_lodCurveError 250), :1126 (bilinear r_textureMode)`; `src/sdl/sdl_window.cpp:1017 (r_ext_multisample 0), :1008 (r_mode -2)`; `src/renderer/tr_backend.cpp:712 (RB_RenderFlares commented out, so r_flares is dead)`; `src/renderer/tr_backend.cpp:438-441 (cg_shadows 2 forced to 1 unless developer)`; `src/sys/sys_main.cpp:94 (com_maxfps 125)`; `src/client/snd_dma.cpp:300 (s_khz 22)`; `assets/ui/jk2mp/setup.menu_patch:896-1127 (MV Options items added by patch)`; `src/qcommon/files.cpp:1338-1341 (patches only from assetsmv.pk3)`

### PX-2 — Color grading through the existing post-process gamma 3D LUT
*visual-wow · effort S · client-only-opt-in*

With r_gammamethod 2 (the default), every frame already goes through a full-screen ARB fragment program that looks up a 64^3 RGB 3D LUT; R_SetColorMappings fills it from r_gamma and overbright. Fold an optional creative grade into that same LUT:
- `r_colorGrade`: 0 = off, plus named built-ins such as cinematic teal/orange, vibrant, noir, holo blue.
- `r_colorLUT <image>`: load a standard 512x512 or 4096x64 LUT strip in PNG or TGA (the format ReShade and Unreal use), so artists can make grades in any photo editor.
- `r_colorGradeSplit 1`: grade only half the screen, for before/after showcase videos.

- **Player-visible effect:** Film-like color moods that switch live, and a built-in before/after split view for videos.
- **Expected gain:** No extra per-frame GPU cost on the default path: same pass, same single 3D texture fetch. Only a one-off 786 KB LUT upload when the setting changes. High confidence, from reading the code path. Not available with r_gammamethod 0 or 1.
- **Risk:** Low. The pass runs after 2D drawing, so the HUD and menus get graded too; fine for v1, but grading only the 3D view would need an extra screen copy. The LUT must be rebuilt on vid_restart. Default stays 0.
- **Evidence:** `src/renderer/tr_image.cpp:2973-3001 (64^3 LUT built and uploaded)`; `src/renderer/tr_backend.cpp:1305-1352 (RB_GammaCorrection full-screen pass)`; `src/renderer/tr_shader.cpp:4155-4170 (ARB program: TEX scene, TEX 3D LUT)`; `src/renderer/tr_cmds.cpp:452-456 (pass issued every frame when postprocessing)`; `src/sdl/sdl_window.cpp:1019 (r_gammamethod default 2)`; `src/renderer/tr_image.cpp:2206-2426 (PNG/TGA loaders already present)`

### PX-3 — Opt-in Hor+ widescreen FOV correction for mods without their own
*visual-wow · effort S · client-only-opt-in*

The base 1.02-1.04 cgame computes fov_y from fov_x and the real pixel aspect, so 16:9 and 21:9 screens lose vertical view ('Vert-'). Only MVSDK-based mods have cg_fovAspectAdjust. The engine already sees every refdef in CG_R_RENDERSCENE. When `cl_fovAspectFix 1`, it would treat fov_x as a 4:3 value and rebuild fov_x/fov_y for the real aspect, only for world scenes (not RDF_NOWORLDMODEL HUD models or hyperspace). Turn it off automatically when the server sets DF_FIXED_FOV in dmflags, and print a hint if the mod already has cg_fovAspectAdjust.

- **Player-visible effect:** Widescreen players see the vertical view a 4:3 player would, plus more on the sides, instead of a zoomed-in picture.
- **Expected gain:** Visual and comfort improvement with no performance cost. Confidence high on the math, which is the same formula as mvsdk cg_view.c:1064-1078.
- **Risk:** Medium. Combined with an MVSDK mod's cg_fovAspectAdjust the correction is applied twice, so document it. Some admins may see a wider view as an advantage; respecting DF_FIXED_FOV handles that. Zoom scopes scale the same way.
- **Evidence:** `src/client/cl_cgame.cpp:855-857 (CG_R_RENDERSCENE pass-through)`; `src/renderer/tr_scene.cpp:406-407 (fov copied verbatim)`; `src/mvsdk/code/cgame/cg_view.c:985-990 (DF_FIXED_FOV), :1064-1078 (cg_fovAspectAdjust exists only in MVSDK)`; `src/mvsdk/code/cgame/cg_draw.c:388 and cg_view.c:1328 (RDF_NOWORLDMODEL marks HUD and hyperspace scenes)`

### PX-4 — Engine-side free-fly camera for demos (works with every mod and every 1.02/1.03/1.04 demo)
*fun-feature · effort M · client-only-opt-in*

Add a `demo_freecam` toggle (and cl_freecamSpeed). It only works while clc.demoplaying.
- Driving the camera: the camera is driven by the usercmds the engine already builds during demo playback (angles plus forward/right/up move), so every existing bind, mouse and joystick works with no new input code.
- What gets overridden while it is active:
  - vieworg, viewaxis and fov of world scenes in CG_R_RENDERSCENE;
  - the FX helper view (CG_FX_ADJUST_TIME), so particle culling follows the camera;
  - the listener in CG_S_RESPATIALIZE, so sound is heard from the camera.
- Renderer changes: an internal flag makes it draw RF_THIRD_PERSON entities (the recorder's own body) and skip RF_FIRST_PERSON ones (the view weapon), and clears the areamask so areas behind closed doors still render. The renderer is statically linked, so this internal interface can change freely.
Limitation: you only see entities the recorder's snapshots contained, the same as any cgame-based freecam.

- **Player-visible effect:** Fly around any old or new demo freely, see the duel from the side, orbit a saber lock, look at the recorder's own character.
- **Expected gain:** Fills the biggest gap in JK2 movie tools without jaMME/q3mme-style forks, which are JKA/Q3-only. No measurable performance cost (one refdef copy per frame). Feasibility confidence medium-high: all hook points were found in the code.
- **Risk:** Low-medium. Some mods may render more than one world scene per frame (picture-in-picture); override only full-viewport scenes without RDF_NOWORLDMODEL. Must never be active in live play (that would be a wallhack), so it is gated on clc.demoplaying.
- **Prerequisites:** PX-6 recommended so the recorder's HUD can be hidden
- **Evidence:** `src/client/cl_input.cpp:1002-1003 (usercmds created even when a demo is playing)`; `src/client/cl_cgame.cpp:855-857 (render scene hook), :795-797 (sound listener hook), :1067-1068 (FX view hook)`; `src/client/FxSystem.cpp:48 and src/client/FxScheduler.cpp:1098 (FX distance culling uses that view)`; `src/renderer/tr_main.cpp:1179-1199 (RF_FIRST_PERSON / RF_THIRD_PERSON filtering)`; `src/renderer/tr_scene.cpp:419-437 (areamask from cgame)`; `src/mvsdk/code/cgame/cg_players.c:3332 (own body added with RF_THIRD_PERSON in first person)`

### PX-5 — Camera paths and keyframes for demos (cinematic fly-throughs)
*fun-feature · effort M · client-only-opt-in*

Builds on the PX-4 camera:
- Commands: `cam_add` (records position, angles, fov and roll at the current demo serverTime), `cam_del`, `cam_clear`, `cam_play`, `cam_save <name>` / `cam_load` (a small text file next to the demo, e.g. demos/<demo>.cam).
- Interpolation: Catmull-Rom/Hermite for position, quaternion slerp/squad for orientation, smoothstep for fov.
- Optional timescale keyframes for speed ramps (slow-motion into a kill, then back to normal).
- Preview: an engine overlay draws the path and the keyframes.
- Because playback is locked to demo serverTime, AVI or FFmpeg export is reproducible frame for frame.
- The spline and quaternion math lives in a pure C++ unit that the existing gtest suite can test.

- **Player-visible effect:** Trailer-style moving shots of any duel, synced to the action, ready to export as a video.
- **Expected gain:** This is the centerpiece of the community 'wow' video. Same feature family as jaMME/q3mme, which are the standard tools for JKA/Q3 frag movies (high confidence they are valued). No performance cost.
- **Risk:** Low. Pure client and demo-only. Timescale keyframes are limited to about 1 ms of game time per frame (see PX-7 and PX-23).
- **Prerequisites:** PX-4; PX-7 for timescale control
- **Evidence:** `src/client/cl_cgame.cpp:1609 (cl_freezeDemo keeps serverTime still for keyframing)`; `src/client/cl_cgame.cpp:1440-1442 (one cgame frame per engine frame at cl.serverTime)`; `src/client/cl_main.cpp:2455-2469 (fixed-step capture timing used by exports)`; `tests/unit (existing gtest infrastructure)`

### PX-6 — Clean feed: hide the HUD in demos, photos and video without relying on mod cvars
*fun-feature · effort S · client-only-opt-in*

When `cl_demoHideHud 1` (demo playback only, or the screenshot/photo flow), the engine drops the cgame's 2D draws: CG_R_DRAWSTRETCHPIC, CG_R_DRAWROTATEPIC(2), CG_R_FONT_DRAWSTRING, CG_R_SETCOLOR and RDF_NOWORLDMODEL scenes such as 3D HUD icons. It also hides its own notify lines and the recording indicator. This works with every mod, because not all mods honor cg_draw2D the same way.

- **Player-visible effect:** Clean cinematic frames: no health bars, chat or crosshair.
- **Expected gain:** Needed for clean screenshots and videos; saves a small amount of draw time. High confidence: all of these draws go through the engine syscall switch.
- **Risk:** Low. A few mods may draw world-space elements (name tags, saber trails) as polys, which stay visible. That is acceptable.
- **Evidence:** `src/client/cl_cgame.cpp:822-824 (font draw), :861-863 (stretch pic), :869-874 (rotate pics), :858-860 (setcolor)`; `src/client/cl_scrn.cpp:487-490 (cgame rendering then recording indicator)`; `src/mvsdk/code/cgame/cg_draw.c:388 (3D HUD icons use RDF_NOWORLDMODEL)`

### PX-7 — Demo transport controls, on-screen timeline and smoother slow motion
*fun-feature · effort S · client-only-opt-in*

Commands that can be bound to keys:
- `demo_pause`: wraps timescale 0, which already pauses demos;
- `demo_speed <x>` / `demo_faster` / `demo_slower`: timescale is already writable in demos;
- `demo_step [ms]`: advance cl.serverTime once while cl_freezeDemo is set.
An optional overlay next to the recording indicator shows play/pause, speed, elapsed time (cl.snap.serverTime minus the first snapshot) and a progress bar from FS_FTell divided by the demo length (keep the length FS_FOpenFileRead already returns).
Smoother slow motion: Com_ModifyMsec truncates msec to an int every frame, so 0.1x at 250 fps gets stuck at 1 ms and plays at the wrong speed. Carry the leftover fraction in demo playback, the way the AVI path already does.

- **Player-visible effect:** Watching demos feels like a video player: pause, step, speed up or down, see where you are in the demo.
- **Expected gain:** Big usability gain for demo viewers and movie makers with minimal code. Slow motion would track the requested speed accurately down to about 1 ms of game time per frame. High confidence.
- **Risk:** Low. Changing timescale in live play stays cheat-protected; the new commands only work when clc.demoplaying.
- **Evidence:** `src/qcommon/common.cpp:2774-2790 (int msec scaled by timescale, clamped to 1)`; `src/qcommon/common.cpp:2819-2821 (timescale 0 or ESC pauses demos)`; `src/qcommon/cvar.cpp:422 (cheat cvars allowed during demo playback)`; `src/client/cl_cgame.cpp:1609-1611 (cl_freezeDemo)`; `src/client/cl_main.cpp:588,606 (demo file length discarded)`; `src/client/cl_main.cpp:2455-2469 (overflow carry already used for AVI)`; `src/client/cl_scrn.cpp:489 (overlay hook point)`

### PX-8 — Demo seeking: fast-forward, rewind and jump to time or bookmark
*fun-feature · effort L · client-only-opt-in*

`demo_seek <+s|-s|mm:ss>` and `demo_jump <bookmark>`.
Fast-forward:
- read demo messages until the target time;
- call the cgame at least every ~16 snapshots so it never falls more than PACKET_BACKUP (32) snapshots or MAX_RELIABLE_COMMANDS (128) commands behind. Configstring 'cs' commands are only applied when the cgame pulls them, so skipping the cgame entirely would leave player names and models wrong;
- skip the renderer back end, the same way the minimized path already does;
- while seeking, suppress S_StartSound and CG_FX_PLAY* traps so jumps don't trigger a burst of sounds and effects.
Rewind:
- restart the demo (a fast reload, since the renderer caches the same map's media) and fast-forward;
- this is required because the engine rejects backwards time and a second world load.
Later: index snapshots during the first pass to make repeated jumps faster.

- **Player-visible effect:** Scrub through a 20-minute duel demo, jump back 10 seconds to rewatch a kill, jump to saved highlights.
- **Expected gain:** Rough estimate: fast-forward without rendering at 50-200x realtime (message decode tens of µs, cgame frame about 0.2-0.5 ms every 16 snapshots). A rewind would cost one demo reload (about 1-3 s on a modern PC with caches) plus fast-forward. Low-medium confidence: not measured, and it depends on demo size. Same feature family as jaMME/q3mme and wolfcamql.
- **Risk:** Medium. The cgame may show transient glitches after a jump (missed events, scoreboard catching up). Local entities may hang on for a moment. Needs testing with base 1.02/1.04 cgames and the popular mods.
- **Prerequisites:** PX-7
- **Evidence:** `src/client/cl_main.cpp:499-545 (sequential CL_ReadDemoMessage)`; `src/client/cl_cgame.cpp:475-480 (cycled-out commands tolerated during demos)`; `src/qcommon/qcommon.h:160,168 (PACKET_BACKUP 32, MAX_RELIABLE_COMMANDS 128)`; `src/client/cl_cgame.cpp:1601-1602 (backwards time is fatal)`; `src/renderer/tr_bsp.cpp:2016-2017 (second world load refused)`; `src/client/cl_scrn.cpp:428-431 and :446 (skip-backend path already exists)`

### PX-9 — Built-in FFmpeg export (MP4/H.264/H.265 via pipe), cross-platform
*fun-feature · effort M · client-only-opt-in*

Add `video-pipe [name]` plus `cl_aviPipeFormat` (default e.g. '-c:v libx264 -crf 18 -preset fast -c:a aac'). This is the Quake3e approach: popen an ffmpeg process and stream the existing AVI writer's output (video plus PCM) into its stdin. The writer already handles non-seekable output for FIFOs. Today that only works on Unix with a manual mkfifo (FS_IsFifo returns false under MSVC); this makes it one command on Windows too, with no 2 GB split.
Security: the format cvar must be CVAR_ARCHIVE|CVAR_VM_NOWRITE so mods cannot inject shell commands. Servers already cannot set it, because systeminfo only writes SYSTEMINFO cvars. Run only an `ffmpeg` binary, and treat the cvar as arguments, never as a whole command line.

- **Player-visible effect:** Record a demo straight to a small, shareable MP4 (YouTube/Discord ready) instead of huge MJPEG AVIs.
- **Expected gain:** Rough estimates (medium confidence):
- H.264 CRF 18 is about 8-10x smaller than MJPEG q90 at 1080p60.
- Faster capture than the in-process libjpeg 9a encode on the Windows bundle, because ffmpeg encodes in parallel in another process.
- Removes the 2 GB split.
- **Risk:** Low-medium. Needs a pipe-backed handle in files.cpp, which build-notes lists as a high-risk file, so keep the change small. Fail gracefully if ffmpeg is missing.
- **Evidence:** `src/client/cl_avi.cpp:398 (afd.isFifo), :484-490 (no split for FIFO), :200-210 (JPEG or raw capture)`; `src/qcommon/files.cpp:1122-1140 (FS_IsFifo, false under MSVC)`; `src/qcommon/q_shared.h:1147 (CVAR_VM_NOWRITE)`; `src/client/cl_parse.cpp:376-385 (server only sets SYSTEMINFO cvars)`; `libs/jpeg-9a (non-turbo libjpeg bundled for Windows)`; `docs/build-notes.md (files.cpp flagged high-risk)`

### PX-10 — Motion blur (sub-frame accumulation) for video export, supersampling later
*visual-wow · effort M · client-only-opt-in*

`cl_aviBlurFrames N`: while capturing, run the engine at cl_aviFrameRate*N sub-steps, read each one back, add it into a float buffer and write the average every N sub-frames. Mix audio per sub-step: snd_dma currently computes samples per frame from cl_aviFrameRate. Because game time is whole milliseconds, rate*N/timescale must stay at or below 1000. For example, 60 fps with 16 sub-frames at 1.0x works; at 0.25x, N is at most 4. `cl_aviSupersample 2` (render 2x then downscale) needs FBO support from the renderer modernization work.

- **Player-visible effect:** Movie-like motion blur on saber swings and flips in exported videos: the classic frag-movie look.
- **Expected gain:** Big visual quality step for videos. Offline cost: rendering slows down N times plus N readbacks (for example 8x blur at 1080p is roughly 50-100 ms per output frame, so a minute of 60 fps video takes about 3-6 minutes to render; low-medium confidence). Nothing changes outside capture.
- **Risk:** Low. Limited to capture and demos. Watch audio sync and the integer-millisecond limit at low timescales.
- **Prerequisites:** PX-12 (PBO readback) recommended; FBO support from the perf-3D renderer work for supersampling
- **Evidence:** `src/client/cl_main.cpp:2455-2469 (fixed capture timestep with overflow carry)`; `src/client/snd_dma.cpp:2337-2348 (audio samples per video frame)`; `src/client/cl_scrn.cpp:554 (CL_TakeVideoFrame after each frame)`; `src/renderer/tr_backend.cpp:1366-1388 (synchronous readback)`

### PX-11 — Fix AVI audio capture when OpenAL is enabled
*tooling · effort S · client-only-transparent*

CL_OpenAVIForWriting decides whether audio capture is possible by comparing a cvar named s_backend with 'OpenAL'. s_backend is an ioquake3 cvar that is never registered in jk2mv, which uses s_UseOpenAL instead. With s_UseOpenAL 1 the check passes, so the AVI header declares an audio stream. But OpenAL bypasses the software mixer that feeds CL_WriteAVIAudioFrame, so no audio samples are written. Fix: test s_UseOpenAL, warn, and leave the audio stream out. The same bug exists upstream, so this is an upstream-friendly patch.

- **Player-visible effect:** Videos recorded by EAX/OpenAL users stop coming out silent or broken; a clear warning tells them to switch audio backend for recording.
- **Expected gain:** Correctness fix. Confidence high that the check never passes as intended; medium on exactly how players see it (silent or desynced audio track).
- **Risk:** Very low.
- **Evidence:** `src/client/cl_avi.cpp:429-449 (s_backend check)`; `src/client/snd_dma.cpp:329-330 (jk2mv uses s_UseOpenAL)`; `src/client/snd_mix.cpp:69-70 (audio frames only written by software mixer)`; `src/client/snd_dma.cpp:2261-2300 (OpenAL branch skips software mixing)`

### PX-12 — Faster, lossless capture: PNG screenshots, PBO async readback, libjpeg-turbo
*tooling · effort S · client-only-opt-in*

Three independent pieces:
- `screenshot_png` and `r_screenshotFormat`: libpng is already linked, but only for loading textures.
- Asynchronous readback through GL_ARB_pixel_buffer_object, double-buffered, for video capture. Fall back to the current path when the extension is missing.
- Replace the bundled Windows libjpeg 9a with libjpeg-turbo, which has the same API.

- **Player-visible effect:** Crisp lossless screenshots for wallpapers and comparisons, and faster video rendering.
- **Expected gain:** Capture overhead per frame should drop noticeably (estimate: readback stall from a few ms to under 1 ms with PBO; JPEG encode 2-4x faster with turbo). Medium-low confidence, not measured; it matters for wall-clock export time, not output quality.
- **Risk:** Low. PBOs need a fallback; PNG compression level should be configurable for speed.
- **Evidence:** `src/renderer/tr_init.cpp:783-884 (JPEG/TGA screenshot commands only)`; `src/renderer/tr_image.cpp:23, 2206-2426 (libpng used for loading only)`; `src/renderer/tr_backend.cpp:1366-1388 and src/renderer/tr_cmds.cpp:515-600 (synchronous glReadPixels and in-process JPEG)`; `libs/jpeg-9a`; `grep: no PBO or FBO calls anywhere in src/renderer`

### PX-13 — Photo mode (freeze, fly, frame, shoot)
*fun-feature · effort S · client-only-opt-in*

`photomode` toggles, in one step:
- cl_freezeDemo, so time stops;
- the PX-4 free camera, with fine speed control plus roll and fov keys;
- PX-6 HUD hiding;
- optional letterbox and grid overlay (not captured);
- PNG capture (PX-12).
The existing r_we weather (rain, snow, fog) can be added for mood. `screenshot_hires N` (tiled or FBO supersampled) comes later, once the renderer has FBOs.

- **Player-visible effect:** Wallpaper-quality shots of any moment in a demo: the community shares these naturally.
- **Expected gain:** High visibility and very cheap once PX-4 and PX-6 exist. No performance cost.
- **Risk:** Low. Demo-only.
- **Prerequisites:** PX-4; PX-6; PX-12 for PNG; FBO renderer work for hi-res
- **Evidence:** `src/client/cl_cgame.cpp:1609 (cl_freezeDemo)`; `src/renderer/tr_WorldEffects.cpp:2173-2241 (r_we snow/rain/fog)`; `src/renderer/tr_cmds.cpp:473-486 (screenshot taken at swap)`

### PX-14 — Engine-drawn performance, network and speed overlay that works with all mods
*fun-feature · effort S · client-only-opt-in*

Add `cl_overlay` (bitmask, default 0), drawn with the console font after the cgame:
- FPS with a frame-time graph and 1% lows, which cg_drawFPS does not offer;
- net panel: ping, snapshot rate, dropped or rate-delayed snapshots, choke;
- speedometer from cl.snap.ps.velocity.
None of these are cheats. Today debuggraph/timegraph are CVAR_CHEAT and cl_framerate only prints to the console. An optional strafe/accel helper for movement players is possible but should stay opt-in, because some servers see it as an aid.

- **Player-visible effect:** Players see their real smoothness and network quality on any server or mod, and movement players get a speedometer.
- **Expected gain:** Negligible cost (a few dozen 2D quads, under 0.05 ms, estimate). It also gives the community a visible, shareable way to check the perf-3D improvements.
- **Risk:** Low. Don't duplicate cgame elements by default; place it so it does not collide with mod HUDs (configurable corner).
- **Evidence:** `src/client/cl_scrn.cpp:403-407 (debug graphs are CVAR_CHEAT)`; `src/client/cl_main.cpp:2484-2495 (cl_framerate prints to console)`; `src/client/cl_scrn.cpp:486-490 (draw point after CL_CGameRendering)`; `src/mvsdk/code/cgame/cg_main.c (cg_drawFPS, cg_lagometer exist in cgame only)`

### PX-15 — Modern controller support: SDL GameController, dual-stick look, gyro and rumble
*fun-feature · effort M · client-only-opt-in*

Replace the legacy SDL_Joystick path, which has a fixed axis mapping and no stick look curve, with SDL_GameController when in_joystick 2.
- Standard Xbox/PlayStation layout mapped onto the existing JOY0-31 and AUX0-31 key codes. Old binds and the per-version key remapping the VMs see keep working; readable names like PAD_A become aliases.
- Left stick moves; right stick looks, with deadzone, response curve and sensitivity cvars (in the style of ioquake3's j_* cvars).
- Triggers can be bound like buttons.
- Optional rumble on damage, detected from the health delta in cl.snap.ps.
- Optional gyro aiming. Gyro needs SDL 2.0.14 or later, so bump the bundled Windows SDL 2.0.9 to a current 2.x; that also brings a newer controller database.

- **Player-visible effect:** Play JK2 on a Steam Deck or with a DualSense/Xbox pad comfortably, gyro-aim the disruptor, feel hits.
- **Expected gain:** Opens handheld and couch play (a strong 'wow' clip: JK2 on a Steam Deck). No performance cost.
- **Risk:** Low-medium. The SDL bump is mostly drop-in (keep SDL2, not SDL3). Rumble from snapshot health is approximate.
- **Prerequisites:** Bundled SDL2 upgrade for gyro/sensors
- **Evidence:** `src/sdl/sdl_input.cpp:9, 398-456 (SDL_Joystick open), :1037-1083 (fixed axis handling)`; `src/client/cl_input.cpp:482-512 (fixed AXIS_SIDE/FORWARD/UP mapping)`; `src/client/cl_keys.cpp:291, 324, 2180-2215 (JOY/AUX key codes and version remap)`; `libs/SDL2/include/SDL_version.h:61-62 (SDL 2.0.9)`

### PX-16 — Borderless desktop fullscreen and Alt+Enter
*fun-feature · effort S · client-only-opt-in*

Add `r_fullscreen 2`, which uses SDL_WINDOW_FULLSCREEN_DESKTOP (no display mode change, instant alt-tab), and an Alt+Enter toggle built on the existing live r_fullscreen switching. Today borderless needs four latched cvars (r_fullscreen 0, r_noborder 1, r_mode -2, r_centerWindow 1). It works with the default post-process gamma. Hardware gamma (method 1) still needs exclusive fullscreen.

- **Player-visible effect:** Alt-tabbing out of the game no longer flickers or changes the desktop resolution.
- **Expected gain:** Comfort improvement, no performance cost. High confidence.
- **Risk:** Very low.
- **Evidence:** `src/sdl/sdl_window.cpp:668-679 (only SDL_WINDOW_FULLSCREEN or BORDERLESS)`; `src/sdl/sdl_window.cpp:340-362 (live fullscreen toggle)`; `CVARS.rst r_gammamethod (hardware gamma only in fullscreen)`

### PX-17 — Console and chat quality of life
*fun-feature · effort S · client-only-opt-in*

- Keep the console command history across restarts in a history file; today it lives only in memory.
- Highlight chat lines that mention the player's name, with an optional sound, and flash the taskbar when the window is unfocused. WIN_SetTaskbarState already exists but is only used on connect and download.
- `cl_chatLog`: a chat-only log file with timestamps.
- Ctrl+C copies the selection or input line; paste already exists.
The notify area and the messagemode input are drawn by the engine, so this works with every mod.

- **Player-visible effect:** Never miss a message addressed to you; history survives restarts.
- **Expected gain:** Small but frequently felt improvements. No performance cost.
- **Risk:** Very low. The chat log is opt-in for privacy.
- **Evidence:** `src/client/cl_keys.cpp:1199-1204 (in-memory history)`; `src/qcommon/qcommon.h:15 (COMMAND_HISTORY 128)`; `src/client/cl_keys.cpp:517-533 (paste only)`; `src/client/cl_console.cpp:31-117 (engine-side chat input), :646 (Con_DrawNotify)`; `src/client/cl_cgame.cpp:1542 and src/client/cl_parse.cpp:588-689 (WIN_SetTaskbarState uses)`

### PX-18 — Join links, quick play and find-a-player
*fun-feature · effort M · client-only-opt-in*

- Register a `jk2mv://connect/<host:port>[?password=]` URL handler: MimeType in res/org.mvdevs.jk2mv.desktop, a registry key in the NSIS installer, plist on macOS. Clicking a link in Discord or on a website then launches the game and connects. Only connect is allowed, and the address is validated; never pass arbitrary commands.
- `quickplay`: use the existing master and ping machinery to pick a server with humans, not full, low ping, on a compatible version, and connect. Add a menu_patch button.
- `findplayer <name>`: send getstatus to every server in the list and report where that player is.

- **Player-visible effect:** One click from a Discord message straight into a game; new players get into a populated server immediately; friends find each other.
- **Expected gain:** Lowers the barrier to join a small community (fewer clicks). No performance impact beyond normal server queries, rate-limited by mv_slowrefresh.
- **Risk:** Low-medium. The URL handler is an attack surface, so validate strictly. findplayer must respect query rate limits.
- **Evidence:** `res/org.mvdevs.jk2mv.desktop (no MimeType / URL scheme)`; `src/client/cl_main.cpp:3652-3690 (multi-master globalservers), :2987 (serverstatus), :3707+ (ping list)`; `CVARS.rst mv_slowrefresh`

### PX-19 — Optional Discord Rich Presence
*fun-feature · effort M · client-only-opt-in*

A compile-time option plus `cl_discordPresence` (default 0) that shows the map, server name, player count and game version, with a 'Join' button using the PX-18 URLs. All the data is already in the client's serverinfo/gamestate. Uses the MIT discord-rpc library, or Discord's newer SDK.

- **Player-visible effect:** Friends see 'Playing JK2 - ffa_bespin - 12/32' with a Join button.
- **Expected gain:** Free social visibility for the game. Negligible CPU. Medium confidence on adoption; EternalJK, an OpenJK fork for JKA, ships a similar feature.
- **Risk:** Low-medium. Third-party dependency and privacy; must stay opt-in and off by default.
- **Prerequisites:** PX-18 for join links
- **Evidence:** `src/client/cl_main.cpp:3700-3720 and cl.gameState serverinfo (data available)`; `src/CMakeLists.txt:313-326 (optional library pattern used for SDL2)`

### PX-20 — Automatic highlight bookmarks for auto-demos
*fun-feature · effort S · client-only-opt-in*

While cl_autoDemo is recording, detect highlights from the snapshot player state: score increases in ps.persistant (stable across versions, unlike event numbers), plus optional multi-kill windows. Write them to a sidecar file next to the demo; saveDemo already stores manual timestamps in the file name. Optionally keep LastDemo automatically when a highlight happens. During playback, `demo_nextMark` / `demo_prevMark` jump between highlights (needs PX-8). PX-5 can use them to start a camera path.

- **Player-visible effect:** After a match, jump straight to your kills; no more scrubbing through 15 minutes of demo.
- **Expected gain:** Turns the existing auto-demo feature into a highlight reel. No performance cost.
- **Risk:** Low. The score heuristic may misfire in team or CTF modes; label bookmarks by type.
- **Prerequisites:** PX-8 for jumping
- **Evidence:** `src/client/cl_demos_auto.cpp:128-150 (saveDemo timestamps), :169-199 (saveDemoLast)`; `src/client/cl_main.cpp:2472-2477 (autodemo lifecycle in CL_Frame)`

### PX-21 — timedemo statistics for benchmarking (percentiles, CSV)
*tooling · effort S · client-only-opt-in*

Extend CL_DemoCompleted, which only prints average fps today, to report:
- frame-time p50, p95 and p99, 1% low, min and max;
- front-end versus back-end ms from com_speeds data;
- optionally a CSV through `timedemo_csv <file>`.
Add a `benchmark <demo>` alias that runs a fixed demo with timedemo 1 and prints a one-line result to paste into bug reports and PRs.

- **Player-visible effect:** Players and contributors can measure and share their performance, and the perf-3D improvements get credible before/after numbers.
- **Expected gain:** Needed to prove the perf-3D work, since nothing can be measured in CI without a GPU. Trivial cost.
- **Risk:** Very low.
- **Evidence:** `src/client/cl_main.cpp:478-492 (average fps only)`; `src/client/cl_cgame.cpp:1668-1674 (deterministic 50 ms timedemo steps)`; `src/client/cl_scrn.cpp:521-528 (frontend/backend timings with com_speeds)`

### PX-22 — Demo cut and trim for shareable clips
*fun-feature · effort M · client-only-opt-in*

`demo_cut <start> <end> <out>`:
- play or seek to start;
- write a new demo that begins with the current gamestate (as CL_Record_f does);
- build a non-delta snapshot from cl.snap and the entity baselines;
- then copy the following messages up to end.
The output is a normal dm_15/dm_16 file that any JK2 client can play.

- **Player-visible effect:** Share a 30-second clip demo instead of a 40 MB match.
- **Expected gain:** Saves bandwidth and storage, and helps clips spread. No runtime cost.
- **Risk:** Medium. The synthesized non-delta snapshot must exactly match each version's netfields (1.02 and 1.04 differ). Test round trips with the base cgames.
- **Prerequisites:** PX-8
- **Evidence:** `src/client/cl_main.cpp:345-460 (CL_Record_f writes gamestate and baselines mid-game)`; `src/client/cl_parse.cpp:226 (recording waits for a non-delta snapshot)`; `src/client/cl_main.cpp:620-622 (protocol chosen from dm_15/dm_16 extension)`

### PX-23 — Sub-millisecond time and cgame camera cooperation through a fork-specific API (MVSDK mods only)
*fun-feature · effort L · needs-vm-or-mod-changes*

Very smooth slow motion (below about 1 ms of game time per frame) and true cgame-aware cameras need the cgame to accept fractional time, because CG_DRAW_ACTIVE_FRAME passes an int serverTime. mvapi.h says forks must not modify it and should define their own API. So add a separately negotiated extension, for example a new vmMain command or a syscall range outside MVAPI's, that only MVSDK-based mods would adopt. Everything else in this list works without it.

- **Player-visible effect:** Ultra-slow-motion shots (0.05x) with fluid animation in exported videos, for mods that opt in.
- **Expected gain:** Only matters to movie makers using MVSDK-based mods. Low priority compared with the engine-only items.
- **Risk:** Medium. API design has to stay compatible with upstream jk2mv's future MVAPI levels; needs changes in the mvsdk submodule, which is a separate project.
- **Prerequisites:** PX-5; PX-7
- **Evidence:** `src/api/mvapi.h:18-22 (forks must define their own API), :31 (MV_APILEVEL 3)`; `src/client/cl_cgame.cpp:1440-1442 (int serverTime passed to cgame)`; `src/client/cl_cgame.cpp:1322 and src/client/cl_ui.cpp:1163 (level-4 handlers unreachable while MV_APILEVEL is 3)`

### PX-24 — Server browser 2.0 in the engine-shipped jk2mvmenu
*fun-feature · effort M · needs-vm-or-mod-changes*

The main menu by default (mv_menuOverride 0) runs jk2mvmenu, built from the mvsdk ui sources and shipped with the engine. So the browser can be improved without affecting mods, but it is a change to a UI module inside the mvsdk submodule:
- filters: hide empty, full or bots-only servers, by version and gametype;
- a humans-only player count column;
- favorites with live ping;
- map levelshot preview;
- PX-18 quickplay and findplayer buttons;
- PX-1 preset buttons.
Smaller additions to the retail menus can keep using assetsmv .menu_patch files.

- **Player-visible effect:** A browser that shows where the real players are, in one screen.
- **Expected gain:** Better onboarding and retention for a small community. No runtime cost.
- **Risk:** Low for mods. Coordination cost with the upstream mvsdk project; patch files only apply when the original menu's hash matches.
- **Prerequisites:** PX-18 for engine-side quickplay/findplayer
- **Evidence:** `src/client/cl_ui.cpp:1217-1238 (jk2mvmenu on main menu)`; `src/CMakeLists.txt:536-589 (mvmenu built from mvsdk)`; `src/botlib/l_script.cpp:1460-1484 (hash-checked menu patching)`; `src/api/mvapi.h (MVSORT_CLIENTS_NOBOTS)`

**Notes.** Nothing was measured: there is no GPU and no game assets here. All figures are estimates from the code paths read plus what comparable forks shipped:
- Quake3e: video-pipe with cl_aviPipeFormat, high confidence.
- jaMME / q3mme: free camera, camera paths, motion blur and seeking for JKA/Q3, high confidence.
- ioquake3: SDL GameController and FIFO-based AVI piping, medium-high confidence.
- ET:Legacy: URL join handler, medium confidence.
- EternalJK: Discord presence, medium confidence.

BEST 'WOW' VIDEO FOR THE COMMUNITY. "Your 2003 demos, re-shot as a cinematic." Take any old dm_15/dm_16 demo, under any mod, and show:
- PX-4 free camera, then a PX-5 camera path with a slow-motion ramp into a saber kill;
- PX-6 clean feed;
- PX-10 motion blur;
- PX-1 'modern' preset plus a PX-2 color grade, with the built-in before/after split screen;
- optional r_we snow;
- exported in one command to MP4 through PX-9.
All of it is engine-only and works with every 1.02-1.04 mod; nothing like it exists for JK2 today. A second clip could show JK2 on a Steam Deck with gyro aiming (PX-15) and joining from a Discord link (PX-18).

ENGINE-ONLY VERSUS VM CHANGES. Everything except PX-23 and PX-24 is engine-only, because the engine already owns the hooks:
- the refdef pass-through, the sound listener and the FX view;
- usercmds generated during demos;
- all 2D draw syscalls;
- the post-process LUT;
- the AVI writer;
- the input layer.
Two things do need VM/mvsdk work:
- PX-23: sub-millisecond time needs a fork-specific API, because mvapi.h forbids forks from changing it.
- PX-24: browser UI changes go into the jk2mvmenu module of the mvsdk submodule. Mods are not affected, but it touches a VM module in a separate project.

GUARDRAILS FOR ALL ITEMS.
- Camera, seek, HUD-hide and time features must only work when clc.demoplaying, otherwise they become wallhacks or cheats in live play.
- New cvars default to the classic behavior.
- Anything that spawns a process or opens files gets CVAR_VM_NOWRITE (q_shared.h:1147).
- Presets must not touch gameplay-coupled cvars (com_maxfps 125, snaps, rate, timeNudge) or the dead or banned ones (r_flares is a no-op; cg_shadows 2 is forced off outside developer).

SUGGESTED ORDER (small, reviewable PRs).
1. PX-11 (bug fix), PX-21 (benchmark stats, which the perf work needs), PX-1, PX-16, PX-17, PX-14, PX-7, PX-6.
2. PX-4, PX-2, PX-9, PX-12, PX-13.
3. PX-5, PX-10, PX-15 with the SDL bump.
4. PX-8, PX-20, PX-22, PX-18 and PX-19.
5. PX-23 and PX-24 only if MVSDK coordination is wanted.

SIDE FINDINGS.
- CVARS.rst gives con_timestamps a default of "1", but the code registers "0" (src/client/cl_console.cpp:411).
- Level-4 MVAPI handlers in cl_cgame.cpp:1322 and cl_ui.cpp:1163 can never run while MV_APILEVEL is 3; same upstream.
- The post-process gamma pass reallocates its scene texture with glCopyTexImage2D every frame (tr_backend.cpp:1325). That is a cheap perf fix for the perf-3D area (use glCopyTexSubImage2D after one allocation).
- An upstream reference clone was made in the scratchpad (/tmp/claude-0/-home-user-jk2mv/8f38330d-50e3-5478-ade0-e0c2e7e6406c/scratchpad/upstream-jk2mv). The repository was not modified.

## Measuring performance and verifying changes (client benchmark, server load test, asset-free CI)

**Current state.** CLIENT TIMING TODAY
- timedemo: `timedemo 1; demo <name>` (demo files are demos/<name>.dm_15 or .dm_16, cl_main.cpp:562-650). When the demo ends, CL_DemoCompleted prints one line, "N frames, X seconds: Y fps" (cl_main.cpp:478-491). The time base is deterministic: every rendered frame advances the game by exactly 50 ms (cl_cgame.cpp:1664-1673), so a 2-minute demo always gives 2400 frames. Loading time is excluded because timeDemoStart is set on the first active frame.
- Hard limits of timedemo:
  - The result is a single average, measured with a 1 ms timer.
  - In timedemo, Com_Frame forces minMsec=1 (common.cpp:2904-2905) and busy-waits until the millisecond ticks over (common.cpp:2907-2914). That caps it at 1000 fps. The same cap applies with com_maxfps 0 (common.cpp:2886).
  - CL_DemoCompleted runs `nextdemo` before CL_Disconnect_f, which longjmps (cl_main.cpp:489-490, 1032-1037). So `nextdemo "demo x"` is probably killed as soon as it starts (unverified, no assets). Workarounds: `nextdemo "wait; demo x"`, or one process per run.
- Timer: Sys_Milliseconds is timeGetTime with timeBeginPeriod(1) on Windows (sys_win32.cpp:411-420, 437-447). On Unix it is gettimeofday, which is wall-clock and not monotonic (sys_unix.cpp:60-82). There is no microsecond timer anywhere.
- com_speeds 1/3: prints one line per frame in ms (frame/all/sv/ev/cl/gm/rf/bk) (common.cpp:2867-3007; packet timing at common.cpp:2300-2318; game time at sv_main_frame.h:133-162).
  - Backend time is CPU submit time only. With r_finish 0 the 3D path skips glFinish (tr_backend.cpp:430-436, 1268-1270), so GPU time is invisible (tr_backend.cpp:1400-1436; frontend at tr_scene.cpp:516).
  - Verified on the dedicated server with 8 bots: every frame printed all:0 gm:0, and the 50 ms sleep showed up in the 'ev' column. At this resolution it is useless.
- r_speeds 1..7 (tr_cmds.cpp:13-64): counts shaders, surfaces, tris, texture MB and overdraw. It has no draw-call, bind or state-change counters (backEndCounters_t in tr_local.h ~984-995). It is CVAR_CHEAT (tr_init.cpp:1178), but cheat cvars may be changed during demo playback (cvar.cpp:422).
- Other existing counters, all at ms resolution:
  - cl_framerate: 32-frame average (cl_main.cpp:2486-2497, CVAR_TEMP at 2890).
  - timegraph/debuggraph: cheat-only (cl_scrn.cpp:403-407).
  - cg_drawFPS: cgame-side, so it depends on the mod; 4-frame average (mvsdk cg_draw.c:1809-1830).
- No GPU timer queries, although glext.h already defines GL_TIME_ELAPSED (glext.h:1530). No profiler integration.

SERVER TODAY
- No server stats command beyond status/serverinfo/uptime (sv_ccmds_registration.h:30-54, common.cpp:2472). The only "Hitch warning" is for frames over 500 ms (common.cpp:2794-2801).
- Bots: JK2 MP bots navigate with botroutes/<map>.wnt (mvsdk ai_wpnav.c:1663). The game never calls trap_BotLibLoadMap (ai_main.c:949-960), so AAS files are not needed. This corrects the brief.
- Bots get their snapshots built but never encoded or sent (sv_snapshot_sender.h:108-110), so a bot-only test skips the encode, Huffman and netchan path.
- Gotchas found by running jk2mvded here:
  1. Hibernation ignores bots (sv_main_frame.h:83-97), so a bot-only server drops to sv_hibernateFps=4 wakeups (SV_FrameMsec at sv_main_frame.h:12-28; default at sv_init_lifecycle.h:107). CVARS.rst documents a sv_hibernateTime cvar that the code does not register, and gives the wrong default (5).
  2. jk2mvded runs as `dedicated 2` (ROM, common.cpp:2579) and heartbeats to 3 hard-coded masters (sv_init_lifecycle.h:89-94). With `+set dedicated 1` it sent no heartbeats (verified).
  3. MAX_CONSOLE_LINES is 32 (common.cpp:437-456) and extra '+' commands are silently dropped. Verified: 31 `+addbot` arguments produced 23 bots and the trailing `+quit` was lost.
  4. Adding many bots in one frame overflows the bots' reliable command queues, and they are kicked with "Server command overflow" (sv_main_messaging.h:71-77). Bots only acknowledge reliable commands when the game AI drains them (sv_bot_lifecycle.h:166-190, ai_main.c:695). With `wait 5` between addbots, 31 bots stayed connected.
  5. ERR_DROP (for example a missing map) exits with code 0. Only ERR_FATAL exits non-zero (3). CI therefore has to parse the log.
  6. Each source IP gets 10 connectionless packets/s (sv_main_connectionless.h:298), and a client tries only 10 UDP ports (net_ip.cpp:815-831). Both matter when many clients run on one machine.

TESTS AND CI TODAY
- The 22 gtest files (about 729 TEST macros) include only test_utils.h and std headers. They re-declare mock structs and never compile or link engine code. tests/CMakeLists.txt:50-53 has "Integration/Performance tests (future)" placeholders.
- tests.yml builds with -DBuildMVMP=OFF -DBuildMVDED=OFF -DBuildMVSDK=OFF (tests.yml:46,81), so that workflow never builds or runs the engine.
- build.yml still targets the retired windows-2019 (line 179) and macos-12 (line 263) images.

VERIFIED IN THIS CONTAINER (scratchpad only, repo untouched)
- Dedicated server without game assets:
  - Required files: an empty zip named base/assets5.pk3 (the filesystem only checks that it exists, files.cpp:3453); base/mpdefault.cfg (files.cpp:3990); and 5 stub string packages, base/strip/{CON_TEXT,MP_INGAME,MP_SVGAME,SP_INGAME,STR_SERVER}.sp, each containing only 'VERSION 1/ID n/REFERENCE X/COUNT 0' (strip.cpp:1523-1525, 1753-1756).
  - Map: an 856-byte RBSP v1 map generated by about 60 lines of Python (1 shader, 12 planes, 1 node, 2 leafs, 1 brush, 1 model, 2 info_player_deathmatch). That is the minimum cm_load.cpp:76-310 accepts.
  - Game code: the mvsdk QVMs that the normal build already packs into assetsmv2.pk3, JIT-compiled. A native jk2mpgame_amd64.so placed in base/ works too.
  - Bots: a stub botfiles/bots.txt.
  - Result: the server ran map ci_box at 20 Hz with 31 bots and shut down cleanly with exit 0.
  - Rough CPU (QVM, container CPU, trivial map): 0.93 ms per 50 ms frame idle (including startup) and 1.7 ms with 31 bots.
- Headless client:
  - SDL_VIDEODRIVER=offscreen with Mesa llvmpipe (through EGL) brought up GL ('GL_RENDERER: llvmpipe', multitexture and anisotropic extensions detected).
  - It also needed stub ui/menus.txt and ui/jk2mpmenus.txt. Without them the UI raises a fatal 'recursive error'.
  - It loaded the UI and wrote base/screenshots/ci_console.tga (640x480, console visibly rendered), and survived vid_restart.
  - xvfb-run failed here with 'Couldn't find matching GLX visual'.
  - Entering a map works until cgame CG_LoadClientInfo hits 'DEFAULT_MODEL (kyle) failed to register', because there is no player GLM.

**Already exists (do not reinvent):**
- timedemo 1 + demo <name>: deterministic 50 ms/frame playback with an average fps line (cl_main.cpp:478-491, cl_cgame.cpp:1664-1673); demos are demos/*.dm_15 (1.02/1.03) and *.dm_16 (1.04) (cl_main.cpp:582-640)
- activeAction cvar: runs a command string on the first active frame, which is useful for scripted timedemo screenshots (cl_cgame.cpp:1552-1558)
- nextdemo cvar for chaining or quitting after a demo (cl_main.cpp:675-688); see the probable ordering issue at cl_main.cpp:489-490
- wait N console command (cmd.cpp:32-38) and +exec <cfg> for scripting; use exec because of the 32 '+' command-line limit (common.cpp:437)
- com_speeds 1/3 frame breakdown in ms on client and dedicated server (common.cpp:2867-3007, sv_main_frame.h:133-162)
- r_speeds 1..7 renderer counters (tr_cmds.cpp:13-64); r_measureOverdraw, r_showtris, r_logFile (GL call log), r_finish, gfxinfo, imagelist (tr_init.cpp:1170-1220)
- cl_framerate (cl_main.cpp:2486-2497), timegraph/debuggraph (cl_scrn.cpp:403-407), cgame cg_drawFPS (mod-side)
- screenshot / screenshot_tga <name> to screenshots/<name>.(jpg|tga) (tr_init.cpp:783-880); video command with a fixed cl_aviFrameRate time step (cl_main.cpp:2456-2469)
- meminfo, zone_stats, hunklog (common.cpp:1339-1343, 1800-1803); com_showtrace trace counts per frame (common.cpp:3012-3023)
- Network debug: showpackets, showdrop (net_chan.cpp:49-50), net_dropsim (net_ip.cpp:890), cl_shownet, cl_showTimeDelta, sv_showloss, sv_padPackets
- Bots: addbot <name> [skill] [team] [delay], botlist, bot_minplayers (game VM, mvsdk g_bot.c:544-609, 909-960, 973-993); botlib cvars bot_enable/bot_thinktime/bot_pause (sv_bot_lifecycle.h:75-104); routes from botroutes/<map>.wnt, no AAS needed
- sv_hibernateFps (must be 0 for bot-only load tests), sv_fps, sv_maxclients (32 max), sv_maxOOBRate, sv_dynamicSnapshots, sv_enforceSnaps (CVARS.rst)
- r_allowsoftwaregl (sdl_window.cpp:814,1018) for llvmpipe/software GL in CI
- Unix crash logger fork that writes crashlog-*.txt (sys_unix.cpp:524, 1181), which CI can check for
- The normal build already produces mvsdk QVMs (assetsmv2.pk3: vm/jk2mpgame.qvm, cgame.qvm, ui.qvm) and native jk2mpgame/cgame/ui .so files usable without retail assets
- build.yml already packages RelWithDebInfo Windows builds (PDBs available for WPR/WPA, VS Profiler, Superluminal)

### BM-1 — Monotonic microsecond timer (Sys_Microseconds) for all profiling paths
*tooling · effort S · client-only-transparent*

Add `int64_t Sys_Microseconds(void)` to sys_public.h. Implement it with QueryPerformanceCounter/QueryPerformanceFrequency in sys_win32.cpp and clock_gettime(CLOCK_MONOTONIC) in sys_unix.cpp. Use it, without changing Sys_Milliseconds semantics, in these timing points: com_speeds (common.cpp:2867-3007), Com_RunAndTimeServerPacket (common.cpp:2300-2318), the game time in SV_Frame (sv_main_frame.h:133-162), the renderer frontend (tr_scene.cpp:516) and backend (tr_backend.cpp:1400-1436). Print them as ms with 3 decimals so existing log readers keep working.

- **Player-visible effect:** None; developer output only.
- **Expected gain:** Timing resolution goes from 1 ms to under 1 us (QPC runs at 10 MHz on Win10+). com_speeds becomes usable on modern CPUs: whole server frames with 8 bots currently print 0 ms, as verified. Confidence: high.
- **Risk:** Very low if purely additive. Keep game-visible time (event timestamps, Sys_Milliseconds) unchanged.
- **Evidence:** `src/sys/sys_win32.cpp:411-420`; `src/sys/sys_win32.cpp:437-447`; `src/sys/sys_unix.cpp:60-82 (gettimeofday, not monotonic)`; `src/qcommon/common.cpp:2867-3007`; `src/server/sv_main_frame.h:133-162`; `src/renderer/tr_backend.cpp:1400-1436`; `Measured: com_speeds 1 on jk2mvded with 8 bots printed 'all:  0 ... gm:  0' for 60/60 frames and 'ev: 50' (the sleep)`

### BM-2 — `benchmark` command: per-frame capture, percentiles, CSV, uncapped timedemo
*tooling · effort M · client-only-transparent*

Add a new src/client/cl_bench.cpp (about 250 lines). The command is `benchmark <demo> [runs=5] [warmup=1] [tag]`.
- It drives timedemo itself (internal state machine, not nextdemo) and replays the demo runs+warmup times.
- Per frame it records, in us, into a preallocated array: the frame-to-frame interval measured at the end of CL_Frame (cl_main.cpp:~2525) and the work time after the sleep. Frontend and backend times (and GPU time from BM-6) are optional.
- While it is active, Com_Frame uses minMsec=0 instead of 1 (common.cpp:2904-2905), which removes the 1000 fps ceiling. Plain timedemo is left unchanged so old numbers stay comparable.
- At the end it prints a summary: frames, avg fps, frame-time p50/p90/p95/p99/p99.9/max, '1% low' (average fps of the slowest 1% of frames, CapFrameX-style), 'P1' (1e6/p99), and the run-to-run coefficient of variation.
- It writes benchmarks/<demo>_<tag>_<date>.csv (per-frame data) and a .txt with GL_VENDOR/RENDERER/VERSION, resolution, key r_* cvars, JK2MV version, demo size and checksum. Files go through FS_FOpenFileWrite, so they land in fs_homepath/<game>/benchmarks/.
- Statistics note: a 2-minute demo gives only 2400 frames (24 frames in the 1% tail). Report 0.1% lows only over all runs pooled (5 x 2400 = 12000 frames).

- **Player-visible effect:** Opt-in console command that prints a results block. It is the basis for shareable before/after numbers and a later result card or overlay (BM-16).
- **Expected gain:** Turns one ms-quantized number per run, capped at 1000 fps, into frame-time distributions and CSV. Typical idTech3 timedemo run-to-run spread on a quiet desktop is about 1-3% (medium confidence, from common Q3-engine experience). Comparing medians of 5 runs should detect changes of about 3% or more, and percentile shifts (stutter fixes) that averages hide.
- **Risk:** Low: only active while the command runs. Watch that FS writes do not happen mid-run (buffer until the end).
- **Prerequisites:** BM-1
- **Evidence:** `src/client/cl_main.cpp:478-491 (only output today)`; `src/client/cl_cgame.cpp:1664-1673 (50 ms deterministic steps)`; `src/qcommon/common.cpp:2886, 2904-2914 (minMsec=1 busy-wait => 1000 fps cap)`; `src/client/cl_main.cpp:489-490 + 1032-1037 (nextdemo executed before the CL_Disconnect_f longjmp)`; `tests/CMakeLists.txt:53 'Performance tests (future)'`

### BM-3 — Reproducible Windows client benchmark protocol, usable today with no code
*tooling · effort S · client-only-transparent*

1) Setup.
- Use the CI artifact 'Windows Package (Portable), RelWithDebInfo, x64'. Unzip each build to C:\jk2bench\builds\<tag>\ and copy the retail assets0/1/2/5.pk3 into its base\.
- Use one shared home for all builds: +set fs_homepath C:\jk2bench\home. It is honored at startup (files.cpp:3972).
- Driver: vsync Off / application-controlled. Power plan High performance. Disable overlays (Steam, Discord, GFE).

2) Reference demos, recorded once with a stock build on a local listen server with bots, 120 s each:
- `jk2mvmp.exe +set fs_homepath C:\jk2bench\home +set g_gametype 0 +set fraglimit 0 +set timelimit 0 +devmap ffa_bespin`. Then add about 12 bots (`botlist` shows names; use `addbot <name> 4` with a few seconds between each), go spectator and `follow` a bot, then `record bench_ffa` ... `stoprecord`.
- Repeat for ctf_yavin (g_gametype 7, open outdoor) as bench_ctf, and a duel_* map close-up for sabers/glow as bench_duel.
- Optionally add one recording from a populated public server, and one 1.02 .dm_15 to cover the 1.02 path.
- Store each demo's SHA-256 with the results.

3) Presets. Pass latched cvars as +set: they apply before R_Init, whereas an +exec'd cfg runs only after the renderer has started (common.cpp:2575 vs 2654-2673).
- CPU-bound: +set r_mode -1 +set r_customwidth 640 +set r_customheight 480.
- Classic: 1920x1080, all defaults.
- GPU-bound: 3840x2160 +set r_ext_multisample 4 +set r_ext_texture_filter_anisotropic 16 +set r_DynamicGlow 1. This also keeps fps under the 1000 cap.
- Always: +set r_fullscreen 1 +set r_swapInterval 0 +set r_picmip 0 +set com_introplayed 1 +set cl_autoDemo 0.

4) Run, from PowerShell. jk2mvmp is a GUI-subsystem exe (src/CMakeLists.txt:468), so use `Start-Process -Wait`:
`Start-Process -Wait $exe -ArgumentList '+set fs_homepath C:\jk2bench\home +set logfile 2 <preset> +set timedemo 1 +set nextdemo quit +demo bench_ffa'`.
Use one process per run, discard the first run, and keep 5 runs. After each run parse home\base\qconsole.log with the regex '(\d+) frames, ([\d.]+) seconds: ([\d.]+) fps'. The log is truncated at every start (common.cpp:179), so copy it each time.

5) Frame-time percentiles before BM-2 exists: capture each run with CapFrameX or PresentMon (`PresentMon.exe -process_name jk2mvmp.exe -output_file run.csv -timed 130 -terminate_after_timed`). OpenGL presents are tracked through the kernel/DWM path. Check that the CSV is not empty.

6) Decision rule: compare the median avg fps and median p99 frame time per (demo, preset). Call a change real only if it exceeds max(3%, 2x the run-to-run spread).

Metrics to report: avg fps, 1% low, P1 / p99 frame time, p50.

- **Player-visible effect:** None directly. It lets the owner publish credible before/after numbers for each optimization.
- **Expected gain:** Immediate, repeatable A/B comparisons. Three presets separate CPU/driver-overhead wins (VBO, batching) from fill-rate wins. Confidence: high for the method, medium for PresentMon on OpenGL.
- **Risk:** Results depend on the hardware state (thermals, background tasks). The 1000 fps cap can saturate the CPU-bound preset on fast PCs until BM-2 exists. In-demo particle randomness has only a small effect.
- **Evidence:** `src/client/cl_main.cpp:478-491`; `src/qcommon/common.cpp:179 (qconsole.log opened with FS_FOpenFileWrite)`; `src/qcommon/common.cpp:2575, 2654-2673 (startup +set applied before renderer, +exec after)`; `src/qcommon/files.cpp:3971-3973 (fs_homepath startup override)`; `src/sdl/sdl_window.cpp:1001-1017 (r_fullscreen/r_mode/r_customwidth/r_swapInterval/r_ext_multisample)`; `src/renderer/tr_init.cpp:1073-1083 (anisotropy, glow, picmip)`

### BM-4 — Golden-frame visual regression test for the classic look
*tooling · effort S · client-only-transparent*

Timedemo time is deterministic (frame k always shows game time base + 50*k), and `wait N` counts frames. So `+set activeAction "wait 100; screenshot_tga g100; wait 100; screenshot_tga g200; ..." +set timedemo 1 +demo bench_ffa` captures the same game moments in every build. Compare the baseline build against a candidate with all new cvars at their defaults, using ImageMagick `magick compare -metric AE -fuzz 3% a.tga b.tga diff.png`, and fail above a pixel budget. This directly enforces the fork rule that any visual change is opt-in with the classic look as default. Run it on the owner's PC with real assets. A reduced CI version covers console/menu/2D (see BM-12). For trailers, the `video` command with a fixed cl_aviFrameRate step (cl_main.cpp:2456-2469) gives deterministic frame dumps.

- **Player-visible effect:** None. It guarantees players see no unintended change to the default look.
- **Expected gain:** Catches accidental changes to default rendering (gamma, overbright, mipmapping, sort order) that fps numbers never show. Confidence: high that capture is deterministic on one GPU and driver. Tolerance is needed for random cgame particles.
- **Risk:** Different GPUs and drivers rasterize slightly differently, so goldens are per machine. cgame effects may use random seeds, which a fuzz tolerance absorbs.
- **Evidence:** `src/client/cl_cgame.cpp:1552-1558 (activeAction on first snapshot)`; `src/client/cl_cgame.cpp:1668-1673`; `src/qcommon/cmd.cpp:32-38 (wait N)`; `src/renderer/tr_init.cpp:783-833 (screenshot_tga <name>)`; `src/client/cl_main.cpp:2456-2469 (video fixed step)`; `FORK_NOTES.md rules / task constraint: classic look by default`

### BM-5 — Renderer counters for draw calls, texture binds and state changes (r_speeds 8)
*tooling · effort S · client-only-transparent*

Add c_drawCalls, c_binds, c_stateChanges and c_vertsUploaded to backEndCounters_t. Increment them in R_DrawElements (tr_shade.cpp:151-169, the single glDrawElements funnel for world surfaces), GL_Bind (tr_backend.cpp:28) and GL_State. Add r_speeds 8, which prints these plus frontend/backend time in us. Feed the per-run totals into the BM-2 summary. Under timedemo these counts are deterministic for a given demo, so they show whether VBO, batching or sort work actually reduced driver work, independently of fps noise.

- **Player-visible effect:** None (dev cvar).
- **Expected gain:** Gives an exact, noise-free metric for the perf-3D roadmap. A 30% cut in draw calls is visible even when fps moves by only 3%. Confidence: high.
- **Risk:** Negligible (integer increments). r_speeds stays CVAR_CHEAT but works in demos.
- **Prerequisites:** BM-1
- **Evidence:** `src/renderer/tr_cmds.cpp:13-64`; `src/renderer/tr_local.h:~984-995 (backEndCounters_t has no draw/bind counters)`; `src/renderer/tr_shade.cpp:151-169`; `src/renderer/tr_backend.cpp:28`; `src/qcommon/cvar.cpp:422 (cheat cvars allowed during demo playback)`

### BM-6 — GPU frame time via GL_ARB_timer_query
*tooling · effort M · client-only-opt-in*

Add an opt-in cvar r_gpuTimers 1. When GL_ARB_timer_query (or EXT_timer_query) is present, wrap RB_ExecuteRenderCommands in GL_TIME_ELAPSED queries using a ring of 4 query objects, and read results 3 frames later so the pipeline never stalls. Report the GPU ms in r_speeds 8, the BM-2 CSV and the BM-16 overlay. This shows whether a scene is CPU-bound or GPU-bound. Today's 'bk' value is CPU submit time only, because with r_finish 0 the 3D path never calls glFinish.

- **Player-visible effect:** None unless shown in the overlay.
- **Expected gain:** Separates GPU-bound from CPU-bound per demo and preset, so the owner optimizes the right side (for example glow passes versus draw-call overhead). Confidence: high on Windows desktop drivers.
- **Risk:** Driver quirks on very old GPUs, so gate on the extension. Avoid reading results synchronously.
- **Prerequisites:** BM-1
- **Evidence:** `src/renderer/glext.h:1530 (GL_TIME_ELAPSED already defined)`; `src/renderer/tr_backend.cpp:430-436, 1268-1270 (glFinish skipped when r_finish 0)`; `src/renderer/tr_backend.cpp:1400-1436`

### BM-7 — Server frame statistics: `serverstats` command and per-second CSV
*tooling · effort M · server-only-transparent*

Instrument SV_Frame (sv_main_frame.h:57-175) with us timestamps per stage:
- packet processing (accumulated in Com_RunAndTimeServerPacket, common.cpp:2300-2318)
- SV_CalcPings
- SV_BotFrame (BOTAI_START_FRAME)
- the GAME_RUN_FRAME loop, with its iteration count (more than one iteration means the server is catching up)
- SV_SendClientMessages, split into snapshot build and encode+send
- timeouts and master heartbeats

Keep the last 8192 frames in a ring (409 s at sv_fps 20). Add:
- `serverstats [seconds]` (60 by default): avg/p50/p95/p99/max per stage over the last N seconds, frames over budget (1000/sv_fps ms), catch-up frames (frames paced at sv_hibernateFps aside), full and fallback snapshots with their bytes, bytes/s out, average entities per snapshot, humans and bots.
- `sv_statsLog 1`: appends one CSV line per second to svstats.csv in fs_homepath.

The statistics are always kept, for a few clock reads per frame; the log defaults off, and nothing changes on the wire.

- **Player-visible effect:** None. Server admins get a real health readout.
- **Expected gain:** Turns server load testing from guessing at CPU% into per-stage budgets. Verified that com_speeds reports 0 ms per frame here, so today there is no signal at all. Confidence: high.
- **Risk:** Low. Keep the bookkeeping allocation-free per frame.
- **Prerequisites:** BM-1
- **Evidence:** `src/server/sv_main_frame.h:57-175`; `src/server/sv_main_frame.h:150-158 (catch-up while loop)`; `src/qcommon/common.cpp:2300-2318`; `src/qcommon/common.cpp:2794-2801 (only >500 ms hitch warning exists)`; `src/server/sv_ccmds_registration.h:30-54 (no stats command)`

### BM-8 — Bot-based server load test protocol (real assets) with the documented gotchas
*tooling · effort S · server-only-transparent*

Command:
`jk2mvded +set dedicated 1 +set fs_homepath <dir> +set sv_maxclients 32 +set sv_fps 20 +set sv_hibernateFps 0 +set g_gametype 0 +set fraglimit 0 +set timelimit 0 +set logfile 2 +map ffa_bespin +exec load.cfg`.

load.cfg ramps in plateaus of 8, 16, 24 and 31 bots. Each plateau is `addbot <name> 4` lines (names from `botlist`) with `wait 40` between them, then `wait 1200` (60 s at 20 Hz), then `serverstats` (after BM-7), then the next step. Repeat at sv_fps 40.

Until BM-7 exists, sample process CPU with `typeperf "\Process(jk2mvded)\% Processor Time" -si 1 -o cpu.csv`. ms per server frame = CPU% / 100 * 1000 / sv_fps.

Gotchas, all verified here:
(a) Bots are not counted as players, so a bot-only server hibernates at 4 wakeups/s unless sv_hibernateFps is 0.
(b) Without `dedicated 1` the server heartbeats to 3 public masters.
(c) More than 32 '+' arguments are silently dropped, so script through +exec.
(d) Adding bots without waits kicks them with 'Server command overflow'.
(e) Bots use botroutes/<map>.wnt (shipped with retail maps). AAS is irrelevant.
(f) Bots do not exercise snapshot encoding or netchan; use BM-9 or BM-10 for that.
(g) ERR_DROP exits 0, so check the log for errors.

To find hotspots, profile the same plateau with `perf record -g` (Linux) or WPR/WPA (Windows, RelWithDebInfo PDBs).

- **Player-visible effect:** None. It produces 'max players at sv_fps X' numbers to optimize against.
- **Expected gain:** Repeatable server CPU scaling curves (CPU per frame against bot count), measurable in an afternoon. On the stub map here, 31 bots cost about 1.7 ms per 50 ms frame against 0.93 ms idle (rough, QVM, trivial map; real maps will cost more). Confidence: high for the method.
- **Risk:** Bots do not represent humans: no network encode, different AI cost. Combine with BM-9 or BM-10.
- **Evidence:** `src/server/sv_main_frame.h:12-28, 83-97 (hibernation ignores bots)`; `src/server/sv_init_lifecycle.h:89-94, 107`; `src/qcommon/common.cpp:2579 (dedicated ROM 2 in jk2mvded)`; `src/qcommon/common.cpp:437-456 (MAX_CONSOLE_LINES 32)`; `src/server/sv_main_messaging.h:71-77; src/server/sv_bot_lifecycle.h:166-190; src/mvsdk/code/game/ai_main.c:695`; `src/mvsdk/code/game/ai_wpnav.c:1663; ai_main.c:949-960`; `src/server/sv_snapshot_sender.h:108-110`; `Verified runs: 31 +addbot on the command line gave 23 bots; exec'd addbots without waits caused 7 overflow kicks; with 'wait 5' all 31 stayed`

### BM-9 — sv_benchBotSnapshots: encode bot snapshots and discard them
*tooling · effort S · server-only-transparent*

Add a dev cvar, default 0. When it is 1, SV_SendClientSnapshot (sv_snapshot_sender.h:98-150) also serializes each bot's snapshot into a scratch msg_t (entity deltas plus Huffman sizing) before returning, then discards it. Encoded bytes go into the BM-7 statistics. Use non-delta or a private delta base so no client state (frames[], nextSnapshotTime, reliable sequence) is changed. A 31-bot test then approximates the per-client encode cost and bandwidth of a full human server without 31 real clients.

- **Player-visible effect:** None.
- **Expected gain:** Covers the snapshot encode path, the main per-client cost on busy idTech3 servers, in a single-process test that runs in CI (BM-11). Confidence: medium, because encode cost depends on delta bases that the bots' fake ack pattern only approximates.
- **Risk:** Must not touch client_t state. Keep it in a dev-only path.
- **Prerequisites:** BM-7
- **Evidence:** `src/server/sv_snapshot_sender.h:98-150`; `src/server/sv_snapshot_sender.h:108-110 (bots return before encode)`

### BM-10 — Real-client network load with headless clients
*tooling · effort S · server-only-transparent*

Use this to measure snapshot build plus encode, netchan, bandwidth and rate behaviour with real clients (retail assets needed, because cgame needs player models).

Linux: run N copies of
`SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 ./jk2mvmp +set net_ip 127.0.0.$i +set r_allowsoftwaregl 1 +set r_fullscreen 0 +set r_mode 3 +set s_initsound 0 +set com_maxfps 30 +set rate 90000 +set snaps 40 +set name load$i +connect 127.0.0.1`
with a movement alias loop (`set m "+forward; wait 60; -forward; +moveleft; wait 30; -moveleft; +attack; wait 5; -attack; vstr m"; vstr m`). The SDL offscreen + llvmpipe path is verified to start GL here. A distinct loopback IP per client avoids the 10 packets/s per-IP connectionless limit and the 10-port scan limit.

Windows: start 4-8 windowed instances with explicit `+set net_port 2907x` and `timeout /t 3` between launches.

Read server-side cost with BM-7.

- **Player-visible effect:** None.
- **Expected gain:** The only realistic test of the network path short of real players. Limited by the CPU cost of llvmpipe clients (roughly 8-16 per desktop, estimate). Confidence: medium.
- **Risk:** Client CPU can perturb the server measurement on the same host, so pin the server to its own cores (taskset or start /affinity).
- **Prerequisites:** BM-7
- **Evidence:** `src/server/sv_main_connectionless.h:298 (SVC_RateLimitAddress 10/1000ms per IP)`; `src/qcommon/net_ip.cpp:815-831 (10-port scan)`; `src/qcommon/net_ip.cpp:862 (net_ip)`; `src/sdl/sdl_window.cpp:814, 1018 (r_allowsoftwaregl)`; `Verified: offscreen client log 'GL_RENDERER: llvmpipe (LLVM 20.1.2, 256 bits)'`

### BM-11 — CI: asset-free dedicated server smoke test and bot soak (verified here)
*tooling · effort M · server-only-transparent*

Add tests/smoke/make_stub_base.py, which generates a GPL-clean base/ at build time with no binaries checked in:
- assets5.pk3: any zip
- mpdefault.cfg
- strip/{CON_TEXT,MP_INGAME,MP_SVGAME,SP_INGAME,STR_SERVER}.sp, each 'VERSION 1\nID n\nREFERENCE X\nCOUNT 0'
- botfiles/bots.txt with 31 entries {name "Bxx" model "kyle" personality "botfiles/default.jkb"}
- maps/ci_box.bsp, a minimal RBSP v1 file of 856 bytes: header 'RBSP', version 1, 18 lumps; 1 dshader_t (CONTENTS_SOLID); 12 dplane_t pairs; 1 dnode_t splitting z=0 into 2 dleaf_t that both reference 1 dbrush_t with 6 axial dbrushside_t; 1 dmodel_t; entities with worldspawn and 2 info_player_deathmatch.
Also copy the build's assetsmv.pk3 and assetsmv2.pk3 (mvsdk QVMs).

Add tests/smoke/server_smoke.sh running:
`jk2mvded +set dedicated 1 +set fs_basepath $D +set fs_homepath $D +set ttycon 0 +set sv_hibernateFps 0 +set sv_maxclients 32 +map ci_box +exec smoke.cfg`
smoke.cfg adds 31 bots with waits, runs 60 s, does a map_restart and a `map ci_box` reload, then status and quit.

Assertions:
- exit code 0
- no 'ERROR', 'Server crashed', 'overflow' or 'recursive error' in the log
- status lists 31 bots
- no crashlog-*.txt
Variants: the QVM path, and the native path with vm_game 0 and the .so copied into base/.

Register it with ctest (label 'smoke') and run it in a new tests.yml job. That workflow currently builds no engine at all.

- **Player-visible effect:** None. It prevents server crashes from reaching players.
- **Expected gain:** The first CI test that executes real engine code: filesystem, VM JIT and native loading, botlib, game module, collision, snapshot building, map reload. Verified end to end here in about 70 s per run. Confidence: high.
- **Risk:** The stub map is trivial, so it is not representative for performance. Keep the generator in sync if FS startup checks change.
- **Evidence:** `src/qcommon/files.cpp:3453 (only existence of assets5.pk3 checked), 3990 (mpdefault.cfg required)`; `src/qcommon/strip.cpp:1523-1525, 1753-1756; src/server/sv_init_lifecycle.h (SP_Register str_server REQUIRED)`; `src/qcommon/cm_load.cpp:76-82, 109-114, 166-171, 257-262, 305-310 (minimum lumps), src/qcommon/qfiles.h:292-294, 342-405`; `src/qcommon/vm.cpp:718-735; src/sys/sys_unix.cpp:725-760 (native .so searched in <basepath>/base, else QVM)`; `.github/workflows/tests.yml:46, 81 (engine targets OFF)`; `Verified locally: 'map: ci_box' status with 31 bots, exit 0; a missing map also exits 0 (ERR_DROP), mpdefault.cfg missing exits 3`

### BM-12 — CI: headless client smoke test via SDL offscreen and Mesa llvmpipe (verified here)
*tooling · effort S · client-only-transparent*

Generate the same stub base plus ui/menus.txt and ui/jk2mpmenus.txt containing '{ }'. Without them the jk2mvmenu UI_LoadMenus fails and the engine reports a 'recursive error'.

Run:
`SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 ./jk2mvmp +set fs_basepath $D +set fs_homepath $D +set r_allowsoftwaregl 1 +set r_fullscreen 0 +set r_mode 3 +set s_initsound 0 +set com_introplayed 1 +wait 60 +toggleconsole +wait 30 +screenshot_tga ci_console +vid_restart +wait 30 +screenshot_tga ci_after +quit`

Assert:
- exit 0
- GL_RENDERER contains llvmpipe
- both TGAs exist, are 640x480 and not uniform
- optionally, a fuzzy golden diff of the console/menu, pinned to the runner's Mesa version

This covers renderer init, extension detection, parsing of bundled shaders (jk2mv.shader_mv, sabers.dynGlow), fonts, the UI QVM/native module, the vid_restart cycle and screenshot I/O.

Plausible but unverified: a client-to-server handshake on ci_box (protocol 16, and 15 via mv_serverversion 1.02 with stub assets0/1 names) through gamestate.

Cannot be tested without real assets: entering a map in the client. cgame aborts with 'DEFAULT_MODEL (kyle) failed to register' because there is no player GLM and _humanoid skeleton. Faking those is an L effort and not recommended.

Run this on an ubuntu-24.04 host runner. The ubuntu:16.04 build containers ship a much older SDL2. xvfb-run failed here with 'Couldn't find matching GLX visual'.

- **Player-visible effect:** None. It catches renderer or UI startup crashes before release.
- **Expected gain:** Automated coverage of the GL startup path and the 2D/UI rendering that every player hits. Verified with SDL 2.30.0 and Mesa 25.2.8. Confidence: high on 24.04, medium on 22.04 (SDL 2.0.20).
- **Risk:** Golden images drift when Mesa versions change, so use a tolerance. llvmpipe tells you nothing about performance.
- **Prerequisites:** BM-11
- **Evidence:** `src/sdl/sdl_window.cpp:814, 960-984, 1018`; `src/renderer/tr_init.cpp:783-833, 1218-1219`; `Verified log: 'GL_RENDERER: llvmpipe', 'Wrote screenshots/ci_console.tga' (twice, before and after vid_restart)`; `Verified failure without stub menus: 'default menu file not found: ui/menus.txt' then 'recursive error'`; `Verified map entry failure: cgame CG_LoadClientInfo 'DEFAULT_MODEL (kyle) failed to register'`

### BM-13 — CI: sanitizer soak and static analysis jobs
*tooling · effort M · server-only-transparent*

Build jk2mvded and jk2mvmp with -fsanitize=address,undefined -fno-omit-frame-pointer (Debug or RelWithDebInfo). Run the BM-11 and BM-12 scenarios under them for a few minutes: 31 bots, repeated map_restart and map loads, vid_restart loops.

Add a clang-tidy job with bugprone-* and the sizeof-expression checks. One latent bug these checks would flag: vm.cpp:713 `Com_Memset(vm, 0, sizeof(vm))` zeroes only pointer-size bytes. It is harmless today only because VM_Free clears the whole struct (vm.cpp:812, 825).

The existing gtest suite does not compile engine code, so this is where real memory-safety coverage would come from.

- **Player-visible effect:** None. It means fewer crashes.
- **Expected gain:** Catches use-after-free, overflow and undefined behaviour in the real server, botlib, VM and renderer startup paths. These are the classes the fork already fixed by hand in earlier commits. Confidence: high.
- **Risk:** ASan memory overhead. The QVM JIT may need ASan suppressions, or run that leg with the native modules.
- **Prerequisites:** BM-11
- **Evidence:** `src/qcommon/vm.cpp:713 vs 812/825`; `tests/unit/*.cpp include only test_utils.h (22 files, about 729 TEST macros), so no engine code is linked`; `CMakeLists.txt:265-266 (only -Wall)`

### BM-14 — CI: deterministic instruction-count performance regression check (Callgrind)
*tooling · effort M · server-only-transparent*

Run the BM-11 soak with vm_game 0 and the native .so in base/ (to avoid JIT noise) for a fixed number of server frames under `valgrind --tool=callgrind --toggle-collect=SV_Frame`. Extract instructions per SV_Frame, and per VM_Call(GAME_RUN_FRAME) and SV_SendClientMessages. Compare with the baseline artifact from master and alert when the change exceeds 5-10%. Wall-clock time on shared GitHub runners varies by 10-30%, while instruction counts are stable to about 1%. Optionally add a client leg (renderer frontend on llvmpipe) once BM-12 exists.

- **Player-visible effect:** None.
- **Expected gain:** Catches server CPU regressions per PR without dedicated hardware. Basis: the same technique is used by SQLite (cachegrind) and the rustc perf suite. Confidence: medium (stub map workload only).
- **Risk:** About 50x slowdown, so keep N frames small (e.g., 600). The stub map underrepresents collision cost.
- **Prerequisites:** BM-11
- **Evidence:** `src/server/sv_main_frame.h:150-168`; `src/qcommon/vm.cpp:718-735 (native module path)`; `Verified run: 31 bots on ci_box at about 1.7 ms CPU per frame, so 600 frames under callgrind fit in minutes`

### BM-15 — Opt-in Tracy profiler build
*tooling · effort M · client-only-transparent*

Add a CMake option UseTracy (default OFF) with zones in: Com_Frame, SV_Frame stages, VM_Call (tagged by module and callnum), CL_ParseServerMessage, RE_RenderScene / R_RenderView, RB_ExecuteRenderCommands, R_DrawElements, S_Update and the Ghoul2 transform. Add TracyPlot for frame time and FrameMark at swap. GPU zones can follow later through GL timer queries. It gives the owner a live timeline and flame view on Windows with the real assets, which is the fastest way to pick the next optimization target.

- **Player-visible effect:** None (compiled out by default).
- **Expected gain:** Hotspot discovery in minutes instead of guesswork. Complements the aggregate numbers from BM-2 and BM-7. Confidence: high.
- **Risk:** None when OFF. Third-party dependency (BSD-licensed) as a submodule or FetchContent.
- **Evidence:** `No profiler integration in src/ (grep shows only Sys_Milliseconds-based timing)`; `.github/workflows/build.yml:26-60 (RelWithDebInfo builds already produced)`

### BM-16 — Opt-in performance overlay HUD to make improvements visible
*tooling · effort S · client-only-opt-in*

Add cl_perfOverlay 1/2. The engine (SCR layer, after cgame) draws a small corner panel: current fps, 1% low over the last 10 s, a frame-time graph in us (reusing the SCR_DebugGraph style), CPU frontend/backend ms, GPU ms (BM-6), and, with a local server, server frame ms (BM-7). It uses the existing console font (r_consoleFont). Unlike cg_drawFPS it does not depend on the mod and is not a 4-frame average, and unlike timegraph it is not cheat-protected or ms-quantized. Default 0.

- **Player-visible effect:** An optional MangoHud-style overlay. Players and the owner can see and screenshot the gains (fps, 1% lows) in before/after videos.
- **Expected gain:** Makes the performance work tangible and shareable, which serves the 'mouth-watering' goal. Draw cost is under 0.1 ms (a few dozen quads). Confidence: high.
- **Risk:** It must stay purely client-side and opt-in. Keep it off during benchmarks or account for its cost.
- **Prerequisites:** BM-1
- **Evidence:** `src/client/cl_scrn.cpp:403-407 (timegraph and friends are CVAR_CHEAT)`; `src/client/cl_main.cpp:2507-2509 (timegraph uses ms)`; `src/mvsdk/code/cgame/cg_draw.c:1809-1830 (cg_drawFPS 4-frame average, mod-side)`

### BM-17 — CI hygiene: retired runner images, and no engine in the tests workflow
*tooling · effort S · client-only-transparent*

build.yml still uses runs-on: windows-2019 (line 179, WinXP toolset job) and macos-12 (line 263). GitHub has retired both images (macos-12 in Dec 2024, windows-2019 by end of June 2025, medium-high confidence), so these jobs fail to start or queue forever.

Fixes:
- Move macOS to macos-13 (x86_64), or macos-14 arm64. arm64 uses NO_VM_COMPILED, so QVMs run through the interpreter, which is slower.
- Decide whether to keep XP support with a self-hosted runner or drop it.
- tests.yml configures with BuildMVMP/MVDED/MVSDK OFF (lines 46, 81). Add an engine build there, or a 'smoke' job hosting BM-11 to BM-14.

- **Player-visible effect:** None.
- **Expected gain:** Restores green CI on all platforms and makes engine-level verification part of every PR. Confidence: high for the tests.yml finding, medium-high for the retirement dates.
- **Risk:** Dropping the XP build is a product decision for the owner.
- **Evidence:** `.github/workflows/build.yml:179, 263`; `.github/workflows/tests.yml:46, 81`; `CMakeLists.txt (mvsdk arm64 sets NO_VM_COMPILED in src/mvsdk/CMakeLists.txt)`

### BM-18 — Docs: fix CVARS.rst drift around hibernation and document the test gotchas
*tooling · effort S · server-only-transparent*

CVARS.rst documents sv_hibernateTime ('value zero disables hibernation'), but the code never registers it. It gives sv_hibernateFps a default of '5', while the code registers '4'. It does not say that bots count as 'no players'. Fix the entries, document the `dedicated 1` and 32-argument command-line limits for scripted runs, and link the benchmark and load-test docs from README.

- **Player-visible effect:** None.
- **Expected gain:** Prevents misleading bot load tests where the server silently runs at 4 wakeups/s. Confidence: high.
- **Risk:** None.
- **Evidence:** `CVARS.rst (sv_hibernateTime / sv_hibernateFps entries)`; `src/server/sv_init_lifecycle.h:107`; `grep: no Cvar_Get for sv_hibernateTime anywhere in src/`; `src/server/sv_main_frame.h:83-97`

**Notes.** Recommended order:
1. BM-3 now. It needs no code, so the owner gets baseline numbers before any optimization lands.
2. BM-1, then BM-2, then BM-5, before the renderer and VBO work, so each perf-3D PR can be proven with percentiles and draw-call counts.
3. BM-7 and BM-8 before any server or entity-scaling work.
4. BM-11 and BM-12 in CI right away. Both were verified end to end in this container. Then BM-13 and BM-14.
5. BM-16 later, as a visible extra that showcases the gains.

What I actually ran (everything in the scratchpad; the repo and git state are untouched; leftover jk2mvded processes killed):
- jk2mvded init, map and quit on a generated 856-byte BSP, with 4, 16 and 31 bots.
- com_speeds sampling.
- Exit codes: 0 on success, 0 on ERR_DROP, 3 on ERR_FATAL.
- Heartbeat suppression with dedicated 1.
- The 32-argument truncation and the 'Server command overflow' bot kicks.
- The jk2mvmp offscreen llvmpipe client: renderer up, UI loaded, console screenshot written and visually checked, vid_restart OK. Map entry blocked by the cgame player-model check.
- The generator script and stub layout are in scratchpad/mkbsp.py and scratchpad/ded/base (ephemeral). The description in BM-11 is enough to recreate them.

Corrections to the brief:
- JK2 MP bots do not need .aas files. The game navigates with botroutes/*.wnt and never calls trap_BotLibLoadMap.
- The existing unit tests do not exercise engine code.

Not verified (no assets or GPU here):
- PresentMon/CapFrameX behaviour on JK2's OpenGL path.
- The nextdemo-then-disconnect ordering issue (cl_main.cpp:489-490). It follows from the code, but I could not run it.
- That the client handshake test works for protocols 15 and 16.

Out-of-area findings worth passing on:
- vm.cpp:713 has a sizeof(pointer) memset, which is latent and harmless today.
- Bots ack reliable commands only through game AI draining. Mass bot joins or many simultaneous joins can therefore overflow MAX_RELIABLE_COMMANDS (128) and kick bots. A server-side fix, auto-acking NA_BOT clients when overflow is imminent, belongs to the server-robustness area.
- Sys_Milliseconds on Unix uses non-monotonic gettimeofday.

A headless 'load client' build target (client code with a null refexport, about L effort) is an alternative to BM-10. It is only worth it if many more than about 16 simulated clients per box are needed.
