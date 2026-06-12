# Secasy Test Vectors

Official reference test vectors for the Secasy hash function.  
Any correct implementation must produce identical output for each input.

**Rounds:** 10 · **Encoding:** lowercase hexadecimal · **Date:** 2026-06-12  
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
|  512 | `""` *(empty)*                                  | `d73b7839a2ecfe6c6df0b0069a815ef41bb905f88aed985a613358c5ac206a9158fa80be81c2ff3de3151bcd9ab342ef3e75cd19a9e4039c666767dcd6291e0c` |
|  512 | `"a"`                                           | `4c77343de76566edb46f421121000ca26bc544c0747efe19be85e657751be0215aeb98ea1f5c8bda6a82c6afa1b100de90819969a2585d167f69cc6978a31014` |
|  512 | `"z"`                                           | `e669f8288dea3dd257d8fc6de657ed3bd38739fe30d03d080beffc4477c1572964e06fbd0d949fd4c270bd028ea7ff1dc65f2b6740feef50345b119283b06214` |
|  512 | `"0"`                                           | `be46e07163a6428db693a1e1ca1cfbe768d1f3b5781e29265bd1bed91ca30418129d0c881ef7903d68e025ddc2123f05ca7825f7e4f46b207200eaafc7388792` |
|  512 | `"abc"`                                         | `564c6b22c9e268b3b37569bb1098bf36bf3eba09f454f8f737dfd23e344b737424ca3fb4aa0c287cc969e4ce22481cbbff899de4f16093821985897401c23890` |
|  512 | `"secasy"`                                      | `d7b8ed372c5e4fecc94a2b7e424eb028b3019141d5a138267cd4b5ab8595e81a94b6cd3c4858c5a73db8751442b57b6c15f8ac684bc31152dd20a329ad589eb5` |
|  512 | `"1234567890"`                                  | `6072c026c17c78aeead275ebb55687d9088a686dfaa842f579b6d4312e6b25bd9d48ab4b62dfc086285eea1409d660e8f226bdc8fffbe9b3aad2fb5369698d3b` |
|  512 | `"The quick brown fox jumps over the lazy dog"` | `cbeee8a69e91d7afef0c9655d087621e451eb91da2f85b18c3801d9f3c2ba478b029f49dfeb760b875629fc3bdbfbde78ed8254c6470fb0ae83aa6065c3c54d4` |
|  512 | `"The quick brown fox jumps over the lazy cog"` | `da6a50e70f103d1a2db6aec270bad2d3a2cd38b540514bf457220399427e6bfade37d73be9f2bd1978e2b68a5a6170ce84aa3b75d0d603c3051755ef41157a11` |
|  512 | `0x00` *(1 byte)*                               | `2e09d8db37a724cd02c9f25b6c734f0460f7c8f855e0469218b88a9e12d214beb1885d2e5974e68a252a8f8fe0e1a9260866e8fa1d9e51992c09ceedb6543865` |
|  512 | `0x01` *(1 byte)*                               | `ebbf338c4cdc519233d5c59ce2612a01f6e0397915e68c9ed9999736870e9389a2442f44104680a6e2421245ea181aa3c37ab0633def41acc2c2ed38b4f3b79c` |
|  512 | `0xFF` *(1 byte)*                               | `355e6d2f997d3be9bd63d8fdb6c9b86c71498bdf5602f23dbd83301e6257357e3f1758f0d8eacdf7dbaf17a632a49c0390d6eeff27a0b65bcbdd3ed0d89e6201` |
|  512 | `0x80` *(1 byte)*                               | `3d3b80fd91e621a21f12a7a960732a806a182180e8f002675a2c60b7f4ef9204d128fb253037f6cedd6989473e558d4896b33864c48a1ef434b056af5f522d8c` |
|  512 | `0x00 × 8`                                      | `280f44fe34035d6198369dc8772f72627e956e111ded7e4be33eb55ad8a8cc2a8051aa64d10f4bf63a273c88ab55b150811998ddf7730c73d862dc337986aef8` |
|  512 | `0x00 × 16`                                     | `8ed2fd24de925794ee76ccf6974e090989a073427167d29dad159687cf412ae528c2c792f6c6e903088ff562b04bd5aea940c9f4b56608036ba37792aad48e68` |
|  512 | `0x00 × 32`                                     | `8c2e115a198b12a0b3169c57c5bf0032fa82a2c46a3808d2892f7b4771db7163ce5313d3aee5285c9ad0c1abcb3c3afeb9a328e7be193958a0e95a6a8812c265` |
|  512 | `0x00 × 64`                                     | `7377f0ce60fecd3e50bf23dcc454b288fd21a828bca35ecf94e00de6b5ec34f4ea4b14d04cb0a7fededbfaa37328de3cc6fb8af54bb966d227a148e88970c58e` |
|  512 | `0xFF × 8`                                      | `106ca454d8c2f2fac5e17d9742d03fc432381db51e042fecc281e72cc70f7b188254ea9cced3b75405d5611bf03e16980d0b786a8c3e8eac40de393e59c32976` |
|  512 | `0xFF × 64`                                     | `2fa17ae621769abc28abd21c6d6436f2cbdd5df2d750d92b6db3e96c4a71beb45f62ba6cb5897880c250be5094504d11a76f92650f45d095f688a4c0e1ec5a69` |
|  512 | `0x00..0xFF` *(256 bytes)*                      | `6ca80337954a02a574461b421b13c8da49aead1f4884ccd2b3dd5c09589d85f506f585a7889956e562edb360574ca041ad64c0df2ca71a48b23d56d184817e95` |
|  512 | `"Markus"`                                      | `1905540d363075eab5159a7a7c3ffb859054383be7f08d6f326c682d27071a03f2b483b47bf276cbf9b30956bb446322ba7362b2de7b80acc17196f19866ffd4` |
|  512 | `"Anna"`                                        | `cb50e2f929b25e3bd313e65b34c6422126295f05a6b8d1523a95051c1661d35aaea58b110ffc723b102f51b7afd17c7b28952bb8582a1e5caeba138a63edc7e4` |
|  512 | `'a' × 55`                                      | `f71eaea41b91151350956c407c2906e3f79eb25927f722432f13c1192ddff965cb38114d1f9d68816dd612676330022b07cbc5c939e07840766be08b1166ab17` |
|  512 | `'a' × 56`                                      | `c27a90eab4c581d62e2e93b6d58b0dae59afb3459582567311c68b89c6c7ada6123cd398d7d10863e771afd4aa686353100b39eb5f46142e18ad0225e57f7cac` |
|  512 | `'a' × 64`                                      | `d677f7010319ac361aa04d0826ca5ad8be80246f61fab6c51e0e428f67f9584027ae74d5844777a2b741f72d3b30c830a61451d5a7a1fc2067d71489cd5520fc` |
|  512 | `0x00 × 1024`                                   | `341449fca1a6debe0ccbcfea7adf18f9dcbc2a77dcedd724cbed83f186f90459e2142c2943be438f02de15669c8d9e7c557ed84cc17f0969c9ce12b306957b61` |
|  512 | `0xAA55 × 512` *(1024 bytes)*                   | `635a4ae1ebf9dde24264ff8a3313bfc48876b281fa757dfcba3d1b2edcd4fad39c5ae4c0eedbfd855f49a2dbdb232fe9272e43eb30f1341442775350e90cd78c` |

---

## 64-Bit Vectors

| Bits | Input                                           | Hash (Secasy)      |
|-----:|-------------------------------------------------|--------------------|
|   64 | `""` *(empty)*                                  | `d73b7839a2ecfe6c` |
|   64 | `"a"`                                           | `4c77343de76566ed` |
|   64 | `"abc"`                                         | `564c6b22c9e268b3` |
|   64 | `"secasy"`                                      | `d7b8ed372c5e4fec` |
|   64 | `"The quick brown fox jumps over the lazy dog"` | `cbeee8a69e91d7af` |
|   64 | `"The quick brown fox jumps over the lazy cog"` | `da6a50e70f103d1a` |
|   64 | `"Markus"`                                      | `1905540d363075ea` |
|   64 | `"Anna"`                                        | `cb50e2f929b25e3b` |

