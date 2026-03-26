#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>

// Change memory protection
void make_rw(void *addr) {
    uintptr_t page = (uintptr_t)addr & ~(getpagesize() - 1);
    mprotect((void *)page, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC);
}

// ARM64 patch: return 0 (success)
// mov w0, #0
// ret
unsigned char patch_ret0[] = {
    0x00, 0x00, 0x80, 0x52,
    0xC0, 0x03, 0x5F, 0xD6
};

// ARM64 patch: return 1
// mov w0, #1
// ret
unsigned char patch_ret1[] = {
    0x20, 0x00, 0x80, 0x52,
    0xC0, 0x03, 0x5F, 0xD6
};

void patch_function(void *func, unsigned char *patch, size_t size) {
    make_rw(func);
    memcpy(func, patch, size);
}

// Try to hook libssl functions
void bypass_ssl() {
    void *handle = NULL;

    // wait until libssl is loaded
    for (int i = 0; i < 10; i++) {
        handle = dlopen("/apex/com.android.conscrypt/lib64/libssl.so", RTLD_NOW);
        if (handle) break;
        sleep(1);
    }

    if (!handle) return;

    void *verify = dlsym(handle, "SSL_get_verify_result");
    void *chain  = dlsym(handle, "SSL_verify_cert_chain");

    if (verify) {
        patch_function(verify, patch_ret0, sizeof(patch_ret0));
    }

    if (chain) {
        patch_function(chain, patch_ret1, sizeof(patch_ret1));
    }
}

// Entry point when injected
__attribute__((constructor))
void init() {
    // small delay to avoid early crash
    sleep(2);

    bypass_ssl();
}
