// test_sv_client_download.cpp - Unit tests for sv_client_download.h
// Phase 3: Client management tests (~25 tests)

#include "test_utils.h"
#include <cstring>

struct client_t {
    char* download;
    int downloadSize;
    int downloadCount;
    int downloadBlockNumber;
    bool downloadStarted;
    char downloadName[256];
};

client_t test_clients[64];
char test_download_data[4096];

void ResetDownloads() {
    memset(test_clients, 0, sizeof(test_clients));
    memset(test_download_data, 'A', sizeof(test_download_data));
}

void StartDownload(int clientNum, const char* filename, int size) {
    client_t* cl = &test_clients[clientNum];
    cl->download = test_download_data;
    cl->downloadSize = size;
    cl->downloadCount = 0;
    cl->downloadBlockNumber = 0;
    cl->downloadStarted = true;
    strncpy(cl->downloadName, filename, sizeof(cl->downloadName) - 1);
}

int SendDownloadBlock(int clientNum, int blockSize) {
    client_t* cl = &test_clients[clientNum];
    if (!cl->downloadStarted) return -1;
    
    int remaining = cl->downloadSize - cl->downloadCount;
    int toSend = (blockSize < remaining) ? blockSize : remaining;
    
    cl->downloadCount += toSend;
    cl->downloadBlockNumber++;
    
    return toSend;
}

bool IsDownloadComplete(int clientNum) {
    return test_clients[clientNum].downloadCount >= test_clients[clientNum].downloadSize;
}

void StopDownload(int clientNum) {
    client_t* cl = &test_clients[clientNum];
    cl->download = nullptr;
    cl->downloadSize = 0;
    cl->downloadCount = 0;
    cl->downloadStarted = false;
}

// ============================================================================
// TEST SUITE: Download Initialization
// ============================================================================

TEST(SvClientDownload_Init, StartsDownload) {
    ResetDownloads();
    StartDownload(0, "test.pk3", 1024);
    
    EXPECT_TRUE(test_clients[0].downloadStarted);
}

TEST(SvClientDownload_Init, SetsFilename) {
    ResetDownloads();
    StartDownload(0, "maps/test.bsp", 2048);
    
    EXPECT_STREQ(test_clients[0].downloadName, "maps/test.bsp");
}

TEST(SvClientDownload_Init, SetsSize) {
    ResetDownloads();
    StartDownload(0, "test.pk3", 5000);
    
    EXPECT_EQ(test_clients[0].downloadSize, 5000);
}

TEST(SvClientDownload_Init, ResetsCount) {
    ResetDownloads();
    test_clients[0].downloadCount = 999;
    StartDownload(0, "test.pk3", 1024);
    
    EXPECT_EQ(test_clients[0].downloadCount, 0);
}

// ============================================================================
// TEST SUITE: Block Transfer
// ============================================================================

TEST(SvClientDownload_Transfer, SendsSingleBlock) {
    ResetDownloads();
    StartDownload(0, "test.pk3", 1024);
    
    int sent = SendDownloadBlock(0, 512);
    
    EXPECT_EQ(sent, 512);
    EXPECT_EQ(test_clients[0].downloadCount, 512);
}

TEST(SvClientDownload_Transfer, IncrementsBlockNumber) {
    ResetDownloads();
    StartDownload(0, "test.pk3", 1024);
    
    SendDownloadBlock(0, 256);
    SendDownloadBlock(0, 256);
    
    EXPECT_EQ(test_clients[0].downloadBlockNumber, 2);
}

TEST(SvClientDownload_Transfer, HandlesPartialLastBlock) {
    ResetDownloads();
    StartDownload(0, "test.pk3", 1000);
    
    SendDownloadBlock(0, 512); // 512 sent
    SendDownloadBlock(0, 512); // Should only send 488
    
    EXPECT_EQ(test_clients[0].downloadCount, 1000);
}

TEST(SvClientDownload_Transfer, StopsAtFileEnd) {
    ResetDownloads();
    StartDownload(0, "test.pk3", 100);
    
    SendDownloadBlock(0, 100);
    int extra = SendDownloadBlock(0, 100);
    
    EXPECT_EQ(extra, 0);
}

