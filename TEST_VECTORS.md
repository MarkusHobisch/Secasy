# Secasy Test Vectors

Official reference test vectors for the Secasy hash function.  
Any correct implementation must produce identical output for each input.

**Rounds:** 10 · **Encoding:** lowercase hexadecimal · **Date:** 2026-03-15  
**Generator:** `tests/testvectors/generate_test_vectors.c` (build target: `SecasyTestVectors`)

---

## How to Use

Instead of comparing hashes manually, run the automated verifier — it checks all vectors and reports PASS/FAIL:

```
cmake --build build --target SecasyVerifyVectors
./build/SecasyVerifyVectors
```

Expected output: `Results: 36/36 PASSED  — ALL VECTORS VERIFIED`

Alternatively, regenerate the raw output with:

```
cmake --build build --target SecasyTestVectors
./build/SecasyTestVectors
```

All 27 vectors must match exactly — any deviation indicates an implementation error.

---

## Reference Table

| Bits | Input                                           | Hash (Secasy)                                                                                                                      |
|-----:|-------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------|
|  512 | `""` *(empty)*                                  | `170cc4dcf0d6f18211e1b34dab26e019c2bd077cb542f53d717544b3b8151f622489f9c95b5ac055c7b13991597469ad62c84674564a659cf6e2298a794cb8a5` |
|  512 | `"a"`                                           | `3a29643d127dc5db52e87165c6a6354f18e21f7af3ca01df6fa3e7a75aebae6d55b4836e98fdec67436705447af36e098df252cf471f4d21acfd939cd200a1fd` |
|  512 | `"z"`                                           | `9be4960d677a2db545422ba467ba6368d96d1cd7707a2fa5c9298f28707b341bce96fe38559af0ac7e6eb37edca080667e2ad87aa5f44d76f6d76333889b5f6c` |
|  512 | `"0"`                                           | `4bc2e5fd617e6e0ddd65c39f3a8175786d8b2734fbf841f50e04217e34b20fe5f8f3bae55f8236192a0e649f9cf75555f07f0b0b0429458e48ec33c5a1b8f1a5` |
|  512 | `"abc"`                                         | `0daae080dab87f0b766d974697bb5f1151c56afacb903c131e0445ecef9ee0b0bbf7987798aad36e7134185c37603600ee60f8451c28ba0789be55d09e40a317` |
|  512 | `"secasy"`                                      | `ea791c8897d5abffaa2f570f12b34db890d4fb75559cc233079973c8274ce864450cc6116877186e7382824df50129f79dc1d379843e77995b3e23db67dc88ce` |
|  512 | `"1234567890"`                                  | `2d6ff2cfacadb4a42d0ce9dddcd8d714a05cefcb09d54225a9bc1c8931ee9512fb161dd6b314ab6af1672e4d3d9fc5c70bd869be03850682a0a3ec5e44afd5f6` |
|  512 | `"The quick brown fox jumps over the lazy dog"` | `7fa315ffc925efc53357165eb79151fbc823985da4bce8a70692ef885f0fc486ea459f74ef3a333b8c7dea3d2f3c9a6f0ea3d036418e2cc61624c17c47779bb3` |
|  512 | `"The quick brown fox jumps over the lazy cog"` | `fa64f95e00bd8d57a4708a8cafccacc70a4eb53f2a3f54a9fee955a8057d3cee734cfb672a407d18021933f2ab30a634c1d08e2044c9d31cc67376c4fdd3a707` |
|  512 | `0x00` *(1 byte)*                               | `7934c6203539b4a56ed85db787b694d87daab5011975984b405ef3e21ab4f84bf922d9c8b80ea3a7ada32629b27f8485fa2fd504ebdf24741cba942b4f83a1b8` |
|  512 | `0x01` *(1 byte)*                               | `88c0b9c2ca25b6529866ad1c7986278058d3291a6c16d94660bdc78ea74fa9fbdc9e2319e964e35713b3a3700cb2ae61b6a5d237a838729f02a89e97dfa967a2` |
|  512 | `0xFF` *(1 byte)*                               | `d64809ba27354d28625c661f266678c2dc41b3d6760b7bd24cf959de641827390a90276ecf16e93985240a5b4e301a05f6ec3c1ebddde24fff2855f1148b09ea` |
|  512 | `0x80` *(1 byte)*                               | `dc509a98affade882bb4eaa992b45cea638bde98e501a160bc2378a8875b4cfe24b43ddd94c0f98c9fafb44c79423477fc788cb9a4e4cf93290f0eccf2f6a666` |
|  512 | `0x00 × 8`                                      | `923012917656c8e282cd3882d27c0c11cb838ed96a5ca68281b9e1f5f111211eb5a7f5c25b12846db3b0d0f416a07c7ba2bd035d78357b7912e0beac3001d4b6` |
|  512 | `0x00 × 16`                                     | `3ca9788634bb23e263ecc1fdf96018e22d47c28828120cda502ea565698102f9d2fcf0f479b7b4ba4e5121dce4a63f8bbe54f6771669499fc103424903a0cbd1` |
|  512 | `0x00 × 32`                                     | `1602a34e3618147423f428efe9a1d65a594aadcab6127792454677caa983611cc7b3c2cac9563df0f6681ddf4f9b8df7d4c35e4f184e7e71b8a18d013a1573d4` |
|  512 | `0x00 × 64`                                     | `b40271aaf0d6b5ba942ed60aecd321da93a58024b59249bb70de600b2b3fb425fea4b76970cd2c550685cf2f55d1d989aef90fd65c18adabf38ad22eff554a36` |
|  512 | `0xFF × 8`                                      | `3b266c7b69db521bf15dd55b98114e54d8b6ec0ddf0dd6dc4fb5071fbf3ee92cf6dfd0bee334929a7d4151801a57f49f04e19b9b30cae47e35aa5daa0b7d90ad` |
|  512 | `0xFF × 64`                                     | `d333e721642c90a0e512d4d8ed4bf6254b53d4a90f3ee116c5390980511d1b93031a5b7de056e4eb708e7b30702608d600c0992e7f1d24fe40056ae9bddce301` |
|  512 | `0x00..0xFF` *(256 bytes)*                      | `43a38ad54ac17ab2519565c42292fd8db751ec74c982b495cf01b57707109d37c12d132a5dae805ba5e4ccd531f8e7b06bb528ab6e61512b4f19b77036d141f4` |
|  512 | `"Markus"`                                      | `fe638bdac5e04f1bc59f1b92b2d166e7019c9e3602bb4a4874dd1d8d9e2ecf9519a7d4f172427b8d763193ebce54af48d2b6bb86a68fa45916b39a738d78c610` |
|  512 | `"Anna"`                                        | `fa8c9eeda7c7dc8d8baf9685d7607f435d96434105e88b4030366cea3fbbacdd994fb75bf221a74687744ccfb13c4482bb8a75d1b53883e9a7e0bdf836381e9a` |
|  512 | `'a' × 55`                                      | `3148370a787a836234923a789a9c144d17ba36ab5fd274b389611e728dbfd6195a393797e77b5f94685cca2b3b30ef48da73c0f006c307efa55555c3d2dd2dac` |
|  512 | `'a' × 56`                                      | `3d6b4720d605f1851cafc8c065a35512c57198ad3bdea71834c99343aaea3ce3f38cd973a7f99bd7101974e92681a8537f3c243805300287b87d74b2b67741bd` |
|  512 | `'a' × 64`                                      | `bea7a40ddbcc37ddc0174bc91eb8a739613fa601ef23c7e4dcf95fb260498fa204f8b87ce1551dceb07a989c32b5ab4d683d604210e3253b3dc56b19af5b9c1e` |
|  512 | `0x00 × 1024`                                   | `45f9ce95b3cb8e9d072ee3d352280279c8043d024b9d8586687f37e42a11ff6e944ead494f835d3c917ad43f0e9600083637b5645f597d44c288fb8ef4db9820` |
|  512 | `0xAA55 × 512` *(1024 bytes)*                   | `a25513c1d00aa98d79dd7a70242228c653b7be9ce3fd3eab355eb05744451eb0d10ca5205f535b474e52567f0aa326073c42bcb670629d1e67f9d66f8c2bb96c` |

---

## 64-Bit Vectors

| Bits | Input                                           | Hash (Secasy)      |
|-----:|-------------------------------------------------|--------------------|
|   64 | `""` *(empty)*                                  | `170cc4dcf0d6f182` |
|   64 | `"a"`                                           | `3a29643d127dc5db` |
|   64 | `"abc"`                                         | `0daae080dab87f0b` |
|   64 | `"secasy"`                                      | `ea791c8897d5abff` |
|   64 | `"The quick brown fox jumps over the lazy dog"` | `7fa315ffc925efc5` |
|   64 | `"The quick brown fox jumps over the lazy cog"` | `fa64f95e00bd8d57` |
|   64 | `"Markus"`                                      | `fe638bdac5e04f1b` |
|   64 | `"Anna"`                                        | `fa8c9eeda7c7dc8d` |

