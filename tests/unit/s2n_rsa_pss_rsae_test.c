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

#include <openssl/evp.h>
#include <openssl/rsa.h>

#include "crypto/s2n_hash.h"
#include "crypto/s2n_rsa_pss.h"
#include "error/s2n_errno.h"
#include "s2n_test.h"
#include "stuffer/s2n_stuffer.h"
#include "testlib/s2n_testlib.h"
#include "tls/s2n_config.h"
#include "tls/s2n_connection.h"
#include "utils/s2n_random.h"
#include "utils/s2n_safety.h"

#define HASH_ALG         S2N_HASH_SHA256
#define HASH_LENGTH      256
#define RANDOM_BLOB_SIZE RSA_PSS_SIGN_VERIFY_RANDOM_BLOB_SIZE

#define hash_state_new(name, input)                                   \
    DEFER_CLEANUP(struct s2n_hash_state name = { 0 }, s2n_hash_free); \
    EXPECT_SUCCESS(s2n_hash_new(&name));                              \
    EXPECT_SUCCESS(s2n_hash_init(&name, HASH_ALG));                   \
    EXPECT_SUCCESS(s2n_hash_update(&name, (input).data, (input).size));

#define hash_state_for_alg_new(name, hash_alg, input)                 \
    DEFER_CLEANUP(struct s2n_hash_state name = { 0 }, s2n_hash_free); \
    EXPECT_SUCCESS(s2n_hash_new(&name));                              \
    EXPECT_SUCCESS(s2n_hash_init(&name, hash_alg));                   \
    EXPECT_SUCCESS(s2n_hash_update(&name, (input).data, (input).size));

struct s2n_rsa_pss_test_case {
    s2n_hash_algorithm hash_alg;
    const char *key_param_n;
    const char *key_param_e;
    const char *key_param_d;
    const char *message;
    const char *signature;
    bool should_pass;
};

/* Small selection of NIST test vectors, taken from:
 * https://csrc.nist.gov/Projects/cryptographic-algorithm-validation-program/Digital-Signatures
 *
 * Note that TLS1.3 requires that the saltlen match the digest length.
 * Not all NIST vectors satisfy that requirement.
 * Specifically, none of the FIPS 186-2 vectors satisfy that requirement: they all use a 20 byte salt.
 * So the tests here are a selection of the FIPS 186-3 vectors.
 */