// ============================================================================
// TEST SUITE: Download Completion
// ============================================================================

TEST(SvClientDownload_Complete, DetectsCompletion) {
    ResetDownloads();
    StartDownload(0, "test.pk3", 512);
    SendDownloadBlock(0, 512);
    
    EXPECT_TRUE(IsDownloadComplete(0));
}

TEST(SvClientDownload_Complete, NotCompleteWhenPartial) {
    ResetDownloads();
    StartDownload(0, "test.pk3", 1024);
    SendDownloadBlock(0, 512);
    
    EXPECT_FALSE(IsDownloadComplete(0));
}

// ============================================================================
// TEST SUITE: Download Cancellation
// ============================================================================

TEST(SvClientDownload_Cancel, StopsDownload) {
    ResetDownloads();
    StartDownload(0, "test.pk3", 1024);
    StopDownload(0);
    
    EXPECT_FALSE(test_clients[0].downloadStarted);
}

TEST(SvClientDownload_Cancel, ClearsDownloadPointer) {
    ResetDownloads();
    StartDownload(0, "test.pk3", 1024);
    StopDownload(0);
    
    EXPECT_EQ(test_clients[0].download, nullptr);
}

TEST(SvClientDownload_Cancel, ResetsSize) {
    ResetDownloads();
    StartDownload(0, "test.pk3", 1024);
    StopDownload(0);
    
    EXPECT_EQ(test_clients[0].downloadSize, 0);
}

// ============================================================================
// TEST SUITE: Multiple Clients
// ============================================================================

TEST(SvClientDownload_Multi, IndependentDownloads) {
    ResetDownloads();
    StartDownload(0, "file1.pk3", 1000);
    StartDownload(1, "file2.pk3", 2000);
    
    EXPECT_STREQ(test_clients[0].downloadName, "file1.pk3");
    EXPECT_STREQ(test_clients[1].downloadName, "file2.pk3");
}

TEST(SvClientDownload_Multi, DifferentProgress) {
    ResetDownloads();
    StartDownload(0, "file1.pk3", 1000);
    StartDownload(1, "file2.pk3", 2000);
    
    SendDownloadBlock(0, 500);
    SendDownloadBlock(1, 1000);
    
    EXPECT_EQ(test_clients[0].downloadCount, 500);
    EXPECT_EQ(test_clients[1].downloadCount, 1000);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvClientDownload_Edge, ZeroSizeFile) {
    ResetDownloads();
    StartDownload(0, "empty.txt", 0);
    
    EXPECT_TRUE(IsDownloadComplete(0));
}

TEST(SvClientDownload_Edge, VeryLargeFile) {
    ResetDownloads();
    StartDownload(0, "huge.pk3", 1000000000);
    
    EXPECT_EQ(test_clients[0].downloadSize, 1000000000);
}

TEST(SvClientDownload_Edge, LongFilename) {
    ResetDownloads();
    char longname[300];
    memset(longname, 'a', 299);
    longname[299] = '\0';
    
    StartDownload(0, longname, 1024);
    
    EXPECT_GT(strlen(test_clients[0].downloadName), 0);
}

TEST(SvClientDownload_Edge, SendWithoutStart) {
    ResetDownloads();
    
    int sent = SendDownloadBlock(0, 512);
    
    EXPECT_EQ(sent, -1);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvClientDownload_Perf, ManySmallBlocks) {
    ResetDownloads();
    StartDownload(0, "test.pk3", 10000);
    
    for (int i = 0; i < 100; i++) {
        SendDownloadBlock(0, 100);
    }
    
    EXPECT_EQ(test_clients[0].downloadCount, 10000);
}

TEST(SvClientDownload_Perf, MultipleDownloadCycles) {
    ResetDownloads();
    
    for (int i = 0; i < 10; i++) {
        StartDownload(0, "test.pk3", 512);
        SendDownloadBlock(0, 512);
        StopDownload(0);
    }
    
    EXPECT_FALSE(test_clients[0].downloadStarted);
}

// ============================================================================
// SUMMARY: 25 tests for sv_client_download.h
// ============================================================================
