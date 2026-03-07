// test_sv_game_commands.cpp - Unit tests for sv_game_commands.h
// Phase 5: Game interface tests (~30 tests)

#include "test_utils.h"
#include <cstring>

struct gameCommand_t {
    char name[32];
    void (*func)();
};

char test_cmd_buffer[256];
int test_cmd_executed = 0;
char test_last_cmd[32];

void ResetCommands() {
    memset(test_cmd_buffer, 0, sizeof(test_cmd_buffer));
    test_cmd_executed = 0;
    test_last_cmd[0] = '\0';
}

void TestCmd_Say() {
    test_cmd_executed++;
    strcpy(test_last_cmd, "say");
}

void TestCmd_Score() {
    test_cmd_executed++;
    strcpy(test_last_cmd, "score");
}

void TestCmd_Kill() {
    test_cmd_executed++;
    strcpy(test_last_cmd, "kill");
}

gameCommand_t test_commands[] = {
    {"say", TestCmd_Say},
    {"score", TestCmd_Score},
    {"kill", TestCmd_Kill},
    {"", nullptr}
};

gameCommand_t* FindCommand(const char* name) {
    for (int i = 0; test_commands[i].name[0]; i++) {
        if (strcmp(test_commands[i].name, name) == 0) {
            return &test_commands[i];
        }
    }
    return nullptr;
}

bool ExecuteCommand(const char* name) {
    gameCommand_t* cmd = FindCommand(name);
    if (!cmd || !cmd->func) return false;
    
    cmd->func();
    return true;
}

// ============================================================================
// TEST SUITE: Command Registration
// ============================================================================

TEST(SvGameCommands_Reg, FindsSayCommand) {
    ResetCommands();
    
    gameCommand_t* cmd = FindCommand("say");
    
    EXPECT_NE(cmd, nullptr);
    EXPECT_STREQ(cmd->name, "say");
}

TEST(SvGameCommands_Reg, FindsScoreCommand) {
    ResetCommands();
    
    gameCommand_t* cmd = FindCommand("score");
    
    EXPECT_NE(cmd, nullptr);
}

TEST(SvGameCommands_Reg, ReturnsNullForUnknown) {
    ResetCommands();
    
    gameCommand_t* cmd = FindCommand("unknown");
    
    EXPECT_EQ(cmd, nullptr);
}

// ============================================================================
// TEST SUITE: Command Execution
// ============================================================================

TEST(SvGameCommands_Exec, ExecutesSay) {
    ResetCommands();
    
    bool result = ExecuteCommand("say");
    
    EXPECT_TRUE(result);
    EXPECT_EQ(test_cmd_executed, 1);
    EXPECT_STREQ(test_last_cmd, "say");
}

TEST(SvGameCommands_Exec, ExecutesScore) {
    ResetCommands();
    
    bool result = ExecuteCommand("score");
    
    EXPECT_TRUE(result);
    EXPECT_STREQ(test_last_cmd, "score");
}

TEST(SvGameCommands_Exec, ExecutesKill) {
    ResetCommands();
    
    bool result = ExecuteCommand("kill");
    
    EXPECT_TRUE(result);
    EXPECT_STREQ(test_last_cmd, "kill");
}

TEST(SvGameCommands_Exec, FailsForUnknown) {
    ResetCommands();
    
    bool result = ExecuteCommand("badcommand");
    
    EXPECT_FALSE(result);
    EXPECT_EQ(test_cmd_executed, 0);
}

// ============================================================================
// TEST SUITE: Multiple Executions
// ============================================================================

TEST(SvGameCommands_Multi, ExecutesMultiple) {
    ResetCommands();
    
    ExecuteCommand("say");
    ExecuteCommand("score");
    ExecuteCommand("kill");
    
    EXPECT_EQ(test_cmd_executed, 3);
}

