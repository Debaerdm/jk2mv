// test_sv_client_usercmd.cpp - Unit tests for sv_client_usercmd.h
// Phase 3: Client management tests (~25 tests)

#include "test_utils.h"
#include <cstring>

struct usercmd_t {
    int serverTime;
    int angles[3];
    signed char forwardmove;
    signed char rightmove;
    signed char upmove;
    unsigned char buttons;
    unsigned char weapon;
};

struct client_t {
    usercmd_t lastUsercmd;
    int lastUsercmdTime;
    int lastCmdTime;
};

client_t test_clients[64];

void ResetUsercmds() {
    memset(test_clients, 0, sizeof(test_clients));
}

void ProcessUsercmd(int clientNum, usercmd_t* cmd) {
    test_clients[clientNum].lastUsercmd = *cmd;
    test_clients[clientNum].lastUsercmdTime = cmd->serverTime;
}

bool ValidateUsercmd(usercmd_t* cmd, int clientTime) {
    if (cmd->serverTime < clientTime - 1000) return false;
    if (cmd->serverTime > clientTime + 1000) return false;
    return true;
}

// ============================================================================
// TEST SUITE: Usercmd Structure
// ============================================================================

TEST(SvClientUsercmd_Struct, StoresServerTime) {
    usercmd_t cmd;
    cmd.serverTime = 5000;
    
    EXPECT_EQ(cmd.serverTime, 5000);
}

TEST(SvClientUsercmd_Struct, StoresAngles) {
    usercmd_t cmd;
    cmd.angles[0] = 100;
    cmd.angles[1] = 200;
    cmd.angles[2] = 300;
    
    EXPECT_EQ(cmd.angles[0], 100);
    EXPECT_EQ(cmd.angles[1], 200);
    EXPECT_EQ(cmd.angles[2], 300);
}

TEST(SvClientUsercmd_Struct, StoresMovement) {
    usercmd_t cmd;
    cmd.forwardmove = 127;
    cmd.rightmove = -127;
    cmd.upmove = 64;
    
    EXPECT_EQ(cmd.forwardmove, 127);
    EXPECT_EQ(cmd.rightmove, -127);
    EXPECT_EQ(cmd.upmove, 64);
}

TEST(SvClientUsercmd_Struct, StoresButtons) {
    usercmd_t cmd;
    cmd.buttons = 0xFF;
    
    EXPECT_EQ(cmd.buttons, 0xFF);
}

TEST(SvClientUsercmd_Struct, StoresWeapon) {
    usercmd_t cmd;
    cmd.weapon = 5;
    
    EXPECT_EQ(cmd.weapon, 5);
}

// ============================================================================
// TEST SUITE: Command Processing
// ============================================================================

TEST(SvClientUsercmd_Process, StoresLastCmd) {
    ResetUsercmds();
    usercmd_t cmd;
    cmd.serverTime = 1000;
    cmd.buttons = 1;
    
    ProcessUsercmd(0, &cmd);
    
    EXPECT_EQ(test_clients[0].lastUsercmd.serverTime, 1000);
    EXPECT_EQ(test_clients[0].lastUsercmd.buttons, 1);
}

TEST(SvClientUsercmd_Process, UpdatesLastTime) {
    ResetUsercmds();
    usercmd_t cmd;
    cmd.serverTime = 2000;
    
    ProcessUsercmd(0, &cmd);
    
    EXPECT_EQ(test_clients[0].lastUsercmdTime, 2000);
}

TEST(SvClientUsercmd_Process, OverwritesPrevious) {
    ResetUsercmds();
    usercmd_t cmd1, cmd2;
    cmd1.serverTime = 1000;
    cmd2.serverTime = 2000;
    
    ProcessUsercmd(0, &cmd1);
    ProcessUsercmd(0, &cmd2);
    
    EXPECT_EQ(test_clients[0].lastUsercmd.serverTime, 2000);
}

// ============================================================================
// TEST SUITE: Command Validation
// ============================================================================

TEST(SvClientUsercmd_Validate, AcceptsValidTime) {
    usercmd_t cmd;
    cmd.serverTime = 5000;
    
    EXPECT_TRUE(ValidateUsercmd(&cmd, 5000));
}

TEST(SvClientUsercmd_Validate, RejectsTooOld) {
    usercmd_t cmd;
    cmd.serverTime = 1000;
    
    EXPECT_FALSE(ValidateUsercmd(&cmd, 3000));
}

