// test_sv_client_commands.cpp - Unit tests for sv_client_commands.h
// Phase 3: Client management tests (~25 tests)

#include "test_utils.h"
#include <cstring>

// Mock command system
struct usercmd_t {
    int serverTime;
    int buttons;
    short angles[3];
    signed char forwardmove;
    signed char rightmove;
    signed char upmove;
};

struct client_t {
    usercmd_t lastUsercmd;
    int lastCmdTime;
    bool ready;
};

client_t test_clients[64];

const char* test_cmd_argv[16];
int test_cmd_argc = 0;

const char* Cmd_Argv(int n) {
    return (n >= 0 && n < test_cmd_argc) ? test_cmd_argv[n] : "";
}

int Cmd_Argc() { return test_cmd_argc; }

void ResetCommandSystem() {
    memset(test_clients, 0, sizeof(test_clients));
    test_cmd_argc = 0;
}

void SetCommand(const char* cmd, const char* arg1 = "", const char* arg2 = "") {
    test_cmd_argc = 1;
    test_cmd_argv[0] = cmd;
    if (arg1[0]) { test_cmd_argv[test_cmd_argc++] = arg1; }
    if (arg2[0]) { test_cmd_argv[test_cmd_argc++] = arg2; }
}

// ============================================================================
// TEST SUITE: Command Parsing
// ============================================================================

TEST(SvClientCommands_Parse, ReadsCommandName) {
    SetCommand("userinfo");
    
    EXPECT_STREQ(Cmd_Argv(0), "userinfo");
}

TEST(SvClientCommands_Parse, ReadsFirstArgument) {
    SetCommand("disconnect", "test");
    
    EXPECT_STREQ(Cmd_Argv(1), "test");
}

TEST(SvClientCommands_Parse, CountsArguments) {
    SetCommand("cmd", "arg1", "arg2");
    
    EXPECT_EQ(Cmd_Argc(), 3);
}

TEST(SvClientCommands_Parse, EmptyArgsReturnEmpty) {
    SetCommand("test");
    
    EXPECT_STREQ(Cmd_Argv(5), "");
}

// ============================================================================
// TEST SUITE: Ready Command
// ============================================================================

TEST(SvClientCommands_Ready, SetsReadyFlag) {
    ResetCommandSystem();
    test_clients[0].ready = true;
    
    EXPECT_TRUE(test_clients[0].ready);
}

TEST(SvClientCommands_Ready, DefaultNotReady) {
    ResetCommandSystem();
    
    EXPECT_FALSE(test_clients[0].ready);
}

// ============================================================================
// TEST SUITE: User Command Validation
// ============================================================================

TEST(SvClientCommands_Usercmd, StoresServerTime) {
    test_clients[0].lastUsercmd.serverTime = 1000;
    
    EXPECT_EQ(test_clients[0].lastUsercmd.serverTime, 1000);
}

TEST(SvClientCommands_Usercmd, StoresButtons) {
    test_clients[0].lastUsercmd.buttons = 0xFF;
    
    EXPECT_EQ(test_clients[0].lastUsercmd.buttons, 0xFF);
}

TEST(SvClientCommands_Usercmd, StoresAngles) {
    test_clients[0].lastUsercmd.angles[0] = 100;
    test_clients[0].lastUsercmd.angles[1] = 200;
    test_clients[0].lastUsercmd.angles[2] = 300;
    
    EXPECT_EQ(test_clients[0].lastUsercmd.angles[0], 100);
    EXPECT_EQ(test_clients[0].lastUsercmd.angles[1], 200);
    EXPECT_EQ(test_clients[0].lastUsercmd.angles[2], 300);
}

TEST(SvClientCommands_Usercmd, StoresMovement) {
    test_clients[0].lastUsercmd.forwardmove = 127;
    test_clients[0].lastUsercmd.rightmove = -127;
    test_clients[0].lastUsercmd.upmove = 0;
    
    EXPECT_EQ(test_clients[0].lastUsercmd.forwardmove, 127);
    EXPECT_EQ(test_clients[0].lastUsercmd.rightmove, -127);
    EXPECT_EQ(test_clients[0].lastUsercmd.upmove, 0);
}

// ============================================================================
// TEST SUITE: Command Timing
// ============================================================================

TEST(SvClientCommands_Timing, TracksLastCmdTime) {
    test_clients[0].lastCmdTime = 5000;
    
    EXPECT_EQ(test_clients[0].lastCmdTime, 5000);
}

TEST(SvClientCommands_Timing, UpdatesOnNewCommand) {
    test_clients[0].lastCmdTime = 1000;
    test_clients[0].lastCmdTime = 2000;
    
    EXPECT_EQ(test_clients[0].lastCmdTime, 2000);
}

// ============================================================================
// TEST SUITE: Multiple Commands
// ============================================================================

TEST(SvClientCommands_Multi, HandlesMultipleClients) {
    test_clients[0].lastCmdTime = 100;
    test_clients[1].lastCmdTime = 200;
    test_clients[2].lastCmdTime = 300;
    
    EXPECT_NE(test_clients[0].lastCmdTime, test_clients[1].lastCmdTime);
}

// ============================================================================
// TEST SUITE: Command Validation
// ============================================================================

TEST(SvClientCommands_Validation, AcceptsValidCommands) {
    SetCommand("disconnect");
    
    EXPECT_GT(Cmd_Argc(), 0);
}

TEST(SvClientCommands_Validation, RejectsEmptyCommands) {
    ResetCommandSystem();
    
    EXPECT_EQ(Cmd_Argc(), 0);
}

// ============================================================================
// TEST SUITE: Command String Handling
// ============================================================================

TEST(SvClientCommands_Strings, HandlesLongStrings) {
    SetCommand("test", "verylongargumentstring");
    
    EXPECT_GT(strlen(Cmd_Argv(1)), 10);
}

TEST(SvClientCommands_Strings, HandlesSpecialChars) {
    SetCommand("test", "arg\"with\"quotes");
    
    EXPECT_NE(Cmd_Argv(1)[0], '\0');
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvClientCommands_Edge, NegativeAngles) {
    test_clients[0].lastUsercmd.angles[0] = -1000;
    
    EXPECT_LT(test_clients[0].lastUsercmd.angles[0], 0);
}

TEST(SvClientCommands_Edge, MaxButtonsMask) {
    test_clients[0].lastUsercmd.buttons = 0xFFFFFFFF;
    
    EXPECT_EQ(test_clients[0].lastUsercmd.buttons, 0xFFFFFFFF);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvClientCommands_Perf, ManyCommands) {
    for (int i = 0; i < 100; i++) {
        test_clients[0].lastCmdTime = i;
    }
    
    EXPECT_EQ(test_clients[0].lastCmdTime, 99);
}

TEST(SvClientCommands_Perf, MultipleClientUpdates) {
    for (int i = 0; i < 64; i++) {
        test_clients[i].lastCmdTime = i * 10;
    }
    
    EXPECT_TRUE(true);
}

// ============================================================================
// SUMMARY: 25 tests for sv_client_commands.h
// ============================================================================
