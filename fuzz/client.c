/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL licenses, (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.openssl.org/source/license.html
 * or in the file LICENSE in the source distribution.
 */

#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include "fuzzer.h"

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
extern int rand_predictable;
#endif
#define ENTROPY_NEEDED 32

/* unused, to avoid warning. */
static int idx;

int FuzzerInitialize(int *argc, char ***argv)
{
    STACK_OF(SSL_COMP) *comp_methods;

    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS | OPENSSL_INIT_ASYNC, NULL);
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS, NULL);
    ERR_get_state();
    CRYPTO_free_ex_index(0, -1);
    idx = SSL_get_ex_data_X509_STORE_CTX_idx();
    RAND_add("", 1, ENTROPY_NEEDED);
    RAND_status();
    RSA_get_default_method();
    comp_methods = SSL_COMP_get_compression_methods();
    OPENSSL_sk_sort((OPENSSL_STACK *)comp_methods);


#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    rand_predictable = 1;
#endif

    return 1;
}

int FuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    SSL *client;
    BIO *in;
    BIO *out;
    SSL_CTX *ctx;

    if (len == 0)
        return 0;

    /*
     * TODO: use the ossltest engine (optionally?) to disable crypto checks.
     */

    /* This only fuzzes the initial flow from the client so far. */
    ctx = SSL_CTX_new(SSLv23_method());

    client = SSL_new(ctx);
    OPENSSL_assert(SSL_set_cipher_list(client, "ALL:eNULL:@SECLEVEL=0") == 1);
    SSL_set_tlsext_host_name(client, "localhost");
    in = BIO_new(BIO_s_mem());
    out = BIO_new(BIO_s_mem());
    SSL_set_bio(client, in, out);
    SSL_set_connect_state(client);
    OPENSSL_assert((size_t)BIO_write(in, buf, len) == len);
    if (SSL_do_handshake(client) == 1) {
        /* Keep reading application data until error or EOF. */
        uint8_t tmp[1024];
        for (;;) {
            if (SSL_read(client, tmp, sizeof(tmp)) <= 0) {
                break;
            }
        }
    }
    SSL_free(client);
    ERR_clear_error();
    SSL_CTX_free(ctx);

    return 0;
}

void FuzzerCleanup(void)
{
}