const struct s2n_rsa_pss_test_case test_cases[] = {
    {
            .hash_alg = S2N_HASH_SHA224,
            .key_param_n = "acfdfb1dcb1459b489cd4a8c9fab64a7da4f044bef1c506f0872e9476f3357abda509d9fb1db6a4f5306c40c058826253171cfedbc160776a48ec35b655bb9963286b6aece1c77dff987a0aae9720ba035dda67f317101bd3cd4e6caac867a8c38b87067938e96e72df1875f94e43e4c06f7a86a1dbe07836ee69763eee29bc13ca906d7740c29e651872a9ec7c6237f7c8290bb0800a030b323d09e7c903751d21a224266f9d6c94c17a4c0cd8175ea67b9d9020f2b3f31a96206084cddb2acef70b11ae25a46c4f6817c4813466d7cac76b27927145bd499ff87f22a946b688e980a00a3d54c72ab9c2c88a55a3ea4c6784068673532737cbe4799e98bd711",
            .key_param_e = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000b29bc5",
            .key_param_d = "29d83f1a032c6a1702d25eb47ee39a09969b83b3ecd764c9ce9cbe08e4dba20d711931820a6d90d26cd666d4eb842f66c12f189d13f67df06eb57edd4e37290a4adbf65a0fe5684e95d8287d9df3b32d3ca674bac95d13adb11dc40121f720df8dc2b5551fd98e167159e2eecdbcf7273a53ddafd8ee08ec9f857c1cfae59c282d3b13cc7dfa8fa85f68517460186f5b72faebe431d262af40d04ad00fd86e2b2cad64d86dcb5f738fc627eb4fcc3fd5fdd5742bf48959701a841016ae6f552a0e40c9279c544baf7c9c467e4958c7018c9f2a4becd9e03f40b4e854acb6534b1b3b5fd27fc085318fd7174a48660ef647574e0f2b99783fb17cece00096d601",
            .message = "7ced8a54c8e35ef5c87d03ee6357b658e2e528eda55ad30f14c88d0cd9895ea04ddf8fbb2fd703859c73cb9f3b07f4acb9e4a311753465f87c25c09bb74a0ebf633e8b7ec28aac4a10c8b22fb9098058c975a9d5a431ce9cf78627cdee3f5f3aa852a526e8c3004d0dc6e22544240164fcdf62c29a19b6006e32ea29e631fa18",
            .signature = "8c074bae48454875aefa2b7ea090d11d8860d7cbee5ee4c3ed02cb45aadb0b4516872b0e4521789d503b4e70092ca2a0c7a88efb7d74c63ce8dffcf06995af7c9567a8df05a01b243c5f3edcfa3922d06967bec9d0faad2c84486dd38602a416ef253e4a28f74ca290e4d743accccd204d8b136dd197e7a2f25a2707f339c6ba444c19bc047dff0584c479ab07c2ae68f219c3c430f19cae3f711c0efab8d09f85ab66ae948c357db67078a359b9c746d2d66b31486c83765ab097b540b5e6f626c9111a295855dff5c2acea102f6a29b9569909dad0d4c79a941a3e71b3137dc68ae2296b6f8175bfab205432e409be9075d12580b5c14924dd53c3d44745d7",
            .should_pass = true,
    },
    {
            .hash_alg = S2N_HASH_SHA224,
            .key_param_n = "acfdfb1dcb1459b489cd4a8c9fab64a7da4f044bef1c506f0872e9476f3357abda509d9fb1db6a4f5306c40c058826253171cfedbc160776a48ec35b655bb9963286b6aece1c77dff987a0aae9720ba035dda67f317101bd3cd4e6caac867a8c38b87067938e96e72df1875f94e43e4c06f7a86a1dbe07836ee69763eee29bc13ca906d7740c29e651872a9ec7c6237f7c8290bb0800a030b323d09e7c903751d21a224266f9d6c94c17a4c0cd8175ea67b9d9020f2b3f31a96206084cddb2acef70b11ae25a46c4f6817c4813466d7cac76b27927145bd499ff87f22a946b688e980a00a3d54c72ab9c2c88a55a3ea4c6784068673532737cbe4799e98bd711",
            .key_param_e = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000b29bc5",
            .key_param_d = "29d83f1a032c6a1702d25eb47ee39a09969b83b3ecd764c9ce9cbe08e4dba20d711931820a6d90d26cd666d4eb842f66c12f189d13f67df06eb57edd4e37290a4adbf65a0fe5684e95d8287d9df3b32d3ca674bac95d13adb11dc40121f720df8dc2b5551fd98e167159e2eecdbcf7273a53ddafd8ee08ec9f857c1cfae59c282d3b13cc7dfa8fa85f68517460186f5b72faebe431d262af40d04ad00fd86e2b2cad64d86dcb5f738fc627eb4fcc3fd5fdd5742bf48959701a841016ae6f552a0e40c9279c544baf7c9c467e4958c7018c9f2a4becd9e03f40b4e854acb6534b1b3b5fd27fc085318fd7174a48660ef647574e0f2b99783fb17cece00096d601",
            .message = "23708e055e891826f8011b1f48a1c7ad24b5d008f9c91ec31ab1c5f358cc3793d389b927102913a04bc78c800e96153e026bc5ec067b85177e650defed730fa7c71cb11f80ce41c1e9eb24a9bc121008759f7cca6475547601fbf0567f6447d9a4346035d7ff0a507b74cde17b9b20d2265bdcea3e3ff1a84b7a5872352849ef",
            .signature = "a8cbf34ffefba916abf4961a6a1032c01bc4aeb8278ba42a18f4f67fdfeab49ebb05cc289aba475a9667649c6ecf6a8ca17ad81f17d5109807c2b260ac440e134e55ee429213cb8e8a2314e7049019fe95ba36e3ec1ed1e77108e4fd84abee28423996e3fbba3d38b285a055d6390d3d8f5b21415e6874ab37423efea5726d47bbfed125494a2164eab12394f89843f8d8a5bd3bf05b31b598e4caa0ad2a8cad4826d238e4ab8d005379f100f97398d896684b08753c89863c224d65a475443d51a91ed668cac691ea7030aa737c3eeeb9db33cff17ce9a8e399bb2b3cdcf2444567398db196fc86ba76114dbd13096cabd692b083fea1c7a94e6564c2e8c093",
            .should_pass = false, /* (1 - Message changed) */
    },
    {
            .hash_alg = S2N_HASH_SHA224,
            .key_param_n = "acfdfb1dcb1459b489cd4a8c9fab64a7da4f044bef1c506f0872e9476f3357abda509d9fb1db6a4f5306c40c058826253171cfedbc160776a48ec35b655bb9963286b6aece1c77dff987a0aae9720ba035dda67f317101bd3cd4e6caac867a8c38b87067938e96e72df1875f94e43e4c06f7a86a1dbe07836ee69763eee29bc13ca906d7740c29e651872a9ec7c6237f7c8290bb0800a030b323d09e7c903751d21a224266f9d6c94c17a4c0cd8175ea67b9d9020f2b3f31a96206084cddb2acef70b11ae25a46c4f6817c4813466d7cac76b27927145bd499ff87f22a946b688e980a00a3d54c72ab9c2c88a55a3ea4c6784068673532737cbe4799e98bd711",
            .key_param_e = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000eaf989",
            .key_param_d = "29d83f1a032c6a1702d25eb47ee39a09969b83b3ecd764c9ce9cbe08e4dba20d711931820a6d90d26cd666d4eb842f66c12f189d13f67df06eb57edd4e37290a4adbf65a0fe5684e95d8287d9df3b32d3ca674bac95d13adb11dc40121f720df8dc2b5551fd98e167159e2eecdbcf7273a53ddafd8ee08ec9f857c1cfae59c282d3b13cc7dfa8fa85f68517460186f5b72faebe431d262af40d04ad00fd86e2b2cad64d86dcb5f738fc627eb4fcc3fd5fdd5742bf48959701a841016ae6f552a0e40c9279c544baf7c9c467e4958c7018c9f2a4becd9e03f40b4e854acb6534b1b3b5fd27fc085318fd7174a48660ef647574e0f2b99783fb17cece00096d601",
            .message = "cd69ac8d1c46f54d46cc9aff98e078521357a36177b91392a0459abe15351993df43f437976bc32ec3f34b7e63a9ac11c66cdb5aa2ed533c64b70827c56d9d849e53d653fd10501278b370e1c1f399e57bbba2ea4ce6874c34475e171dec8d6db3a33b2875be04c10c14f171dc48a795da4ede579d2a158bb7fb84d745395317",
            .signature = "5725c1ac60b2d610977fae6c52a94155d687d575aa2f5cee817dbf77e1ed692266c0ff14a2105b343dcbb075c43ee6a839a64bd1d151d3801e62f9d76c8d94787c29d1e88ca2ea7f23d7ffe1aac3d5072f556a8087516df5dff9fcde5b0e849d08f1b42707d2cda2eac462bc8810737e03d47dc8240db73c3ccf7106886f8ae9c4c36473493cb5242e2d70bd5c36be35ce1e5aa5286d6439b247fb64ba06af222b133f05d39e6a44a1ad6ad567b9e0b5655486af0b9cfef1d714f311dfcd983bd90674f964f64243be02feb3a202cd5f945505826fda112bba3e5b837bdcb9e974953ee7b1ad7a6e21a0a64268d52eaba083c6719632c21e6615fe4395ce3f53",
            .should_pass = false, /* (2 - Public Key e changed ) */
    },
    {
            .hash_alg = S2N_HASH_SHA224,
            .key_param_n = "acfdfb1dcb1459b489cd4a8c9fab64a7da4f044bef1c506f0872e9476f3357abda509d9fb1db6a4f5306c40c058826253171cfedbc160776a48ec35b655bb9963286b6aece1c77dff987a0aae9720ba035dda67f317101bd3cd4e6caac867a8c38b87067938e96e72df1875f94e43e4c06f7a86a1dbe07836ee69763eee29bc13ca906d7740c29e651872a9ec7c6237f7c8290bb0800a030b323d09e7c903751d21a224266f9d6c94c17a4c0cd8175ea67b9d9020f2b3f31a96206084cddb2acef70b11ae25a46c4f6817c4813466d7cac76b27927145bd499ff87f22a946b688e980a00a3d54c72ab9c2c88a55a3ea4c6784068673532737cbe4799e98bd711",
            .key_param_e = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000b29bc5",
            .key_param_d = "29d83f1a032c6a1702d25eb47ee39a09969b83b3ecd764c9ce9cbe08e4dba20d711931820a6d90d26cd666d4eb842f66c12f189d13f67df06eb57edd4e37290a4adbf65a0fe5684e95d8287d9df3b32d3ca674bac95d13adb11dc40121f720df8dc2b5551fd98e167159e2eecdbcf7273a53ddafd8ee08ec9f857c1cfae59c282d3b13cc7dfa8fa85f68517460186f5b72faebe431d262af40d04ad00fd86e2b2cad64d86dcb5f738fc627eb4fcc3fd5fdd5742bf48959701a841016ae6f552a0e40c9279c544baf7c9c467e4958c7018c9f2a4becd9e03f40b4e854acb6534b1b3b5fd27fc085318fd7174a48660ef647574e0f2b99783fb17cece00096d601",
            .message = "0af6365d6065dfc44795d87a8119fb44cb8223e0fb26e1eb4b207102f598ec280045be67592f5bba25ba2e2b56e0d2397cbe857cde52da8cca83ae1e29615c7056af35e8319f2af86fdccc4434cd7707e319c9b2356659d78867a6467a154e76b73c81260f3ab443cc039a0d42695076a79bd8ca25ebc8952ed443c2103b2900",
            .signature = "3b32dab565d2f0a5ac267ba8fd4c381f5b0c40f20c19f0dc7b236232a8bdb55c1c33f13f4f74266d7628767763582bb0fb526263e2a4521e09a3207ee5fd7cfdcdac85ccffe3e1048f590dadc19035bf2f3b5b954e8c783f6bd3e99d98930aea8eced5e3119331be1d606410d16d0e5ef069179cd6b513b94b36c1cfe6ad64ad285e818b58c4c89bf9330c4bcec96d08ba21a35cd4851da1f405d928df3fcfeb85108171bb4ae5b0310193bd8b5b861983b7a2483ba6bccfbf2d6e15bac1e2f2f8363c6154546744e301fd2661ebb3a29030d269c8954ace992b075c63ac12a992eed4956cb2a48495d82c47fa68e66740d38a80caa69190b19730ed7d72835b",
            .should_pass = false, /* (3 - Signature changed ) */
    },
    {
            .hash_alg = S2N_HASH_SHA224,
            .key_param_n = "acfdfb1dcb1459b489cd4a8c9fab64a7da4f044bef1c506f0872e9476f3357abda509d9fb1db6a4f5306c40c058826253171cfedbc160776a48ec35b655bb9963286b6aece1c77dff987a0aae9720ba035dda67f317101bd3cd4e6caac867a8c38b87067938e96e72df1875f94e43e4c06f7a86a1dbe07836ee69763eee29bc13ca906d7740c29e651872a9ec7c6237f7c8290bb0800a030b323d09e7c903751d21a224266f9d6c94c17a4c0cd8175ea67b9d9020f2b3f31a96206084cddb2acef70b11ae25a46c4f6817c4813466d7cac76b27927145bd499ff87f22a946b688e980a00a3d54c72ab9c2c88a55a3ea4c6784068673532737cbe4799e98bd711",
            .key_param_e = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000b29bc5",
            .key_param_d = "29d83f1a032c6a1702d25eb47ee39a09969b83b3ecd764c9ce9cbe08e4dba20d711931820a6d90d26cd666d4eb842f66c12f189d13f67df06eb57edd4e37290a4adbf65a0fe5684e95d8287d9df3b32d3ca674bac95d13adb11dc40121f720df8dc2b5551fd98e167159e2eecdbcf7273a53ddafd8ee08ec9f857c1cfae59c282d3b13cc7dfa8fa85f68517460186f5b72faebe431d262af40d04ad00fd86e2b2cad64d86dcb5f738fc627eb4fcc3fd5fdd5742bf48959701a841016ae6f552a0e40c9279c544baf7c9c467e4958c7018c9f2a4becd9e03f40b4e854acb6534b1b3b5fd27fc085318fd7174a48660ef647574e0f2b99783fb17cece00096d601",
            .message = "c9a121fa15bffefb864fe3cfc2b1bf775886be3ff5151c40daee3c288dccf43ecfc02ba0cf8ca7cf9d4d206ee15e9947cd78f08f501eb36b8d3835b38bfee1f52e17cfbd66029513a6b66046988543e80f46ce1a3db3e30a2610c5b9540e7202ccee33d842e971cf89d0cadd4df0646204388167229e54238cbe14c450c44e6c",
            .signature = "7d081b0e38d73256476358c013908f3497810ac4bd5d76252f3ef440c5742ea552ef5348eccf6ef13afdf616288dbbf05a5b25e66a8745d9ddf66639ca89366e28062529e80b655b0268e922c8d77eb8ceee830fa14c15238dc1442dfdaffbf92436c794c24f6104374bf131616bcf4cca12608cee203e7eb0eba04556a0a0ac3dea9fa39dac8082e39f9a739955e036d0636008f5c3c4f823a5e6e5229fde6b94f986520de9b9a77ca34ccc0236762c77e33e105a9346553dc0e4469d3929ec38e8813a551af26b0f7d9dbb12d40b7d9fe195948e1ea1245362b01faeb4157605a9ab5158a7bc4df644f12121b03ee22e9f23fa5f40067b333b865bf3e41244",
            .should_pass = false, /* (4 - Format of the EM is incorrect - hash moved to left ) */
    },
    {
            .hash_alg = S2N_HASH_SHA224,
            .key_param_n = "acfdfb1dcb1459b489cd4a8c9fab64a7da4f044bef1c506f0872e9476f3357abda509d9fb1db6a4f5306c40c058826253171cfedbc160776a48ec35b655bb9963286b6aece1c77dff987a0aae9720ba035dda67f317101bd3cd4e6caac867a8c38b87067938e96e72df1875f94e43e4c06f7a86a1dbe07836ee69763eee29bc13ca906d7740c29e651872a9ec7c6237f7c8290bb0800a030b323d09e7c903751d21a224266f9d6c94c17a4c0cd8175ea67b9d9020f2b3f31a96206084cddb2acef70b11ae25a46c4f6817c4813466d7cac76b27927145bd499ff87f22a946b688e980a00a3d54c72ab9c2c88a55a3ea4c6784068673532737cbe4799e98bd711",
            .key_param_e = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000b29bc5",
            .key_param_d = "29d83f1a032c6a1702d25eb47ee39a09969b83b3ecd764c9ce9cbe08e4dba20d711931820a6d90d26cd666d4eb842f66c12f189d13f67df06eb57edd4e37290a4adbf65a0fe5684e95d8287d9df3b32d3ca674bac95d13adb11dc40121f720df8dc2b5551fd98e167159e2eecdbcf7273a53ddafd8ee08ec9f857c1cfae59c282d3b13cc7dfa8fa85f68517460186f5b72faebe431d262af40d04ad00fd86e2b2cad64d86dcb5f738fc627eb4fcc3fd5fdd5742bf48959701a841016ae6f552a0e40c9279c544baf7c9c467e4958c7018c9f2a4becd9e03f40b4e854acb6534b1b3b5fd27fc085318fd7174a48660ef647574e0f2b99783fb17cece00096d601",
            .message = "4cde2a6d9ecfdefa3c631c95aa9933634ae12668db8b53a03f80524b8130208816cb7d2563b492b68033d7e43c6a3408618a67f93946a521508884d77c6318e91b4a5c779c7fd40841cd71d7227ab56e767817760edba9ce2290f8da504b341ee2c1910b5018ec18059bb21566b3febc1112018a6232a7cd3cfe77fd06cfbc4f",
            .signature = "4151dbee3d483d20b57b51d80bd2d382fc85575fa031582a88a135b2615c0a079bb08c2cb7ba6020c35706aadfb6bc35d69ce4df0b2e5fca1ea62a807e9765501571d6d123ae114afc19b26ba4b5db1b0522859d2269117059651053b9d72cc253c90285652a7e3094dce3eb5c9ef5fbf35eb268e3b8fe693c2bfc4bbb8682d709a12f3038fd04629bc48ae13ee91112741dab717c58856cc4215eedaeedd57edb2926085ebc05d15038f0deba90b00e18e710e64d31ff563a1ac88ecf9cadca2845babe9dd26771f0a629976fe13ba14f7c9ca37b98bb3a9d96ca9d72273da78ff3b17f6d7aa2645c42e3251e9161c958bd6f10d20f08fe2437aa79d73f9081",
            .should_pass = false, /* (5 - Format of the EM is incorrect - 00 on end of pad removed ) */
    },
    {
            .hash_alg = S2N_HASH_SHA256,
            .key_param_n = "a47d04e7cacdba4ea26eca8a4c6e14563c2ce03b623b768c0d49868a57121301dbf783d82f4c055e73960e70550187d0af62ac3496f0a3d9103c2eb7919a72752fa7ce8c688d81e3aee99468887a15288afbb7acb845b7c522b5c64e678fcd3d22feb84b44272700be527d2b2025a3f83c2383bf6a39cf5b4e48b3cf2f56eef0dfff18555e31037b915248694876f3047814415164f2c660881e694b58c28038a032ad25634aad7b39171dee368e3d59bfb7299e4601d4587e68caaf8db457b75af42fc0cf1ae7caced286d77fac6cedb03ad94f1433d2c94d08e60bc1fdef0543cd2951e765b38230fdd18de5d2ca627ddc032fe05bbd2ff21e2db1c2f94d8b",
            .key_param_e = "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010e43f",
            .key_param_d = "11a0dd285f66471a8da30bcb8c24a1d5c8db942fc9920797ca442460a800b75bbc738beb8ee0e874b053e64707df4cfc7837c40e5be68b8a8e1d0145169ca6271d81887e19a1cd95b2fd0de0dba347fe637bcc6cdc24eebe03c24d4cf3a5c6154d78f141fe34169924d0f89533658eacfdeae99ce1a88027c18ff92653a835aa3891bfffcd388ffc2388ce2b10568543750502ccbc69c0088f1d690e97a5f5bdd1888cd2faa43c04ae24539522dde2d9c202f655fc55754440b53a1532aab47851f60b7a067e240b738e1b1daae6ca0d59eeae27686cd88857e9adadc2d4b82b07a61a358456aaf807669693ffb13c9964a63654cadc81ee59df511ca3a4bd67",
            .message = "e002377affb04f0fe4598de9d92d31d6c786040d5776976556a2cfc55e54a1dcb3cb1b126bd6a4bed2a184990ccea773fcc79d246553e6c64f686d21ad4152673cafec22aeb40f6a084e8a5b4991f4c64cf8a927effd0fd775e71e8329e41fdd4457b3911173187b4f09a817d79ea2397fc12dfe3d9c9a0290c8ead31b6690a6",
            .signature = "4f9b425c2058460e4ab2f5c96384da2327fd29150f01955a76b4efe956af06dc08779a374ee4607eab61a93adc5608f4ec36e47f2a0f754e8ff839a8a19b1db1e884ea4cf348cd455069eb87afd53645b44e28a0a56808f5031da5ba9112768dfbfca44ebe63a0c0572b731d66122fb71609be1480faa4e4f75e43955159d70f081e2a32fbb19a48b9f162cf6b2fb445d2d6994bc58910a26b5943477803cdaaa1bd74b0da0a5d053d8b1dc593091db5388383c26079f344e2aea600d0e324164b450f7b9b465111b7265f3b1b063089ae7e2623fc0fda8052cf4bf3379102fbf71d7c98e8258664ceed637d20f95ff0111881e650ce61f251d9c3a629ef222d",
            .should_pass = true,
    },
    {
            .hash_alg = S2N_HASH_SHA256,
            .key_param_n = "ce4924ff470fb99d17f66595561a74ded22092d1dc27122ae15ca8cac4bfae11daa9e37a941430dd1b81aaf472f320835ee2fe744c83f1320882a8a02316ceb375f5c4909232bb2c6520b249c88be4f47b8b86fdd93678c69e64f50089e907a5504fdd43f0cad24aaa9e317ef2ecade3b5c1fd31f3c327d70a0e2d4867e6fe3f26272e8b6a3cce17843e359b82eb7a4cad8c42460179cb6c07fa252efaec428fd5cae5208b298b255109026e21272424ec0c52e1e5f72c5ab06f5d2a05e77c193b647ec948bb844e0c2ef1307f53cb800d4f55523d86038bb9e21099a861b6b9bcc969e5dddbdf7171b37d616381b78c3b22ef66510b2765d9617556b175599879d8558100ad90b830e87ad460a22108baa5ed0f2ba9dfc05167f8ab61fc9f8ae01603f9dd5e66ce1e642b604bca9294b57fb7c0d83f054bacf4454c298a272c44bc718f54605b91e0bfafd772aebaf3828846c93018f98e315708d50be8401eb9a8778dcbd0d6db9370860411b004cd37fbb8b5df87edee7aae949fff34607b",
            .key_param_e = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000073b193",
            .key_param_d = "258f084036b7ffda1d0aa0373a50011dd976b7fd0ee4b889654b044ab241fb754675466909429b1acba9d9c1abf2e9bb494cea81c4ba10dcd1036f36ea81dc24ce983e3ae7da7cf810ddc05c96f9cc3a9046fdf58c9902172c7e53a1bced1b7884f728133be9b4a911023e3159d5f252f407a8080c88f122cf4a9e53f103aecb412cd44d9d53c145757b14eb85a5b0d7f8be88c56bb00e7357d43d6a828953f93124d1b39c0cc137dff2972a402ebfe29eb614c6578e102c61a6001833323d4b79bee101e76a9c59a358471b6225688584fbdd790a1e38a60a5f8bf647f7374680aa1d6cc0372fd12ef233bf6bf726fa4af45e1ead9b58df08f62aa76fe9fd9bb1a975bb1c4ddb9b005453f957dfe4148d2644c1c490877431b67e975c5e02b2dc408de09e531c05c0517311a5cfeb4165b5f44060bb3433fff6ee8f0ad3f559b8458f20cbdca84649f0c8a3b6989f676bc0fe4691032d2a08978f9053abf21c1d081f8ec32735dd1ff0407c3302bf55d167197dbe92c678294d5f1f832da5bb",
            .message = "0897d40e7c0f2dfc07b0c7fddaf5fd8fcc6af9c1fdc17bebb923d59c9fc43bd402ba39738f0f85f23015f75131f9d650a29b55e2fc9d5ddf07bb8df9fa5a80f1e4634e0b4c5155bf148939b1a4ea29e344a66429c850fcde7336dad616f0039378391abcfafe25ca7bb594057af07faf7a322f7fab01e051c63cc51b39af4d23",
            .signature = "8ebed002d4f54de5898a5f2e69d770ed5a5ce1d45ad6dd9ce5f1179d1c46daa4d0394e21a99d803358d9abfd23bb53166394f997b909e675662066324ca1f2b731deba170525c4ee8fa752d2d7f201b10219489f5784e399d916302fd4b7adf88490df876501c46742a93cfb3aaab9602e65d7e60d7c4ceadb7eb67e421d180323a6d38f38b9f999213ebfccc7e04f060fbdb7c210206522b494e199e98c6c24e457f8696644fdcaebc1b9031c818322c29d135e1172fa0fdf7be1007dabcaab4966332e7ea1456b6ce879cd910c9110104fc7d3dcab076f2bd182bb8327a863254570cdf2ab38e0cda31779deaad616e3437ed659d74e5a4e045a70133890b81bc4f24ab6da67a2ee0ce15baba337d091cb5a1c44da690f81145b0252a6549bbb20cd5cc47afec755eb37fed55a9a33d36557424503d805a0a120b76941f4150d89342d7a7fa3a2b08c515e6f68429cf7afd1a3fce0f428351a6f9eda3ab24a7ef591994c21fbf1001f99239e88340f9b359ec72e8a212a1920e6cf993ff848",
            .should_pass = false, /* saltlen is 0 instead of 32 */
    },
    {
            .hash_alg = S2N_HASH_SHA384,
            .key_param_n = "cb59aae30883db678ea7b2a5e7009799066f060757525166030714a25e808482e752f04f78611b3509d6005b411530c9ada4d8fbddc85f9db3d209eccc6cf0cae9aeb902e96688d2547974b7eb3323eeaa2257cf8d5c7c97f5176e2cfd29e19d0487380e3e64338c73bd592d52e9dbcc9606ed5835758c1a112c6a004256b5c4338322695df2ba573be0e79a7b9f9afd497dd38eed39f05f88d7d399d1e984d048067596ad1847ce51221babf51873bad2d81fa91cf3d9fd307e3ebd41fc49a8b3252ed7a71fd2357330bef2f1f89f2c80f740567e2ae8d168e56f007e8fefa33d2eb92b7d830a4f978ffe842ef0697db50602b19642afc50ac1f837e476c0fd",
            .key_param_e = "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024f1bf",
            .key_param_d = "50cc6f7dd99782ce5943d719eccf85ff22ae29e61473dbb80f3b210b16377c51c1d3d65cbaa558db54c58d6683e1aabe03ab4f3802f1cb67eae16787cf5ccf618024c7d8b46a69c73289c7ccf96ff23863f7de4cdb89cfc6b66072184fd61f5fb9b84750d7d63dfdea78e130214a0ba949c0f0a3ddf3ef1395910991590405ff4fad11ad22b159297479cc14f96739e5e63c28610a0ab413823db00f3b3067ec2d4ba17475d439d3b00951146621887ceb845707d5daedd29744ad6e5105159a31f7309e2e39e847be3be0a3a302bef77b8d3979570deaa2713c1221b06d7bff6941e1b59dc1cb88158664be80a9fd7c17b08cfa49f381ad5d186ff5f91fa8c5",
            .message = "f991a40a6c3cda01f1a2fed01ca0cf425588a071205eb997a147fa205f3ec10448090e53f56be512309cf445b3f6764d33f157749d5199c7a09ef6246bd5c793b85d24d9093c4d4b318b48e11727cc8bb7aa5ec8699aba7466e074e1887bdf2a51752ec42f16d956fe5943cbcf9c99a5e89bfd940c9fe447fcf3bc823d98d371",
            .signature = "6b42514e88d93079d158336897dc34b450e424d61f6ddcf86fe8c9a368ae8a22c4ee4084c978b5169379da10ae6a6ae8cd80028e198cd0a8db532cd78a409f53baf7d231b545835b0dc06d594d76868d986889419a959268fd321bbc8bad5e800e452fe1a1a2a5a851d542494473deb425171a2f37ffc4cf0600a8d561b20f777407bbef1b596f92e518c0929e8bd52b015c2718d14443a56056f65015515673deef32ae5399ae71f97873ec1508f8c41d6a66a13017685134e5425c4b580a7f6986c26fb272f0ed215d6698dcec9e7c5258173b295b3611869254a538945de952dedf291837df0d7a205e1b76b01140df4edce3afe7245d46ee3b292bb117b1",
            .should_pass = true,
    },
    {
            .hash_alg = S2N_HASH_SHA384,
            .key_param_n = "8b71c2bcb324a3fc23d292fb4f18cab5140d521013361a07071bc788859cbba33fc226b2cef9c1b3663d307acd3e4d8eb7acff63d048495a2d61fbeb617a42c4f424a347673173902cd1cb11780003e715662d195996fbff55f6b9feb54a18197e6848aa8baa15fa020cc54e72ec976d766ed63ee4e00071a11e29d7baf30e3f",
            .key_param_e = "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000035c661",
            .key_param_d = "3e3e9386d77f5a669da5424826c12e3717df7badde17a69ca195f59251cf5fe7b33c3ed8466b1ca22cfeb016de414e205d0e082a84a2bb75e63daae4d2024e8bb72eb3adf586a9f639c94122c6984c6d246895aa4888c25d26d1490469ed98f963fc45fe7971d4d4bfe996cae28b5ce343c787c6c602f830bc09c55dcff53401",
            .message = "aab88ff728c8f829841a14e56194bbf278d69f88317a81b4749aa5fdbc9383486e09bff96a2c5b5bdf392c4263438aef43334c33170ef4d89a76263cb9745f3fea74e35fbf91f722bb1351b56436cdd2992e61e6266753749611a9b449dce281c600e37251813446c1b16c858cf6ea6424cdc6e9860f07510f7417af925574d5",
            .signature = "657296e902331b8030a72920c6c16b22ea65fe18e7e10b7cdbb8a44ef0f4c66f3e9c22f8f35e4184b420ad3f1bdbc1d6a65e6230abca8a9bee10887833dae15a84bf09a4542389c685fd33e7385c6001b49aa108f2272a46a832bbadc067ff06b09b2f5f40c81cc2acac03311a3945f7a9f2ea81213ba9ba626d6a7ed49f17dd",
            .should_pass = false, /* saltlen is 20 instead of 48 */
    },
};

