/* Copyright (c) 2014, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <openssl/base.h>

#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(memory_sanitizer)
#define OPENSSL_ASAN
#endif
#endif

// This file isn't built on ARM or Aarch64 because we link statically in those
// builds and trying to override malloc in a static link doesn't work. It's also
// disabled on ASan builds as this interferes with ASan's malloc interceptor.
//
// TODO(davidben): See if this and ASan's and MSan's interceptors can be made to
// coexist.
#if defined(__linux__) && !defined(OPENSSL_ARM) && \
    !defined(OPENSSL_AARCH64) && !defined(OPENSSL_ASAN)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <new>


/* This file defines overrides for the standard allocation functions that allow
 * a given allocation to be made to fail for testing. If the program is run
 * with MALLOC_NUMBER_TO_FAIL set to a base-10 number then that allocation will
 * return NULL. If MALLOC_ABORT_ON_FAIL is also defined then the allocation
 * will abort() rather than return NULL.
 *
 * This code is not thread safe. */

static uint64_t current_malloc_count = 0;
static uint64_t malloc_number_to_fail = 0;
static char failure_enabled = 0, abort_on_fail = 0;
static int in_call = 0;

extern "C" {
/* These are other names for the standard allocation functions. */
extern void *__libc_malloc(size_t size);
extern void *__libc_calloc(size_t num_elems, size_t size);
extern void *__libc_realloc(void *ptr, size_t size);
}

static void exit_handler(void) {
  if (failure_enabled && current_malloc_count > malloc_number_to_fail) {
    _exit(88);
  }
}

static void cpp_new_handler() {
  // Return to try again. It won't fail a second time.
  return;
}

/* should_fail_allocation returns true if the current allocation should fail. */
static int should_fail_allocation() {
  static int init = 0;
  char should_fail;

  if (in_call) {
    return 0;
  }

  in_call = 1;

  if (!init) {
    const char *env = getenv("MALLOC_NUMBER_TO_FAIL");
    if (env != NULL && env[0] != 0) {
      char *endptr;
      malloc_number_to_fail = strtoull(env, &endptr, 10);
      if (*endptr == 0) {
        failure_enabled = 1;
        atexit(exit_handler);
        std::set_new_handler(cpp_new_handler);
      }
    }
    abort_on_fail = (NULL != getenv("MALLOC_ABORT_ON_FAIL"));
    init = 1;
  }

  in_call = 0;

  if (!failure_enabled) {
    return 0;
  }

  should_fail = (current_malloc_count == malloc_number_to_fail);
  current_malloc_count++;

  if (should_fail && abort_on_fail) {
    abort();
  }
  return should_fail;
}

extern "C" {

void *malloc(size_t size) {
  if (should_fail_allocation()) {
    return NULL;
  }

  return __libc_malloc(size);
}

void *calloc(size_t num_elems, size_t size) {
  if (should_fail_allocation()) {
    return NULL;
  }

  return __libc_calloc(num_elems, size);
}

void *realloc(void *ptr, size_t size) {
  if (should_fail_allocation()) {
    return NULL;
  }

  return __libc_realloc(ptr, size);
}

}  // extern "C"

#endif  /* defined(linux) && !ARM && !AARCH64 && !ASAN */
