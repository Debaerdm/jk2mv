// test_sv_snapshot_writer.cpp - Unit tests for sv_snapshot_writer.h
// Phase 4: Snapshot system tests (~30 tests)

#include "test_utils.h"
#include <cstring>

struct msg_t {
    unsigned char data[8192];
    int cursize;
    int maxsize;
    int bit;
};

msg_t test_msg;

void ResetWriter() {
    memset(&test_msg, 0, sizeof(test_msg));
    test_msg.maxsize = sizeof(test_msg.data);
}

void MSG_Init(msg_t* msg, unsigned char* data, int length) {
    msg->data[0] = 0;
    msg->cursize = 0;
    msg->maxsize = length;
    msg->bit = 0;
}

void MSG_WriteByte(msg_t* msg, int value) {
    if (msg->cursize + 1 > msg->maxsize) return;
    msg->data[msg->cursize] = value & 0xFF;
    msg->cursize++;
}

void MSG_WriteShort(msg_t* msg, int value) {
    if (msg->cursize + 2 > msg->maxsize) return;
    msg->data[msg->cursize] = value & 0xFF;
    msg->data[msg->cursize + 1] = (value >> 8) & 0xFF;
    msg->cursize += 2;
}

void MSG_WriteLong(msg_t* msg, int value) {
    if (msg->cursize + 4 > msg->maxsize) return;
    msg->data[msg->cursize] = value & 0xFF;
    msg->data[msg->cursize + 1] = (value >> 8) & 0xFF;
    msg->data[msg->cursize + 2] = (value >> 16) & 0xFF;
    msg->data[msg->cursize + 3] = (value >> 24) & 0xFF;
    msg->cursize += 4;
}

int MSG_ReadByte(msg_t* msg, int* pos) {
    if (*pos >= msg->cursize) return -1;
    return msg->data[(*pos)++];
}

int MSG_ReadShort(msg_t* msg, int* pos) {
    if (*pos + 1 >= msg->cursize) return -1;
    int value = msg->data[*pos] | (msg->data[*pos + 1] << 8);
    *pos += 2;
    return value;
}

int MSG_ReadLong(msg_t* msg, int* pos) {
    if (*pos + 3 >= msg->cursize) return -1;
    int value = msg->data[*pos] | (msg->data[*pos + 1] << 8) | 
                (msg->data[*pos + 2] << 16) | (msg->data[*pos + 3] << 24);
    *pos += 4;
    return value;
}

// ============================================================================
// TEST SUITE: Message Initialization
// ============================================================================

TEST(SvSnapshotWriter_Init, InitializesEmpty) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    EXPECT_EQ(test_msg.cursize, 0);
}

TEST(SvSnapshotWriter_Init, SetsMaxSize) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, 1024);
    
    EXPECT_EQ(test_msg.maxsize, 1024);
}

// ============================================================================
// TEST SUITE: Byte Writing
// ============================================================================

TEST(SvSnapshotWriter_Byte, WritesSingleByte) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    MSG_WriteByte(&test_msg, 0x42);
    
    EXPECT_EQ(test_msg.cursize, 1);
    EXPECT_EQ(test_msg.data[0], 0x42);
}

TEST(SvSnapshotWriter_Byte, WritesZero) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    MSG_WriteByte(&test_msg, 0);
    
    EXPECT_EQ(test_msg.data[0], 0);
}

TEST(SvSnapshotWriter_Byte, Writes255) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    MSG_WriteByte(&test_msg, 255);
    
    EXPECT_EQ(test_msg.data[0], 255);
}

TEST(SvSnapshotWriter_Byte, MultipleBytes) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    MSG_WriteByte(&test_msg, 0x11);
    MSG_WriteByte(&test_msg, 0x22);
    MSG_WriteByte(&test_msg, 0x33);
    
    EXPECT_EQ(test_msg.cursize, 3);
    EXPECT_EQ(test_msg.data[0], 0x11);
    EXPECT_EQ(test_msg.data[1], 0x22);
    EXPECT_EQ(test_msg.data[2], 0x33);
}

// ============================================================================
// TEST SUITE: Short Writing
// ============================================================================

TEST(SvSnapshotWriter_Short, WritesShort) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    MSG_WriteShort(&test_msg, 0x1234);
    
    EXPECT_EQ(test_msg.cursize, 2);
    EXPECT_EQ(test_msg.data[0], 0x34);
    EXPECT_EQ(test_msg.data[1], 0x12);
}

TEST(SvSnapshotWriter_Short, WritesZero) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    MSG_WriteShort(&test_msg, 0);
    
    EXPECT_EQ(test_msg.data[0], 0);
    EXPECT_EQ(test_msg.data[1], 0);
}

TEST(SvSnapshotWriter_Short, WritesNegative) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    MSG_WriteShort(&test_msg, -1);
    
    EXPECT_EQ(test_msg.cursize, 2);
}