int main(int argc, char **argv)
{
    BEGIN_TEST();
    EXPECT_SUCCESS(s2n_disable_tls13_in_test());

    /* Load the RSA cert */
    struct s2n_cert_chain_and_key *rsa_cert_chain = NULL;
    EXPECT_SUCCESS(s2n_test_cert_chain_and_key_new(&rsa_cert_chain,
            S2N_RSA_2048_PKCS1_CERT_CHAIN, S2N_RSA_2048_PKCS1_KEY));

    s2n_stack_blob(result, HASH_LENGTH, HASH_LENGTH);

    /* Generate a random blob of data */
    s2n_stack_blob(random_msg, RANDOM_BLOB_SIZE, RANDOM_BLOB_SIZE);
    EXPECT_OK(s2n_get_private_random_data(&random_msg));

    /* If RSA_PSS not supported, cannot sign/verify with PSS */
    {
        struct s2n_pkey rsa_public_key;
        s2n_pkey_type rsa_pkey_type;
        EXPECT_OK(s2n_asn1der_to_public_key_and_type(&rsa_public_key, &rsa_pkey_type, &rsa_cert_chain->cert_chain->head->raw));
        EXPECT_EQUAL(rsa_pkey_type, S2N_PKEY_TYPE_RSA);

        hash_state_new(sign_hash, random_msg);
        hash_state_new(verify_hash, random_msg);

#if defined(S2N_LIBCRYPTO_SUPPORTS_RSA_PSS_SIGNING)
        EXPECT_EQUAL(s2n_is_rsa_pss_signing_supported(), 1);
#else
        EXPECT_EQUAL(s2n_is_rsa_pss_signing_supported(), 0);
#endif

        if (!s2n_is_rsa_pss_signing_supported()) {
            EXPECT_FAILURE_WITH_ERRNO(rsa_public_key.sign(rsa_cert_chain->private_key, S2N_SIGNATURE_RSA_PSS_RSAE, &sign_hash, &result),
                    S2N_ERR_RSA_PSS_NOT_SUPPORTED);
            EXPECT_FAILURE_WITH_ERRNO(rsa_public_key.verify(&rsa_public_key, S2N_SIGNATURE_RSA_PSS_RSAE, &verify_hash, &result),
                    S2N_ERR_RSA_PSS_NOT_SUPPORTED);
        } else {
            EXPECT_SUCCESS(rsa_public_key.sign(rsa_cert_chain->private_key, S2N_SIGNATURE_RSA_PSS_RSAE, &sign_hash, &result));
            EXPECT_SUCCESS(rsa_public_key.verify(&rsa_public_key, S2N_SIGNATURE_RSA_PSS_RSAE, &verify_hash, &result));
        }

        EXPECT_SUCCESS(s2n_pkey_free(&rsa_public_key));
    };

#if RSA_PSS_CERTS_SUPPORTED

    struct s2n_cert_chain_and_key *rsa_pss_cert_chain = NULL;
    EXPECT_SUCCESS(s2n_test_cert_chain_and_key_new(&rsa_pss_cert_chain,
            S2N_RSA_PSS_2048_SHA256_LEAF_CERT, S2N_RSA_PSS_2048_SHA256_LEAF_KEY));

    /* Self-Talk tests */
    {
        struct s2n_pkey rsa_public_key;
        s2n_pkey_type rsa_pkey_type;
        EXPECT_OK(s2n_asn1der_to_public_key_and_type(&rsa_public_key, &rsa_pkey_type, &rsa_cert_chain->cert_chain->head->raw));
        EXPECT_EQUAL(rsa_pkey_type, S2N_PKEY_TYPE_RSA);

        /* Test: RSA cert can sign/verify with PSS */
        {
            hash_state_new(sign_hash, random_msg);
            hash_state_new(verify_hash, random_msg);

            EXPECT_SUCCESS(rsa_public_key.sign(rsa_cert_chain->private_key, S2N_SIGNATURE_RSA_PSS_RSAE, &sign_hash, &result));
            EXPECT_SUCCESS(rsa_public_key.verify(&rsa_public_key, S2N_SIGNATURE_RSA_PSS_RSAE, &verify_hash, &result));
        };

        /* Test: RSA cert can't verify with PSS what it signed with PKCS1v1.5 */
        {
            hash_state_new(sign_hash, random_msg);
            hash_state_new(verify_hash, random_msg);

            EXPECT_SUCCESS(rsa_public_key.sign(rsa_cert_chain->private_key, S2N_SIGNATURE_RSA_PSS_RSAE, &sign_hash, &result));
            EXPECT_FAILURE_WITH_ERRNO(rsa_public_key.verify(&rsa_public_key, S2N_SIGNATURE_RSA, &verify_hash, &result),
                    S2N_ERR_VERIFY_SIGNATURE);
        };

        /* Test: RSA cert can't verify with PKCS1v1.5 what it signed with PSS */
        {
            hash_state_new(sign_hash, random_msg);
            hash_state_new(verify_hash, random_msg);

            EXPECT_SUCCESS(rsa_public_key.sign(rsa_cert_chain->private_key, S2N_SIGNATURE_RSA, &sign_hash, &result));
            EXPECT_FAILURE_WITH_ERRNO(rsa_public_key.verify(&rsa_public_key, S2N_SIGNATURE_RSA_PSS_RSAE, &verify_hash, &result),
                    S2N_ERR_VERIFY_SIGNATURE);
        };

        /* Test: If they share the same RSA key,
         * an RSA cert and an RSA_PSS cert are equivalent for PSS signatures. */
        {
            DEFER_CLEANUP(struct s2n_pkey rsa_public_key_as_pss = { 0 }, s2n_pkey_free);
            s2n_pkey_type rsa_public_key_as_pss_type = S2N_PKEY_TYPE_UNKNOWN;
            EXPECT_OK(s2n_asn1der_to_public_key_and_type(&rsa_public_key_as_pss, &rsa_public_key_as_pss_type,
                    &rsa_cert_chain->cert_chain->head->raw));
            EXPECT_EQUAL(rsa_public_key_as_pss_type, S2N_PKEY_TYPE_RSA);

            DEFER_CLEANUP(struct s2n_pkey rsa_pss_public_key = { 0 }, s2n_pkey_free);
            s2n_pkey_type rsa_pss_pkey_type_shared = S2N_PKEY_TYPE_UNKNOWN;
            EXPECT_OK(s2n_asn1der_to_public_key_and_type(&rsa_pss_public_key, &rsa_pss_pkey_type_shared,
                    &rsa_pss_cert_chain->cert_chain->head->raw));
            EXPECT_EQUAL(rsa_pss_pkey_type_shared, S2N_PKEY_TYPE_RSA_PSS);

            /* Set the keys equal.
             *
             * When EVP signing APIs are enabled, s2n-tls validates the signature algorithm type
             * against the EVP pkey type, so the EVP pkey type must be RSA_PSS in order to use the
             * RSA_PSS signature algorithm. However, EVP_PKEY_set1_RSA sets the EVP pkey type to
             * RSA, even if the EVP pkey type was RSA_PSS (there is no EVP_PKEY_set1_RSA_PSS API).
             *
             * To ensure that the RSA_PSS EVP pkey type remains set to RSA_PSS, the RSA key is
             * overridden instead of the RSA_PSS key, since its pkey type is RSA anyway.
             */
            RSA *rsa_key_copy = EVP_PKEY_get1_RSA(rsa_pss_public_key.pkey);
            POSIX_GUARD_OSSL(EVP_PKEY_set1_RSA(rsa_public_key_as_pss.pkey, rsa_key_copy), S2N_ERR_KEY_INIT);
            RSA_free(rsa_key_copy);

            /* RSA signed with PSS, RSA_PSS verified with PSS */
            {
                hash_state_new(sign_hash, random_msg);
                hash_state_new(verify_hash, random_msg);

                EXPECT_SUCCESS(rsa_public_key_as_pss.sign(rsa_pss_cert_chain->private_key, S2N_SIGNATURE_RSA_PSS_RSAE,
                        &sign_hash, &result));
                EXPECT_SUCCESS(rsa_pss_public_key.verify(&rsa_pss_public_key, S2N_SIGNATURE_RSA_PSS_PSS,
                        &verify_hash, &result));
            };

            /* RSA_PSS signed with PSS, RSA verified with PSS */
            {
                hash_state_new(sign_hash, random_msg);
                hash_state_new(verify_hash, random_msg);

                EXPECT_SUCCESS(rsa_pss_public_key.sign(rsa_pss_cert_chain->private_key, S2N_SIGNATURE_RSA_PSS_PSS,
                        &sign_hash, &result));
                EXPECT_SUCCESS(rsa_public_key_as_pss.verify(&rsa_public_key_as_pss, S2N_SIGNATURE_RSA_PSS_RSAE,
                        &verify_hash, &result));
            };
        };

        EXPECT_SUCCESS(s2n_pkey_free(&rsa_public_key));
    };

    /* Test: NIST test vectors */
    for (size_t i = 0; i < s2n_array_len(test_cases); i++) {
        struct s2n_rsa_pss_test_case test_case = test_cases[i];

        struct s2n_pkey rsa_public_key = { 0 };
        s2n_pkey_type rsa_pkey_type = 0;
        EXPECT_OK(s2n_asn1der_to_public_key_and_type(&rsa_public_key, &rsa_pkey_type,
                &rsa_cert_chain->cert_chain->head->raw));
        EXPECT_EQUAL(rsa_pkey_type, S2N_PKEY_TYPE_RSA);

        /* Modify the rsa_public_key for each test_case. */
        RSA *rsa_key_copy = EVP_PKEY_get1_RSA(rsa_public_key.pkey);
        BIGNUM *n = BN_new(), *e = BN_new(), *d = BN_new();
        EXPECT_SUCCESS(BN_hex2bn(&n, test_case.key_param_n));
        EXPECT_SUCCESS(BN_hex2bn(&e, test_case.key_param_e));
        EXPECT_SUCCESS(BN_hex2bn(&d, test_case.key_param_d));
        EXPECT_SUCCESS(RSA_set0_key(rsa_key_copy, n, e, d));
        POSIX_GUARD_OSSL(EVP_PKEY_set1_RSA(rsa_public_key.pkey, rsa_key_copy), S2N_ERR_KEY_INIT);
        RSA_free(rsa_key_copy);

        struct s2n_stuffer message_stuffer = { 0 }, signature_stuffer = { 0 };
        POSIX_GUARD_RESULT(s2n_stuffer_alloc_from_hex(&message_stuffer, test_case.message));
        POSIX_GUARD_RESULT(s2n_stuffer_alloc_from_hex(&signature_stuffer, test_case.signature));
        hash_state_for_alg_new(verify_hash, test_case.hash_alg, message_stuffer.blob);

        int ret_val = rsa_public_key.verify(&rsa_public_key, S2N_SIGNATURE_RSA_PSS_RSAE,
                &verify_hash, &signature_stuffer.blob);
        if (test_case.should_pass) {
            EXPECT_SUCCESS(ret_val);
        } else {
            EXPECT_FAILURE_WITH_ERRNO(ret_val, S2N_ERR_VERIFY_SIGNATURE);
        }

        EXPECT_SUCCESS(s2n_stuffer_free(&message_stuffer));
        EXPECT_SUCCESS(s2n_stuffer_free(&signature_stuffer));
        EXPECT_SUCCESS(s2n_pkey_free(&rsa_public_key));
    }

    EXPECT_SUCCESS(s2n_cert_chain_and_key_free(rsa_pss_cert_chain));
#endif

    EXPECT_SUCCESS(s2n_cert_chain_and_key_free(rsa_cert_chain));
    END_TEST();
}
