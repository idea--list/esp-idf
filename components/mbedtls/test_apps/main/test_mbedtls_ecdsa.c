/* mbedTLS Elliptic Curve Digital Signature performance tests
 *
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <esp_log.h>

#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/error.h>

#include "test_utils.h"
#include "ccomp_timer.h"
#include "unity.h"

#define TEST_ASSERT_MBEDTLS_OK(X) TEST_ASSERT_EQUAL_HEX32(0, -(X))

#if CONFIG_NEWLIB_NANO_FORMAT
#define NEWLIB_NANO_COMPAT_FORMAT             PRIu32
#define NEWLIB_NANO_COMPAT_CAST(int64_t_var)  (uint32_t)int64_t_var
#else
#define NEWLIB_NANO_COMPAT_FORMAT             PRId64
#define NEWLIB_NANO_COMPAT_CAST(int64_t_var)  int64_t_var
#endif

#if CONFIG_MBEDTLS_HARDWARE_ECC || CONFIG_MBEDTLS_HARDWARE_ECDSA_VERIFY

/*
 * All the following values are in big endian format, as required by the mbedTLS APIs
 */

const uint8_t sha[] = {
    0x0c, 0xaa, 0x08, 0xb4, 0xf0, 0x89, 0xd3, 0x45,
    0xbb, 0x55, 0x98, 0xd9, 0xc2, 0xe9, 0x65, 0x5d,
    0x7e, 0xa3, 0xa9, 0xc3, 0xcd, 0x69, 0xb1, 0xcf,
    0x91, 0xbe, 0x58, 0x10, 0xfe, 0x80, 0x65, 0x6e
};

const uint8_t ecdsa256_r[] = {
    0x26, 0x1a, 0x0f, 0xbd, 0xa5, 0xe5, 0x1e, 0xe7,
    0xb3, 0xc3, 0xb7, 0x09, 0xd1, 0x4a, 0x7a, 0x2a,
    0x16, 0x69, 0x4b, 0xaf, 0x76, 0x5c, 0xd4, 0x0e,
    0x93, 0x57, 0xb8, 0x67, 0xf9, 0xa1, 0xe5, 0xe8
};

const uint8_t ecdsa256_s[] = {
    0x63, 0x59, 0xc0, 0x3b, 0x6a, 0xc2, 0xc4, 0xc4,
    0xaf, 0x47, 0x5c, 0xe6, 0x6d, 0x43, 0x3b, 0xa7,
    0x91, 0x51, 0x15, 0x62, 0x7e, 0x46, 0x0e, 0x68,
    0x84, 0xce, 0x72, 0xa0, 0xd8, 0x8b, 0x69, 0xd5
};

const uint8_t ecdsa256_pub_x[] = {
    0xcb, 0x59, 0xde, 0x9c, 0xbb, 0x28, 0xaa, 0xac,
    0x72, 0x06, 0xc3, 0x43, 0x2a, 0x65, 0x82, 0xcc,
    0x68, 0x01, 0x76, 0x68, 0xfc, 0xec, 0xf5, 0x91,
    0xd1, 0x9e, 0xbf, 0xcf, 0x67, 0x7d, 0x7d, 0xbe
};

const uint8_t ecdsa256_pub_y[] = {
    0x00, 0x66, 0x14, 0x74, 0xe0, 0x06, 0x44, 0x66,
    0x6f, 0x3b, 0x8c, 0x3b, 0x2d, 0x05, 0xf6, 0xd5,
    0xb2, 0x5d, 0xe4, 0x85, 0x6c, 0x61, 0x38, 0xc5,
    0xb1, 0x21, 0xde, 0x2b, 0x44, 0xf5, 0x13, 0x62
};

const uint8_t ecdsa192_r[] = {
    0x2b, 0x8a, 0x18, 0x2f, 0xb2, 0x75, 0x26, 0xb7,
    0x1c, 0xe1, 0xe2, 0x6d, 0xaa, 0xe7, 0x74, 0x2c,
    0x42, 0xc8, 0xd5, 0x09, 0x4f, 0xb7, 0xee, 0x9f
};