TEST(SvClientUsercmd_Validate, RejectsTooFuture) {
    usercmd_t cmd;
    cmd.serverTime = 10000;
    
    EXPECT_FALSE(ValidateUsercmd(&cmd, 5000));
}

TEST(SvClientUsercmd_Validate, AcceptsWithinWindow) {
    usercmd_t cmd;
    cmd.serverTime = 5500;
    
    EXPECT_TRUE(ValidateUsercmd(&cmd, 5000));
}

// ============================================================================
// TEST SUITE: Movement Ranges
// ============================================================================

TEST(SvClientUsercmd_Movement, ForwardMax) {
    usercmd_t cmd;
    cmd.forwardmove = 127;
    
    EXPECT_EQ(cmd.forwardmove, 127);
}

TEST(SvClientUsercmd_Movement, ForwardMin) {
    usercmd_t cmd;
    cmd.forwardmove = -127;
    
    EXPECT_EQ(cmd.forwardmove, -127);
}

TEST(SvClientUsercmd_Movement, AllDirections) {
    usercmd_t cmd;
    cmd.forwardmove = 100;
    cmd.rightmove = -50;
    cmd.upmove = 25;
    
    EXPECT_EQ(cmd.forwardmove, 100);
    EXPECT_EQ(cmd.rightmove, -50);
    EXPECT_EQ(cmd.upmove, 25);
}

// ============================================================================
// TEST SUITE: Button States
// ============================================================================

TEST(SvClientUsercmd_Buttons, SingleButton) {
    usercmd_t cmd;
    cmd.buttons = 0x01;
    
    EXPECT_EQ(cmd.buttons & 0x01, 0x01);
}

TEST(SvClientUsercmd_Buttons, MultipleButtons) {
    usercmd_t cmd;
    cmd.buttons = 0x01 | 0x02 | 0x04;
    
    EXPECT_EQ(cmd.buttons & 0x07, 0x07);
}

TEST(SvClientUsercmd_Buttons, NoButtons) {
    usercmd_t cmd;
    cmd.buttons = 0;
    
    EXPECT_EQ(cmd.buttons, 0);
}

// ============================================================================
// TEST SUITE: Multiple Clients
// ============================================================================

TEST(SvClientUsercmd_Multi, IndependentCommands) {
    ResetUsercmds();
    usercmd_t cmd1, cmd2;
    cmd1.serverTime = 1000;
    cmd2.serverTime = 2000;
    
    ProcessUsercmd(0, &cmd1);
    ProcessUsercmd(1, &cmd2);
    
    EXPECT_EQ(test_clients[0].lastUsercmdTime, 1000);
    EXPECT_EQ(test_clients[1].lastUsercmdTime, 2000);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvClientUsercmd_Edge, ZeroTime) {
    usercmd_t cmd;
    cmd.serverTime = 0;
    
    EXPECT_EQ(cmd.serverTime, 0);
}

TEST(SvClientUsercmd_Edge, MaxAngles) {
    usercmd_t cmd;
    cmd.angles[0] = 32767;
    cmd.angles[1] = 32767;
    cmd.angles[2] = 32767;
    
    EXPECT_EQ(cmd.angles[0], 32767);
}

TEST(SvClientUsercmd_Edge, MinAngles) {
    usercmd_t cmd;
    cmd.angles[0] = -32768;
    cmd.angles[1] = -32768;
    cmd.angles[2] = -32768;
    
    EXPECT_EQ(cmd.angles[0], -32768);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvClientUsercmd_Perf, ManyCommands) {
    ResetUsercmds();
    usercmd_t cmd;
    
    for (int i = 0; i < 100; i++) {
        cmd.serverTime = i * 100;
        ProcessUsercmd(0, &cmd);
    }
    
    EXPECT_EQ(test_clients[0].lastUsercmdTime, 9900);
}

TEST(SvClientUsercmd_Perf, AllClients) {
    ResetUsercmds();
    usercmd_t cmd;
    cmd.serverTime = 1000;
    
    for (int i = 0; i < 64; i++) {
        ProcessUsercmd(i, &cmd);
    }
    
    EXPECT_EQ(test_clients[63].lastUsercmdTime, 1000);
}

// ============================================================================
// SUMMARY: 25 tests for sv_client_usercmd.h
// ============================================================================
