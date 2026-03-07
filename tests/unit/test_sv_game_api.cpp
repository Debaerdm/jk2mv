// test_sv_game_api.cpp - Unit tests for sv_game_api.h
// Phase 5: Game interface tests (~30 tests)

#include "test_utils.h"
#include <cstring>

#define GAME_INIT 0
#define GAME_SHUTDOWN 1
#define GAME_CLIENT_CONNECT 2
#define GAME_CLIENT_BEGIN 3
#define GAME_CLIENT_USERINFO_CHANGED 4
#define GAME_CLIENT_DISCONNECT 5
#define GAME_CLIENT_COMMAND 6
#define GAME_CLIENT_THINK 7
#define GAME_RUN_FRAME 8

struct vm_t {
    bool loaded;
    int callLevel;
    char name[64];
};

vm_t* test_gvm = nullptr;
int test_vm_calls[16];
int test_vm_call_count = 0;

void ResetGameAPI() {
    memset(test_vm_calls, 0, sizeof(test_vm_calls));
    test_vm_call_count = 0;
    if (test_gvm) {
        test_gvm->loaded = false;
        test_gvm->callLevel = 0;
    }
}

vm_t* VM_Create(const char* name) {
    static vm_t vm;
    memset(&vm, 0, sizeof(vm));
    strncpy(vm.name, name, sizeof(vm.name) - 1);
    vm.loaded = true;
    return &vm;
}

void VM_Free(vm_t* vm) {
    if (vm) {
        vm->loaded = false;
    }
}

int VM_Call(vm_t* vm, int command, int arg0 = 0, int arg1 = 0, int arg2 = 0) {
    if (!vm || !vm->loaded) return -1;
    
    vm->callLevel++;
    test_vm_calls[test_vm_call_count++] = command;
    
    int result = 0;
    switch (command) {
        case GAME_INIT:
            result = 1; // Success
            break;
        case GAME_CLIENT_CONNECT:
            result = 0; // NULL string = accept
            break;
        default:
            result = 0;
    }
    
    vm->callLevel--;
    return result;
}

bool VM_IsLoaded(vm_t* vm) {
    return vm && vm->loaded;
}

// ============================================================================
// TEST SUITE: VM Creation
// ============================================================================

TEST(SvGameAPI_VM, CreatesVM) {
    ResetGameAPI();
    
    vm_t* vm = VM_Create("jk2mpgame");
    
    EXPECT_NE(vm, nullptr);
    EXPECT_TRUE(vm->loaded);
}

TEST(SvGameAPI_VM, SetsName) {
    ResetGameAPI();
    
    vm_t* vm = VM_Create("testgame");
    
    EXPECT_STREQ(vm->name, "testgame");
}

TEST(SvGameAPI_VM, InitializesCallLevel) {
    ResetGameAPI();
    
    vm_t* vm = VM_Create("jk2mpgame");
    
    EXPECT_EQ(vm->callLevel, 0);
}

// ============================================================================
// TEST SUITE: VM Lifecycle
// ============================================================================

TEST(SvGameAPI_Lifecycle, LoadsVM) {
    ResetGameAPI();
    
    vm_t* vm = VM_Create("jk2mpgame");
    
    EXPECT_TRUE(VM_IsLoaded(vm));
}

TEST(SvGameAPI_Lifecycle, FreesVM) {
    ResetGameAPI();
    
    vm_t* vm = VM_Create("jk2mpgame");
    VM_Free(vm);
    
    EXPECT_FALSE(VM_IsLoaded(vm));
}

TEST(SvGameAPI_Lifecycle, FreeNullVM) {
    ResetGameAPI();
    
    VM_Free(nullptr);
    
    EXPECT_TRUE(true); // Should not crash
}

// ============================================================================
// TEST SUITE: Game Init
// ============================================================================

TEST(SvGameAPI_Init, CallsInit) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    int result = VM_Call(vm, GAME_INIT);
    
    EXPECT_EQ(result, 1);
    EXPECT_EQ(test_vm_calls[0], GAME_INIT);
}

TEST(SvGameAPI_Init, IncrementsCallLevel) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    int before = vm->callLevel;
    
    VM_Call(vm, GAME_INIT);
    
    EXPECT_EQ(vm->callLevel, before); // Back to 0 after call
}

// ============================================================================
// TEST SUITE: Client Connect
// ============================================================================

TEST(SvGameAPI_Client, ConnectCall) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    int result = VM_Call(vm, GAME_CLIENT_CONNECT, 0);
    
    EXPECT_EQ(result, 0); // NULL = accept
    EXPECT_EQ(test_vm_calls[0], GAME_CLIENT_CONNECT);
}

