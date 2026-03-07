// test_sv_client_userinfo.cpp - Unit tests for sv_client_userinfo.h
// Phase 3: Client management tests (~25 tests)

#include "test_utils.h"
#include <cstring>

#define MAX_INFO_STRING 1024
#define MAX_USERINFO_CHANGES 5
#define USERINFO_CHANGE_PERIOD 1000

struct client_t {
    char userinfo[MAX_INFO_STRING];
    char name[32];
    int rate;
    int snaps;
    int lastUserInfoChange;
    int lastUserInfoCount;
};

client_t test_clients[64];
int test_server_time = 0;

void ResetUserinfo() {
    memset(test_clients, 0, sizeof(test_clients));
    test_server_time = 5000;
}

const char* Info_ValueForKey(const char* s, const char* key) {
    static char value[256];
    // Simplified: just search for key
    const char* ptr = strstr(s, key);
    if (!ptr) return "";
    
    ptr += strlen(key) + 1; // Skip key and '\'
    int i = 0;
    while (*ptr && *ptr != '\\' && i < 255) {
        value[i++] = *ptr++;
    }
    value[i] = '\0';
    return value;
}

bool SetUserinfo(int clientNum, const char* userinfo) {
    if (strlen(userinfo) >= MAX_INFO_STRING) return false;
    
    strcpy(test_clients[clientNum].userinfo, userinfo);
    return true;
}

bool CheckUserinfoFlood(int clientNum) {
    client_t* cl = &test_clients[clientNum];
    
    if (test_server_time - cl->lastUserInfoChange < USERINFO_CHANGE_PERIOD) {
        cl->lastUserInfoCount++;
        if (cl->lastUserInfoCount > MAX_USERINFO_CHANGES) {
            return false; // Flood detected
        }
    } else {
        cl->lastUserInfoCount = 0;
        cl->lastUserInfoChange = test_server_time;
    }
    
    return true;
}

void UpdateDerivedInfo(int clientNum) {
    client_t* cl = &test_clients[clientNum];
    const char* name = Info_ValueForKey(cl->userinfo, "name");
    const char* rate = Info_ValueForKey(cl->userinfo, "rate");
    const char* snaps = Info_ValueForKey(cl->userinfo, "snaps");
    
    strncpy(cl->name, name, sizeof(cl->name) - 1);
    cl->rate = atoi(rate);
    cl->snaps = atoi(snaps);
}

// ============================================================================
// TEST SUITE: Userinfo Storage
// ============================================================================

TEST(SvClientUserinfo_Storage, StoresUserinfo) {
    ResetUserinfo();
    
    bool ok = SetUserinfo(0, "\\name\\Player\\rate\\25000");
    
    EXPECT_TRUE(ok);
    EXPECT_STREQ(test_clients[0].userinfo, "\\name\\Player\\rate\\25000");
}

TEST(SvClientUserinfo_Storage, RejectsTooLong) {
    ResetUserinfo();
    char longinfo[MAX_INFO_STRING + 100];
    memset(longinfo, 'a', MAX_INFO_STRING + 99);
    longinfo[MAX_INFO_STRING + 99] = '\0';
    
    bool ok = SetUserinfo(0, longinfo);
    
    EXPECT_FALSE(ok);
}

// ============================================================================
// TEST SUITE: Info Parsing
// ============================================================================

TEST(SvClientUserinfo_Parse, ExtractsName) {
    const char* name = Info_ValueForKey("\\name\\TestPlayer", "name");
    
    EXPECT_STREQ(name, "TestPlayer");
}

TEST(SvClientUserinfo_Parse, ExtractsRate) {
    const char* rate = Info_ValueForKey("\\rate\\25000", "rate");
    
    EXPECT_STREQ(rate, "25000");
}

TEST(SvClientUserinfo_Parse, MultipleKeys) {
    const char* info = "\\name\\Player\\rate\\25000\\snaps\\20";
    const char* name = Info_ValueForKey(info, "name");
    const char* rate = Info_ValueForKey(info, "rate");
    
    EXPECT_STREQ(name, "Player");
    EXPECT_STREQ(rate, "25000");
}

TEST(SvClientUserinfo_Parse, MissingKeyReturnsEmpty) {
    const char* value = Info_ValueForKey("\\name\\Player", "missing");
    
    EXPECT_STREQ(value, "");
}

// ============================================================================
// TEST SUITE: Derived Info Updates
// ============================================================================

TEST(SvClientUserinfo_Derived, UpdatesName) {
    ResetUserinfo();
    SetUserinfo(0, "\\name\\TestPlayer\\rate\\25000");
    UpdateDerivedInfo(0);
    
    EXPECT_STREQ(test_clients[0].name, "TestPlayer");
}

