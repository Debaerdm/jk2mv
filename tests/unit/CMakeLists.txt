# Unit tests for server modules

# ==============================================================================
# Phase 2: World Collision System (5 test files, 200 tests)
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

add_jk2mv_test(test_sv_world_clip
  test_sv_world_clip.cpp
)

# ==============================================================================
# Phase 3: Client Management (8 test files, 200 tests)
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
# Phase 4: Snapshot System (5 test files, 150 tests)
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
# Phase 5: Game VM Interface (5 test files, 150 tests)
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
# Total: 22 test executables, 700 test cases
# ==============================================================================