const uint8_t ecdsa192_s[] = {
    0x1a, 0x74, 0xb4, 0x5, 0xf4, 0x28, 0xa5, 0xb6,
    0xce, 0xed, 0xa5, 0xff, 0xa8, 0x60, 0x06, 0x2f,
    0xf6, 0xeb, 0x24, 0x59, 0x24, 0x30, 0x5b, 0x12
};

const uint8_t ecdsa192_pub_x[] = {
    0xd0, 0x3f, 0x6f, 0xe7, 0x5d, 0xaa, 0xf4, 0xc0,
    0x1e, 0x63, 0x7b, 0x82, 0xab, 0x23, 0x33, 0x34,
    0x74, 0x59, 0x56, 0x5d, 0x21, 0x10, 0x9c, 0xb1
};

const uint8_t ecdsa192_pub_y[] = {
    0x85, 0xfc, 0x76, 0xcb, 0x65, 0xbc, 0xc4, 0xbe,
    0x74, 0x09, 0xfd, 0xf3, 0x74, 0xdc, 0xc2, 0xde,
    0x7e, 0x4b, 0x23, 0xad, 0x46, 0x5c, 0x87, 0xc2
};

void test_ecdsa_verify(mbedtls_ecp_group_id id, const uint8_t *hash, const uint8_t *r_comp, const uint8_t *s_comp,
                       const uint8_t *pub_x, const uint8_t *pub_y)
{
    int64_t elapsed_time;
    mbedtls_mpi r, s;

    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    mbedtls_ecdsa_context ecdsa_context;
    mbedtls_ecdsa_init(&ecdsa_context);

    mbedtls_ecp_group_load(&ecdsa_context.MBEDTLS_PRIVATE(grp), id);
    size_t plen = mbedtls_mpi_size(&ecdsa_context.MBEDTLS_PRIVATE(grp).P);

    TEST_ASSERT_MBEDTLS_OK(mbedtls_mpi_read_binary(&r, r_comp, plen));

    TEST_ASSERT_MBEDTLS_OK(mbedtls_mpi_read_binary(&s, s_comp, plen));

    TEST_ASSERT_MBEDTLS_OK(mbedtls_mpi_read_binary(&ecdsa_context.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(X), pub_x, plen));
    TEST_ASSERT_MBEDTLS_OK(mbedtls_mpi_read_binary(&ecdsa_context.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(Y), pub_y, plen));
    TEST_ASSERT_MBEDTLS_OK(mbedtls_mpi_lset(&ecdsa_context.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(Z), 1));

    ccomp_timer_start();
    TEST_ASSERT_MBEDTLS_OK(mbedtls_ecdsa_verify(&ecdsa_context.MBEDTLS_PRIVATE(grp), hash, 32, &ecdsa_context.MBEDTLS_PRIVATE(Q), &r, &s));
    elapsed_time = ccomp_timer_stop();

    if (id == MBEDTLS_ECP_DP_SECP192R1) {
        TEST_PERFORMANCE_CCOMP_LESS_THAN(ECDSA_P192_VERIFY_OP, "%" NEWLIB_NANO_COMPAT_FORMAT" us", NEWLIB_NANO_COMPAT_CAST(elapsed_time));
    } else if (id == MBEDTLS_ECP_DP_SECP256R1) {
        TEST_PERFORMANCE_CCOMP_LESS_THAN(ECDSA_P256_VERIFY_OP, "%" NEWLIB_NANO_COMPAT_FORMAT" us", NEWLIB_NANO_COMPAT_CAST(elapsed_time));
    }

    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    mbedtls_ecdsa_free(&ecdsa_context);
}

TEST_CASE("mbedtls ECDSA signature verification performance on SECP192R1", "[mbedtls]")
{
    test_ecdsa_verify(MBEDTLS_ECP_DP_SECP192R1, sha, ecdsa192_r, ecdsa192_s,
                 ecdsa192_pub_x, ecdsa192_pub_y);
}

TEST_CASE("mbedtls ECDSA signature verification performance on SECP256R1", "[mbedtls]")
{
    test_ecdsa_verify(MBEDTLS_ECP_DP_SECP256R1, sha, ecdsa256_r, ecdsa256_s,
                 ecdsa256_pub_x, ecdsa256_pub_y);
}

#endif /* CONFIG_MBEDTLS_HARDWARE_ECC */