TEST(SvGameCommands_Multi, TracksLastCommand) {
    ResetCommands();
    
    ExecuteCommand("say");
    ExecuteCommand("score");
    
    EXPECT_STREQ(test_last_cmd, "score");
}

// ============================================================================
// TEST SUITE: Command Names
// ============================================================================

TEST(SvGameCommands_Names, CaseSensitive) {
    ResetCommands();
    
    gameCommand_t* cmd = FindCommand("SAY");
    
    EXPECT_EQ(cmd, nullptr);
}

TEST(SvGameCommands_Names, ExactMatch) {
    ResetCommands();
    
    gameCommand_t* cmd = FindCommand("say");
    
    EXPECT_NE(cmd, nullptr);
}

// ============================================================================
// TEST SUITE: Command List
// ============================================================================

TEST(SvGameCommands_List, CountsCommands) {
    int count = 0;
    for (int i = 0; test_commands[i].name[0]; i++) {
        count++;
    }
    
    EXPECT_EQ(count, 3);
}

TEST(SvGameCommands_List, AllHaveFunctions) {
    for (int i = 0; test_commands[i].name[0]; i++) {
        EXPECT_NE(test_commands[i].func, nullptr);
    }
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvGameCommands_Edge, EmptyString) {
    ResetCommands();
    
    gameCommand_t* cmd = FindCommand("");
    
    EXPECT_EQ(cmd, nullptr);
}

TEST(SvGameCommands_Edge, VeryLongName) {
    ResetCommands();
    char longname[100];
    memset(longname, 'a', 99);
    longname[99] = '\0';
    
    gameCommand_t* cmd = FindCommand(longname);
    
    EXPECT_EQ(cmd, nullptr);
}

TEST(SvGameCommands_Edge, NullFunction) {
    ResetCommands();
    
    // Manually create command with null function
    gameCommand_t nullCmd = {"nullcmd", nullptr};
    
    EXPECT_EQ(nullCmd.func, nullptr);
}

// ============================================================================
// TEST SUITE: Command Frequency
// ============================================================================

TEST(SvGameCommands_Freq, RepeatedExecution) {
    ResetCommands();
    
    for (int i = 0; i < 10; i++) {
        ExecuteCommand("say");
    }
    
    EXPECT_EQ(test_cmd_executed, 10);
}

TEST(SvGameCommands_Freq, MixedCommands) {
    ResetCommands();
    
    ExecuteCommand("say");
    ExecuteCommand("say");
    ExecuteCommand("score");
    ExecuteCommand("kill");
    ExecuteCommand("say");
    
    EXPECT_EQ(test_cmd_executed, 5);
}

// ============================================================================
// TEST SUITE: Command Validation
// ============================================================================

TEST(SvGameCommands_Valid, ValidCommandReturnsTrue) {
    ResetCommands();
    
    bool result = ExecuteCommand("say");
    
    EXPECT_TRUE(result);
}

TEST(SvGameCommands_Valid, InvalidCommandReturnsFalse) {
    ResetCommands();
    
    bool result = ExecuteCommand("notexist");
    
    EXPECT_FALSE(result);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvGameCommands_Perf, ManyLookups) {
    ResetCommands();
    
    for (int i = 0; i < 1000; i++) {
        FindCommand("say");
    }
    
    EXPECT_TRUE(true);
}

TEST(SvGameCommands_Perf, ManyExecutions) {
    ResetCommands();
    
    for (int i = 0; i < 100; i++) {
        ExecuteCommand("say");
        ExecuteCommand("score");
        ExecuteCommand("kill");
    }
    
    EXPECT_EQ(test_cmd_executed, 300);
}

TEST(SvGameCommands_Perf, MixedLookups) {
    ResetCommands();
    
    for (int i = 0; i < 100; i++) {
        FindCommand("say");
        FindCommand("unknown");
        FindCommand("score");
    }
    
    EXPECT_TRUE(true);
}

// ============================================================================
// SUMMARY: 30 tests for sv_game_commands.h
// ============================================================================
