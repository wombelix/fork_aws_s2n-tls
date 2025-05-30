/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <stdio.h>
#include <string.h>

#include "api/s2n.h"
#include "crypto/s2n_pq.h"
#include "s2n_test.h"
#include "stuffer/s2n_stuffer.h"
#include "tests/testlib/s2n_nist_kats.h"
#include "tls/s2n_cipher_suites.h"
#include "tls/s2n_prf.h"
#include "utils/s2n_safety.h"

#define KAT_FILE_NAME "kats/hybrid_prf.kat"

/* The lengths for premaster_kem_secret and client_key_exchange_message must be defined in the KAT file,
 * since they vary based on which KEM is being used. The other lengths are fixed and can be defined here. */
#define PREMASTER_CLASSIC_SECRET_LENGTH 48
#define CLIENT_RANDOM_LENGTH            32
#define SERVER_RANDOM_LENGTH            32
#define MASTER_SECRET_LENGTH            48

#define NUM_TEST_VECTORS 10

int main(int argc, char **argv)
{
    BEGIN_TEST();
    EXPECT_SUCCESS(s2n_disable_tls13_in_test());

    if (!s2n_pq_is_enabled()) {
        /* The hybrid PRF sets a seed too large for the openssl PRF,
         * but s2n-tls doesn't support PQ with openssl anyway.
         *
         * Only run this test in environments where PQ is possible.
         */
        END_TEST();
    }

    FILE *kat_file = fopen(KAT_FILE_NAME, "r");
    EXPECT_NOT_NULL(kat_file);

    uint8_t premaster_classic_secret[PREMASTER_CLASSIC_SECRET_LENGTH];
    uint8_t client_random[CLIENT_RANDOM_LENGTH];
    uint8_t server_random[SERVER_RANDOM_LENGTH];
    uint8_t expected_master_secret[MASTER_SECRET_LENGTH];

    for (uint32_t i = 0; i < NUM_TEST_VECTORS; i++) {
        /* Verify test index */
        uint32_t count = 0;
        POSIX_GUARD(FindMarker(kat_file, "count = "));
        POSIX_ENSURE_GT(fscanf(kat_file, "%u", &count), 0);
        POSIX_ENSURE_EQ(count, i);

        struct s2n_connection *conn = NULL;
        EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_SERVER));
        conn->actual_protocol_version = S2N_TLS12;
        /* Really only need for the hash function in the PRF */
        conn->secure->cipher_suite = &s2n_ecdhe_rsa_with_aes_256_gcm_sha384;

        /* Read test vector from KAT file */
        uint32_t premaster_kem_secret_length = 0;
        uint32_t client_key_exchange_message_length = 0;

        POSIX_GUARD(ReadHex(kat_file, premaster_classic_secret, PREMASTER_CLASSIC_SECRET_LENGTH, "premaster_classic_secret = "));

        POSIX_GUARD(FindMarker(kat_file, "premaster_kem_secret_length = "));
        POSIX_ENSURE_GT(fscanf(kat_file, "%u", &premaster_kem_secret_length), 0);

        uint8_t *premaster_kem_secret = NULL;
        POSIX_ENSURE_REF(premaster_kem_secret = malloc(premaster_kem_secret_length));
        POSIX_GUARD(ReadHex(kat_file, premaster_kem_secret, premaster_kem_secret_length, "premaster_kem_secret = "));

        POSIX_GUARD(ReadHex(kat_file, client_random, CLIENT_RANDOM_LENGTH, "client_random = "));
        POSIX_GUARD(ReadHex(kat_file, server_random, SERVER_RANDOM_LENGTH, "server_random = "));

        POSIX_GUARD(FindMarker(kat_file, "client_key_exchange_message_length = "));
        POSIX_ENSURE_GT(fscanf(kat_file, "%u", &client_key_exchange_message_length), 0);

        uint8_t *client_key_exchange_message = NULL;
        POSIX_ENSURE_REF(client_key_exchange_message = malloc(client_key_exchange_message_length));
        POSIX_GUARD(ReadHex(kat_file, client_key_exchange_message, client_key_exchange_message_length, "client_key_exchange_message = "));

        POSIX_GUARD(ReadHex(kat_file, expected_master_secret, MASTER_SECRET_LENGTH, "master_secret = "));

        struct s2n_blob classic_pms = { 0 };
        EXPECT_SUCCESS(s2n_blob_init(&classic_pms, premaster_classic_secret, PREMASTER_CLASSIC_SECRET_LENGTH));
        struct s2n_blob kem_pms = { 0 };
        EXPECT_SUCCESS(s2n_blob_init(&kem_pms, premaster_kem_secret, premaster_kem_secret_length));

        /* In the future the hybrid_kex client_key_send (client side) and client_key_receive (server side) will concatenate the two parts */
        DEFER_CLEANUP(struct s2n_blob combined_pms = { 0 }, s2n_free);
        EXPECT_SUCCESS(s2n_alloc(&combined_pms, classic_pms.size + kem_pms.size));
        struct s2n_stuffer combined_stuffer = { 0 };
        EXPECT_SUCCESS(s2n_stuffer_init(&combined_stuffer, &combined_pms));
        EXPECT_SUCCESS(s2n_stuffer_write(&combined_stuffer, &classic_pms));
        EXPECT_SUCCESS(s2n_stuffer_write(&combined_stuffer, &kem_pms));

        EXPECT_MEMCPY_SUCCESS(conn->handshake_params.client_random, client_random, CLIENT_RANDOM_LENGTH);
        EXPECT_MEMCPY_SUCCESS(conn->handshake_params.server_random, server_random, SERVER_RANDOM_LENGTH);

        EXPECT_SUCCESS(s2n_alloc(&conn->kex_params.client_key_exchange_message, client_key_exchange_message_length));

        EXPECT_MEMCPY_SUCCESS(conn->kex_params.client_key_exchange_message.data, client_key_exchange_message, client_key_exchange_message_length);

        EXPECT_SUCCESS(s2n_prf_hybrid_master_secret(conn, &combined_pms));
        EXPECT_BYTEARRAY_EQUAL(expected_master_secret, conn->secrets.version.tls12.master_secret, S2N_TLS_SECRET_LEN);
        EXPECT_SUCCESS(s2n_free(&conn->kex_params.client_key_exchange_message));
        EXPECT_SUCCESS(s2n_connection_free(conn));

        free(premaster_kem_secret);
        free(client_key_exchange_message);
    }

    if (FindMarker(kat_file, "count = ") == 0) {
        FAIL_MSG("Found unexpected test vectors in the KAT file. Has the KAT file been changed? Did you update NUM_TEST_VECTORS?");
    }

    fclose(kat_file);
    END_TEST();
}