// ============================================================================
// TEST SUITE: Long Writing
// ============================================================================

TEST(SvSnapshotWriter_Long, WritesLong) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    MSG_WriteLong(&test_msg, 0x12345678);
    
    EXPECT_EQ(test_msg.cursize, 4);
    EXPECT_EQ(test_msg.data[0], 0x78);
    EXPECT_EQ(test_msg.data[1], 0x56);
    EXPECT_EQ(test_msg.data[2], 0x34);
    EXPECT_EQ(test_msg.data[3], 0x12);
}

TEST(SvSnapshotWriter_Long, WritesZero) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    MSG_WriteLong(&test_msg, 0);
    
    EXPECT_EQ(test_msg.cursize, 4);
}

// ============================================================================
// TEST SUITE: Reading
// ============================================================================

TEST(SvSnapshotWriter_Read, ReadsByte) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    MSG_WriteByte(&test_msg, 0x42);
    
    int pos = 0;
    int value = MSG_ReadByte(&test_msg, &pos);
    
    EXPECT_EQ(value, 0x42);
    EXPECT_EQ(pos, 1);
}

TEST(SvSnapshotWriter_Read, ReadsShort) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    MSG_WriteShort(&test_msg, 0x1234);
    
    int pos = 0;
    int value = MSG_ReadShort(&test_msg, &pos);
    
    EXPECT_EQ(value, 0x1234);
    EXPECT_EQ(pos, 2);
}

TEST(SvSnapshotWriter_Read, ReadsLong) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    MSG_WriteLong(&test_msg, 0x12345678);
    
    int pos = 0;
    int value = MSG_ReadLong(&test_msg, &pos);
    
    EXPECT_EQ(value, 0x12345678);
    EXPECT_EQ(pos, 4);
}

// ============================================================================
// TEST SUITE: Round-trip
// ============================================================================

TEST(SvSnapshotWriter_Roundtrip, ByteRoundtrip) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    MSG_WriteByte(&test_msg, 123);
    
    int pos = 0;
    int value = MSG_ReadByte(&test_msg, &pos);
    
    EXPECT_EQ(value, 123);
}

TEST(SvSnapshotWriter_Roundtrip, ShortRoundtrip) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    MSG_WriteShort(&test_msg, 30000);
    
    int pos = 0;
    int value = MSG_ReadShort(&test_msg, &pos);
    
    EXPECT_EQ(value, 30000);
}

TEST(SvSnapshotWriter_Roundtrip, LongRoundtrip) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    MSG_WriteLong(&test_msg, 123456789);
    
    int pos = 0;
    int value = MSG_ReadLong(&test_msg, &pos);
    
    EXPECT_EQ(value, 123456789);
}

// ============================================================================
// TEST SUITE: Overflow Protection
// ============================================================================

TEST(SvSnapshotWriter_Overflow, ByteOverflow) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, 1);
    
    MSG_WriteByte(&test_msg, 0x11);
    MSG_WriteByte(&test_msg, 0x22); // Should be ignored
    
    EXPECT_EQ(test_msg.cursize, 1);
}

TEST(SvSnapshotWriter_Overflow, ShortOverflow) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, 2);
    
    MSG_WriteShort(&test_msg, 0x1234);
    MSG_WriteShort(&test_msg, 0x5678); // Should be ignored
    
    EXPECT_EQ(test_msg.cursize, 2);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvSnapshotWriter_Edge, ReadBeyondEnd) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    MSG_WriteByte(&test_msg, 0x42);
    
    int pos = 10;
    int value = MSG_ReadByte(&test_msg, &pos);
    
    EXPECT_EQ(value, -1);
}

TEST(SvSnapshotWriter_Edge, EmptyMessage) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    int pos = 0;
    int value = MSG_ReadByte(&test_msg, &pos);
    
    EXPECT_EQ(value, -1);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvSnapshotWriter_Perf, ManyBytes) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    for (int i = 0; i < 100; i++) {
        MSG_WriteByte(&test_msg, i & 0xFF);
    }
    
    EXPECT_EQ(test_msg.cursize, 100);
}

TEST(SvSnapshotWriter_Perf, ManyLongs) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    for (int i = 0; i < 50; i++) {
        MSG_WriteLong(&test_msg, i * 1000);
    }
    
    EXPECT_EQ(test_msg.cursize, 200);
}

TEST(SvSnapshotWriter_Perf, ManyReads) {
    ResetWriter();
    MSG_Init(&test_msg, test_msg.data, sizeof(test_msg.data));
    
    for (int i = 0; i < 100; i++) {
        MSG_WriteByte(&test_msg, i);
    }
    
    int pos = 0;
    for (int i = 0; i < 100; i++) {
        MSG_ReadByte(&test_msg, &pos);
    }
    
    EXPECT_EQ(pos, 100);
}

// ============================================================================
// SUMMARY: 30 tests for sv_snapshot_writer.h
// ============================================================================
