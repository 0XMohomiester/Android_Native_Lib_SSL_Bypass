#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <android/log.h>

#define TAG "SSLBYPASS"

// Log macros
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Change memory protection
void make_rw(void *addr) {
    uintptr_t page = (uintptr_t)addr & ~(getpagesize() - 1);
    if (mprotect((void *)page, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGE("mprotect failed");
    } else {
        LOGI("mprotect success at %p", addr);
    }
}

// ARM64 patch: return 0
unsigned char patch_ret0[] = {
    0x00, 0x00, 0x80, 0x52,
    0xC0, 0x03, 0x5F, 0xD6
};

// ARM64 patch: return 1
unsigned char patch_ret1[] = {
    0x20, 0x00, 0x80, 0x52,
    0xC0, 0x03, 0x5F, 0xD6
};

void patch_function(void *func, unsigned char *patch, size_t size, const char *name) {
    if (!func) {
        LOGE("Function %s not found", name);
        return;
    }

    LOGI("Patching %s at %p", name, func);

    make_rw(func);
    memcpy(func, patch, size);

    LOGI("Patch applied to %s", name);
}

// Main SSL bypass logic
void bypass_ssl() {
    void *handle = NULL;

    LOGI("Waiting for libssl...");

    for (int i = 0; i < 10; i++) {
        handle = dlopen("/apex/com.android.conscrypt/lib64/libssl.so", RTLD_NOW);
        if (handle) {
            LOGI("libssl loaded from APEX!");
            break;
        }
        sleep(1);
    }

    if (!handle) {
        LOGE("Failed to load APEX libssl, trying fallback...");
        handle = dlopen("libssl.so", RTLD_NOW);
    }

    if (!handle) {
        LOGE("Failed to load any libssl!");
        return;
    }

    void *verify = dlsym(handle, "SSL_get_verify_result");
    void *chain  = dlsym(handle, "SSL_verify_cert_chain");

    LOGI("SSL_get_verify_result = %p", verify);
    LOGI("SSL_verify_cert_chain = %p", chain);

    if (verify) {
        patch_function(verify, patch_ret0, sizeof(patch_ret0), "SSL_get_verify_result");
    }

    if (chain) {
        patch_function(chain, patch_ret1, sizeof(patch_ret1), "SSL_verify_cert_chain");
    }

    LOGI("SSL bypass finished");
}

// Entry point
__attribute__((constructor))
void init() {
    LOGI("==== SSL BYPASS LIB LOADED ====");

    sleep(2); // avoid early crash

    bypass_ssl();
}
