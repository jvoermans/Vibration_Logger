#include <unity.h>

void test_failing(void) {
    TEST_ASSERT_EQUAL(32, 32);
}


int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_failing);
    UNITY_END();

    return 0;
}