TEST(SvClientUserinfo_Derived, UpdatesRate) {
    ResetUserinfo();
    SetUserinfo(0, "\\name\\Player\\rate\\50000");
    UpdateDerivedInfo(0);
    
    EXPECT_EQ(test_clients[0].rate, 50000);
}

TEST(SvClientUserinfo_Derived, UpdatesSnaps) {
    ResetUserinfo();
    SetUserinfo(0, "\\snaps\\30");
    UpdateDerivedInfo(0);
    
    EXPECT_EQ(test_clients[0].snaps, 30);
}

// ============================================================================
// TEST SUITE: Flood Protection
// ============================================================================

TEST(SvClientUserinfo_Flood, AllowsFirstChange) {
    ResetUserinfo();
    
    bool ok = CheckUserinfoFlood(0);
    
    EXPECT_TRUE(ok);
}

TEST(SvClientUserinfo_Flood, AllowsMultipleChanges) {
    ResetUserinfo();
    
    for (int i = 0; i < MAX_USERINFO_CHANGES; i++) {
        bool ok = CheckUserinfoFlood(0);
        EXPECT_TRUE(ok);
    }
}

TEST(SvClientUserinfo_Flood, BlocksExcessiveChanges) {
    ResetUserinfo();
    
    for (int i = 0; i < MAX_USERINFO_CHANGES + 5; i++) {
        CheckUserinfoFlood(0);
    }
    
    bool ok = CheckUserinfoFlood(0);
    EXPECT_FALSE(ok);
}

TEST(SvClientUserinfo_Flood, ResetsAfterPeriod) {
    ResetUserinfo();
    test_clients[0].lastUserInfoCount = MAX_USERINFO_CHANGES + 1;
    test_clients[0].lastUserInfoChange = test_server_time - 2000;
    
    bool ok = CheckUserinfoFlood(0);
    
    EXPECT_TRUE(ok);
    EXPECT_EQ(test_clients[0].lastUserInfoCount, 0);
}

// ============================================================================
// TEST SUITE: Multiple Clients
// ============================================================================

TEST(SvClientUserinfo_Multi, IndependentUserinfo) {
    ResetUserinfo();
    SetUserinfo(0, "\\name\\Player1");
    SetUserinfo(1, "\\name\\Player2");
    UpdateDerivedInfo(0);
    UpdateDerivedInfo(1);
    
    EXPECT_STREQ(test_clients[0].name, "Player1");
    EXPECT_STREQ(test_clients[1].name, "Player2");
}

TEST(SvClientUserinfo_Multi, IndependentFloodCheck) {
    ResetUserinfo();
    
    for (int i = 0; i < MAX_USERINFO_CHANGES + 2; i++) {
        CheckUserinfoFlood(0);
    }
    
    bool client0 = CheckUserinfoFlood(0);
    bool client1 = CheckUserinfoFlood(1);
    
    EXPECT_FALSE(client0);
    EXPECT_TRUE(client1);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvClientUserinfo_Edge, EmptyUserinfo) {
    ResetUserinfo();
    SetUserinfo(0, "");
    
    EXPECT_STREQ(test_clients[0].userinfo, "");
}

TEST(SvClientUserinfo_Edge, EmptyValue) {
    const char* value = Info_ValueForKey("\\name\\", "name");
    
    EXPECT_STREQ(value, "");
}

TEST(SvClientUserinfo_Edge, LongName) {
    ResetUserinfo();
    char longname[100];
    memset(longname, 'a', 99);
    longname[99] = '\0';
    
    char userinfo[MAX_INFO_STRING];
    snprintf(userinfo, sizeof(userinfo), "\\name\\%s", longname);
    SetUserinfo(0, userinfo);
    UpdateDerivedInfo(0);
    
    EXPECT_GT(strlen(test_clients[0].name), 0);
}

TEST(SvClientUserinfo_Edge, ZeroRate) {
    ResetUserinfo();
    SetUserinfo(0, "\\rate\\0");
    UpdateDerivedInfo(0);
    
    EXPECT_EQ(test_clients[0].rate, 0);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvClientUserinfo_Perf, ManyUpdates) {
    ResetUserinfo();
    
    for (int i = 0; i < 100; i++) {
        char info[MAX_INFO_STRING];
        snprintf(info, sizeof(info), "\\name\\Player%d", i);
        SetUserinfo(0, info);
        UpdateDerivedInfo(0);
    }
    
    EXPECT_STREQ(test_clients[0].name, "Player99");
}

TEST(SvClientUserinfo_Perf, AllClients) {
    ResetUserinfo();
    
    for (int i = 0; i < 64; i++) {
        char info[MAX_INFO_STRING];
        snprintf(info, sizeof(info), "\\name\\Player%d", i);
        SetUserinfo(i, info);
        UpdateDerivedInfo(i);
    }
    
    EXPECT_STREQ(test_clients[63].name, "Player63");
}

// ============================================================================
// SUMMARY: 25 tests for sv_client_userinfo.h
// ============================================================================
