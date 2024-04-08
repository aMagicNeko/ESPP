#include <cryptopp/keccak.h>
#include <cryptopp/hex.h>
#include <bthread/bthread.h>
#include <iostream>
#include <gflags/gflags_declare.h>
void* test(void* args) {
    while(true) {
        std::cout << 1;
    }
}
int main() {
    //bthread_t bid;
    //bthread_start_background(&bid, NULL, test, NULL);
    while (true)
    sleep(1);
    return 0;

}