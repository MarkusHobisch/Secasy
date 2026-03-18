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
|  512 | `"The quick brown fox jumps over the lazy dog"` | `29982929a9a81356bacd93d4469b4becba57254db71c55ad362a2f47a7454df6198bcc6b308d62eace54403ad9842a17d3cb4a18527e74de328bc763d4172105` |
|  512 | `"The quick brown fox jumps over the lazy cog"` | `5e7fac4bdfc5b792e33546dcd0b825cc657ac4a329a5529837790fe1728867142f3d8130c52c66f1c19307bd6a25a72451a19e341edb26bbe16ee3914e910239` |
|  512 | `0x00` *(1 byte)*                               | `7934c6203539b4a56ed85db787b694d87daab5011975984b405ef3e21ab4f84bf922d9c8b80ea3a7ada32629b27f8485fa2fd504ebdf24741cba942b4f83a1b8` |
|  512 | `0x01` *(1 byte)*                               | `88c0b9c2ca25b6529866ad1c7986278058d3291a6c16d94660bdc78ea74fa9fbdc9e2319e964e35713b3a3700cb2ae61b6a5d237a838729f02a89e97dfa967a2` |
|  512 | `0xFF` *(1 byte)*                               | `d64809ba27354d28625c661f266678c2dc41b3d6760b7bd24cf959de641827390a90276ecf16e93985240a5b4e301a05f6ec3c1ebddde24fff2855f1148b09ea` |
|  512 | `0x80` *(1 byte)*                               | `dc509a98affade882bb4eaa992b45cea638bde98e501a160bc2378a8875b4cfe24b43ddd94c0f98c9fafb44c79423477fc788cb9a4e4cf93290f0eccf2f6a666` |
|  512 | `0x00 × 8`                                      | `8912d0a0bf063521b47492589b38846f88f75efe2eec62c4eb8f1bc1598e98ec1c597fb63a4b015b180bc93badbb1cba76081dcc893716fa99d7f60600ef3f87` |
|  512 | `0x00 × 16`                                     | `0e2bead304e4fc4e061ab0aedf7dcd73d07be927e64ccbf63d97a7f0b645b01c956adf4483c0e50eb9dbe7decc8517c57085b01150fa03c16a6f0362b19f70f5` |
|  512 | `0x00 × 32`                                     | `be12bc1f46521285ff22aa2a54ba8084875c0c2c9f2e6a5093e14bd9bdf3c2d21ea9661fea1c6a0f9ae270caca56de833483440e4588d8e27a83d581920b4199` |
|  512 | `0x00 × 64`                                     | `b2807173ae08fcbcf5ae5f601a9b8bda8bf21576bbfc692aa7961f9993ce60cfa98db2ed4a94a143df1eb3e7e4a91f4fd09da51b460099016035e7340b4fb224` |
|  512 | `0xFF × 8`                                      | `e54c726c1ac815eceef213d88b916229abb848962296e21676021ed47ab69de8eff7675e95ec2a1a32567cd1a4a4d4c448b61df289db6f05895b18e378268114` |
|  512 | `0xFF × 64`                                     | `d333e721642c90a0e512d4d8ed4bf6254b53d4a90f3ee116c5390980511d1b93031a5b7de056e4eb708e7b30702608d600c0992e7f1d24fe40056ae9bddce301` |
|  512 | `0x00..0xFF` *(256 bytes)*                      | `2c1aaf15839379413a05ace1387c368488ecf9261d7a50dce698292406bf9a040bbdd290a6b3f1719e626c77b8dd313882343911a2bb13885cd647ebaa9a9fdd` |
|  512 | `"Markus"`                                      | `fe638bdac5e04f1bc59f1b92b2d166e7019c9e3602bb4a4874dd1d8d9e2ecf9519a7d4f172427b8d763193ebce54af48d2b6bb86a68fa45916b39a738d78c610` |
|  512 | `"Anna"`                                        | `fa8c9eeda7c7dc8d8baf9685d7607f435d96434105e88b4030366cea3fbbacdd994fb75bf221a74687744ccfb13c4482bb8a75d1b53883e9a7e0bdf836381e9a` |
|  512 | `'a' × 55`                                      | `0e9aaed994730971d55f22c898b9ae1056b65e46a29952af7fa562aa5be1d15aa24bfa8f5d1dcacfeb2d198f5d779241af2bb48665a521f229493a55c9af58ba` |
|  512 | `'a' × 56`                                      | `f43e410d1a2475cc1f6162f82beab9c918dbfe8272c7a7423aa00108f65fdc53f02e1b9d58c1deb047279ccb8661e52a31dc2874b65bb91e3c0f89503bd42a72` |
|  512 | `'a' × 64`                                      | `0a2fe8f881f24a47d432f58a78498bb322c734ec7061298d2789e58e6fef9eda53cff909b22776011a932bd47da8b666dd2655587d4a90dad077407d58501436` |
|  512 | `0x00 × 1024`                                   | `d25c0c131643968dc344e995ead28844939094e5706b42dab4c1d4ad5684897a0bb617ea02545dc64e36298924b9b6db338e6fb7847d87d3b21ab0c4c26435f4` |
|  512 | `0xAA55 × 512` *(1024 bytes)*                   | `3a46179f9f0cac29344ca14723815a30ab330ba57dd7b001645dcec0c767ef1000497adc0a317b100ee7dda72a9a9e25a7bc38ee01b5aff0d03883f7b8cab1c0` |

---

## 64-Bit Vectors

| Bits | Input                                           | Hash (Secasy)      |
|-----:|-------------------------------------------------|--------------------|
|   64 | `""` *(empty)*                                  | `170cc4dcf0d6f182` |
|   64 | `"a"`                                           | `3a29643d127dc5db` |
|   64 | `"abc"`                                         | `0daae080dab87f0b` |
|   64 | `"secasy"`                                      | `ea791c8897d5abff` |
|   64 | `"The quick brown fox jumps over the lazy dog"` | `29982929a9a81356` |
|   64 | `"The quick brown fox jumps over the lazy cog"` | `5e7fac4bdfc5b792` |
|   64 | `"Markus"`                                      | `fe638bdac5e04f1b` |
|   64 | `"Anna"`                                        | `fa8c9eeda7c7dc8d` |

