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

if(UNIX)
  add_jk2mv_test(test_sys_spawn
    test_sys_spawn.cpp
    ${CMAKE_SOURCE_DIR}/src/sys/sys_spawn.cpp
  )
endif()

# ==============================================================================
# qcommon: netchan Huffman tables (links the real engine sources)
# ==============================================================================

add_jk2mv_test(test_qcommon_huffman
  test_qcommon_huffman.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/huffman.cpp
  ${CMAKE_SOURCE_DIR}/src/qcommon/msg.cpp
)
