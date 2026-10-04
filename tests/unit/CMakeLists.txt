# Unit tests for server modules

# ==============================================================================
# Phase 2: World Collision System (4 test files)
# ==============================================================================

add_jk2mv_test(test_sv_world_sectors
  test_sv_world_sectors.cpp
)

add_jk2mv_test(test_sv_world_linking
  test_sv_world_linking.cpp
)

add_jk2mv_test(test_sv_world_area
  test_sv_world_area.cpp
)

add_jk2mv_test(test_sv_world_trace
  test_sv_world_trace.cpp
)

# the real world module and collision model, on maps built in memory
add_jk2mv_test(test_sv_world_clip
  test_sv_world_clip.cpp
  ${CMAKE_SOURCE_DIR}/src/server/sv_world.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/cm_load.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/cm_patch.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/cm_polylib.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/cm_test.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/cm_trace.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/q_math.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/q_shared.cpp
)

# ==============================================================================
# Phase 3: Client Management (8 test files)
# ==============================================================================

add_jk2mv_test(test_sv_client_challenge
  test_sv_client_challenge.cpp
)

add_jk2mv_test(test_sv_client_commands
  test_sv_client_commands.cpp
)

add_jk2mv_test(test_sv_client_connect
  test_sv_client_connect.cpp
)

add_jk2mv_test(test_sv_client_disconnect
  test_sv_client_disconnect.cpp
)

add_jk2mv_test(test_sv_client_download
  test_sv_client_download.cpp
)

add_jk2mv_test(test_sv_client_gamestate
  test_sv_client_gamestate.cpp
)

add_jk2mv_test(test_sv_client_usercmd
  test_sv_client_usercmd.cpp
)

add_jk2mv_test(test_sv_client_userinfo
  test_sv_client_userinfo.cpp
)

# usercmd accounting and sv_maxUsercmdRate (links the real engine source)
add_jk2mv_test(test_sv_usercmd_rate
  test_sv_usercmd_rate.cpp
  ${CMAKE_SOURCE_DIR}/src/server/sv_usercmd_rate.cpp
)

# ==============================================================================
# Phase 4: Snapshot System (5 test files)
# ==============================================================================

add_jk2mv_test(test_sv_snapshot_builder
  test_sv_snapshot_builder.cpp
)

add_jk2mv_test(test_sv_snapshot_delta
  test_sv_snapshot_delta.cpp
)

add_jk2mv_test(test_sv_snapshot_entities
  test_sv_snapshot_entities.cpp
)

add_jk2mv_test(test_sv_snapshot_sender
  test_sv_snapshot_sender.cpp
)

add_jk2mv_test(test_sv_snapshot_writer
  test_sv_snapshot_writer.cpp
)

# ==============================================================================
# Phase 5: Game VM Interface (5 test files)
# ==============================================================================

add_jk2mv_test(test_sv_game_api
  test_sv_game_api.cpp
)

add_jk2mv_test(test_sv_game_commands
  test_sv_game_commands.cpp
)

add_jk2mv_test(test_sv_game_entities
  test_sv_game_entities.cpp
)

add_jk2mv_test(test_sv_game_init
  test_sv_game_init.cpp
)

add_jk2mv_test(test_sv_game_shutdown
  test_sv_game_shutdown.cpp
)

# ==============================================================================
# Total: 22 test executables
# ==============================================================================

# ==============================================================================
# Renderer: alpha of the alpha to coverage images (links the real engine source)
# ==============================================================================

add_jk2mv_test(test_r_coverage
  test_r_coverage.cpp
  ${CMAKE_SOURCE_DIR}/src/renderer/tr_coverage.cpp
)

# Renderer: what the post-process gamma pass shows (color grade under r_fbo)
add_jk2mv_test(test_r_gammapass
  test_r_gammapass.cpp
  ${CMAKE_SOURCE_DIR}/src/renderer/tr_gammapass.cpp
)

# ==============================================================================
# Client: demo tools (links the real engine source)
# ==============================================================================

add_jk2mv_test(test_cl_campath
  test_cl_campath.cpp
  ${CMAKE_SOURCE_DIR}/src/client/cl_campath.cpp
)

add_jk2mv_test(test_cl_fov
  test_cl_fov.cpp
  ${CMAKE_SOURCE_DIR}/src/client/cl_fov.cpp
)

add_jk2mv_test(test_cl_videoclock
  test_cl_videoclock.cpp
)

add_jk2mv_test(test_cl_wav
  test_cl_wav.cpp
)

# ==============================================================================
# Renderer: dynamic light selection (links the real engine source)
# ==============================================================================

add_jk2mv_test(test_tr_dlightselect
  test_tr_dlightselect.cpp
  ${CMAKE_SOURCE_DIR}/src/renderer/tr_dlightselect.cpp
)

# ==============================================================================
# Server: snapshot entity ring (uses the real engine header)
# ==============================================================================

add_jk2mv_test(test_sv_snapshot_ring
  test_sv_snapshot_ring.cpp
)

# Server: frame statistics (links the real engine source)
add_jk2mv_test(test_sv_stats
  test_sv_stats.cpp
  ${CMAKE_SOURCE_DIR}/src/server/sv_stats.cpp
)

# ==============================================================================
# Server: server-side demos (links the real engine sources)
# ==============================================================================

add_jk2mv_test(test_sv_demo
  test_sv_demo.cpp
  ${CMAKE_SOURCE_DIR}/src/server/sv_demo.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/msg.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/huffman.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/q_shared.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/q_math.cpp
)

add_jk2mv_test(test_sv_demo_writer
  test_sv_demo_writer.cpp
  ${CMAKE_SOURCE_DIR}/src/server/sv_demo_writer.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/q_shared.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/q_math.cpp
)

# Process spawning for video_mp4 (links the real engine source)
add_jk2mv_test(test_sys_spawn
  test_sys_spawn.cpp
  ${CMAKE_SOURCE_DIR}/src/sys/sys_spawn.cpp
)

if(WIN32)
  # the program the Windows tests start, which reports what it got
  add_executable(test_sys_spawn_helper test_sys_spawn_helper.cpp)
  target_link_libraries(test_sys_spawn_helper PRIVATE shell32)
  add_dependencies(test_sys_spawn test_sys_spawn_helper)

  # the same tests without the handle list, as on Windows XP
  add_jk2mv_test(test_sys_spawn_winxp
    test_sys_spawn.cpp
    ${CMAKE_SOURCE_DIR}/src/sys/sys_spawn.cpp
  )
  target_compile_definitions(test_sys_spawn_winxp PRIVATE SYS_SPAWN_NO_HANDLE_LIST)
  add_dependencies(test_sys_spawn_winxp test_sys_spawn_helper)
endif()

# ==============================================================================
# qcommon: netchan Huffman tables (links the real engine sources)
# ==============================================================================

add_jk2mv_test(test_qcommon_huffman
  test_qcommon_huffman.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/huffman.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/msg.cpp
)