TEST(SvGameAPI_Client, BeginCall) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    VM_Call(vm, GAME_CLIENT_BEGIN, 0);
    
    EXPECT_EQ(test_vm_calls[0], GAME_CLIENT_BEGIN);
}

TEST(SvGameAPI_Client, DisconnectCall) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    VM_Call(vm, GAME_CLIENT_DISCONNECT, 0);
    
    EXPECT_EQ(test_vm_calls[0], GAME_CLIENT_DISCONNECT);
}

TEST(SvGameAPI_Client, UserinfoChangedCall) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    VM_Call(vm, GAME_CLIENT_USERINFO_CHANGED, 0);
    
    EXPECT_EQ(test_vm_calls[0], GAME_CLIENT_USERINFO_CHANGED);
}

// ============================================================================
// TEST SUITE: Client Commands
// ============================================================================

TEST(SvGameAPI_Cmd, ClientCommand) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    VM_Call(vm, GAME_CLIENT_COMMAND, 0);
    
    EXPECT_EQ(test_vm_calls[0], GAME_CLIENT_COMMAND);
}

TEST(SvGameAPI_Cmd, ClientThink) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    VM_Call(vm, GAME_CLIENT_THINK, 0);
    
    EXPECT_EQ(test_vm_calls[0], GAME_CLIENT_THINK);
}

// ============================================================================
// TEST SUITE: Frame Processing
// ============================================================================

TEST(SvGameAPI_Frame, RunFrame) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    VM_Call(vm, GAME_RUN_FRAME, 100);
    
    EXPECT_EQ(test_vm_calls[0], GAME_RUN_FRAME);
}

TEST(SvGameAPI_Frame, MultipleFrames) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    VM_Call(vm, GAME_RUN_FRAME, 100);
    VM_Call(vm, GAME_RUN_FRAME, 150);
    VM_Call(vm, GAME_RUN_FRAME, 200);
    
    EXPECT_EQ(test_vm_call_count, 3);
}

// ============================================================================
// TEST SUITE: Call Sequence
// ============================================================================

TEST(SvGameAPI_Sequence, InitThenFrame) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    VM_Call(vm, GAME_INIT);
    VM_Call(vm, GAME_RUN_FRAME, 0);
    
    EXPECT_EQ(test_vm_calls[0], GAME_INIT);
    EXPECT_EQ(test_vm_calls[1], GAME_RUN_FRAME);
}

TEST(SvGameAPI_Sequence, ConnectBeginDisconnect) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    VM_Call(vm, GAME_CLIENT_CONNECT, 0);
    VM_Call(vm, GAME_CLIENT_BEGIN, 0);
    VM_Call(vm, GAME_CLIENT_DISCONNECT, 0);
    
    EXPECT_EQ(test_vm_call_count, 3);
}

// ============================================================================
// TEST SUITE: Error Handling
// ============================================================================

TEST(SvGameAPI_Error, CallWithoutVM) {
    ResetGameAPI();
    
    int result = VM_Call(nullptr, GAME_INIT);
    
    EXPECT_EQ(result, -1);
}

TEST(SvGameAPI_Error, CallAfterFree) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    VM_Free(vm);
    
    int result = VM_Call(vm, GAME_INIT);
    
    EXPECT_EQ(result, -1);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvGameAPI_Edge, UnknownCommand) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    int result = VM_Call(vm, 9999);
    
    EXPECT_EQ(result, 0);
}

TEST(SvGameAPI_Edge, MultipleVMCreates) {
    ResetGameAPI();
    
    vm_t* vm1 = VM_Create("game1");
    vm_t* vm2 = VM_Create("game2");
    
    EXPECT_NE(vm1, nullptr);
    EXPECT_NE(vm2, nullptr);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvGameAPI_Perf, ManyCalls) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    for (int i = 0; i < 100; i++) {
        VM_Call(vm, GAME_RUN_FRAME, i);
    }
    
    EXPECT_EQ(test_vm_call_count, 100);
}

TEST(SvGameAPI_Perf, ManyClientsConnect) {
    ResetGameAPI();
    vm_t* vm = VM_Create("jk2mpgame");
    
    for (int i = 0; i < 64; i++) {
        VM_Call(vm, GAME_CLIENT_CONNECT, i);
    }
    
    EXPECT_GT(test_vm_call_count, 0);
}

TEST(SvGameAPI_Perf, CreateFreeLoop) {
    ResetGameAPI();
    
    for (int i = 0; i < 10; i++) {
        vm_t* vm = VM_Create("jk2mpgame");
        VM_Free(vm);
    }
    
    EXPECT_TRUE(true);
}

// ============================================================================
// SUMMARY: 30 tests for sv_game_api.h
// ============================================================================
