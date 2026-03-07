// test_main.cpp - Main entry point for JK2MV Google Test suite

#include <gtest/gtest.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Custom test listener for colored output
    ::testing::TestEventListeners& listeners =
        ::testing::UnitTest::GetInstance()->listeners();
    
    // Run all tests
    int result = RUN_ALL_TESTS();
    
    return result;
}
