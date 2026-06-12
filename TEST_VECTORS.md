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
|  512 | `""` *(empty)*                                  | `0223add84c1344216d1618cab705af13d80883bd21f81a0542faeeaf8cea84f8aded59a1f7dcefea18dfc49462cf5add83d22f86cdc1c5cfeec49a7938b430c1` |
|  512 | `"a"`                                           | `55d3c40bef7b48854a3645bf74344fd23e98c772f8ed571f32fb49267da65e6c275dcada025f65b91bc04c8d87186d061022ce410bd1745304854ff4908a7ba0` |
|  512 | `"z"`                                           | `9337bc9dff2a3b53d500650fa34d47fa16c90d81477054a25891b5f2eb9361499a5a5e648fb66df0dc2306d633d97a971debaf47d7fc873f5fb457b97c1f93e6` |
|  512 | `"0"`                                           | `d02380dba32fff80466044eaede83133bc9d08fa38a062e532d9cd0983589498a9169118ce10c64a1f53552818c8f7fd95901937638129af0bccdd46ae395b62` |
|  512 | `"abc"`                                         | `cbbd5e397871b0c4d686decf25866a6be1505f64d29b2412ec19dffa7fafddb9f6e360902cc4976001ace125d9d951080c7661bb86ee0aaf173fe2513402c456` |
|  512 | `"secasy"`                                      | `2cd0dab047357f778d77c074c5018335ee1ea63942cd86f34ec58bfdc0998ab2af6c71c23e658e7010135786bc31922f70ba3d4b39fd95edd161230fb7c999ab` |
|  512 | `"1234567890"`                                  | `3d1f134ab491d3157603c86d3da652e3aee87d8fc6bad2b1e7cd32b24fcf527f20b1e7d4d8e3d24e59969cf761f8521c927b5219eb0cd1eacb60073c742151b8` |
|  512 | `"The quick brown fox jumps over the lazy dog"` | `504403abb1f3641b2f60a4846923b23690704070345e34ee6cedd844ed866b114b8d770736b49b5eca9a0feef7e6e8188959aac1b1110be6e9e8478c7a461e52` |
|  512 | `"The quick brown fox jumps over the lazy cog"` | `bdb65ed6fbffe32d00d92b1206de53e5a093f7d00be7028fc7dbac52d720535f0a4791cc0c92a38fdebf5e2e8392f42e6e542a6bc32321fb3940772f43f39289` |
|  512 | `0x00` *(1 byte)*                               | `96ee9ae4b56de493aac84bb77b5b7303bea1fc8a41490173d27bad5d07368fe3e6555e2fcd241e53fa2f0f029311acc30e08bfd558ff3b3421e270a81eecc9a4` |
|  512 | `0x01` *(1 byte)*                               | `a16c91469e8355001cc4bda936bfec59981cea0bcefc83b11375166e67391b0a8ecd42d0ff75b2620a256f3397b249bb857d9b962feee11300d5c7f8c82b786c` |
|  512 | `0xFF` *(1 byte)*                               | `6d320a4d99b92beb21fdac87b3dc9f2bd6c94ec1ce00126a8b94f0fbe82385aa406093360246f8eaf52c35701c6a6c29a9f7d7aa368ddf695ec379e450b152a9` |
|  512 | `0x80` *(1 byte)*                               | `0f7c54d8114fcbf3f758447ae97e62d4df34341dc1acf9b6c71023c099db9098aeec1363720a277a96c803064a38be5c7ea3f2a92267553e667fe24bfa95ec20` |
|  512 | `0x00 × 8`                                      | `d1de76cc9419fe2bdef0bdec2ade68abec03050bc1a2d32bf9154c2b58673dab0627934aef2ba82c1339da6a85f012ac204c218a1cb47d2c2d5e68a9b378e7ac` |
|  512 | `0x00 × 16`                                     | `8ac44b90e0011334dcf74f98dfdfa0d99775547e4c663bbd9a4fb1f37b239770112b8ee051203b0534bd68012930d53ba6e6f7a2131218e75ba2b188f19ed3ca` |
|  512 | `0x00 × 32`                                     | `e8b2cc38a22642f5e390a431029140d0de6e7c2962fc3eabd94c5421c3673c86d42a2c1a23d23a61cf080412843d383cc9e5dc0ae4a83617c4c3b403451333f2` |
|  512 | `0x00 × 64`                                     | `cd3486996fec845b4aef64cb3395dc572af1e9e610371051829e9d979232a360c3cba4aa122e3330f46abc1fed04e20270cebabee6655b5522a37747f276001a` |
|  512 | `0xFF × 8`                                      | `146e1c6bbcd1b9c835eaa0c9d9d9415e810d58fb89d9a35b4a806e2cf6f44d97dfba3d31745cb68eeb26cb2eefd02c845cb2e63da4f6271214b6e27130432636` |
|  512 | `0xFF × 64`                                     | `2f53ca6e1f74e1c27ec3f950d93e9d4cce342833930858d61da457164cd214616d1485f9069bcfebbc84b4dbc0658b750bf4e3be7a2f47005b6512a133f9028a` |
|  512 | `0x00..0xFF` *(256 bytes)*                      | `c6ebd8334d829a96ee96fb381734edcafa78e113b04124d71f7d7b48d5fa82fd1826059af09148fd278437ebea56b03cf9f7782f5cd22e59d0f25bad29146aa2` |
|  512 | `"Markus"`                                      | `c38c743806b043e367b72411f32586ca0be1d3ebdf9ac9b1b00c83c5cc100c975437339fb8854f7ef861e379a4fa92649c8c9353916fd54b40b7432d7de51832` |
|  512 | `"Anna"`                                        | `5fc38f965be81ec446d2af539f98253e2de1cf10e3482bb814f0eece26f83232fc000e8b6aa838abe30f2e48ae583f25ca1e4e05f208459fb12d6dc335b84c19` |
|  512 | `'a' × 55`                                      | `d27e67570be770c1ca71606abb90475ac264597e6b391df3ba5752921ae1f48cb24a4ba5ca8acb25aa3d44b97a33a1bea2303dcd29dc78579a2336e0d9854ef0` |
|  512 | `'a' × 56`                                      | `8ecec0f084075f029dd7bc946ebaba4413661235de722c6f9e1f06dad329a8f8b2bff27c41e114225d78dc21c19a778cbef248c31159e2bde1c31d67051165ff` |
|  512 | `'a' × 64`                                      | `294cd2b86b80db61a44c5b57a4284c575287a031f5faf836c06f08f940807c4e5c4315ad6fff94b379bafe7cc00de2c788f287c30b5c0d175619506e423aedbc` |
|  512 | `0x00 × 1024`                                   | `54a5812945342cdc72956ec6681f959189d8bb9133bbfcda5896e35536d40b2118e1f79049d4b37770e83934f90901bd602d1c69af4874dd1fdfbd40d8cc7d86` |
|  512 | `0xAA55 × 512` *(1024 bytes)*                   | `9b5d656c8f73fee79e1a49ad1767cc491c62b8ba3e9d51bf25b5b3194574d679a6da747eb42b205f9abf703f636f6ac6b17a30af7c953172b4c7b2b08b8bbcab` |

---

## 64-Bit Vectors

| Bits | Input                                           | Hash (Secasy)      |
|-----:|-------------------------------------------------|--------------------|
|   64 | `""` *(empty)*                                  | `0223add84c134421` |
|   64 | `"a"`                                           | `55d3c40bef7b4885` |
|   64 | `"abc"`                                         | `cbbd5e397871b0c4` |
|   64 | `"secasy"`                                      | `2cd0dab047357f77` |
|   64 | `"The quick brown fox jumps over the lazy dog"` | `504403abb1f3641b` |
|   64 | `"The quick brown fox jumps over the lazy cog"` | `bdb65ed6fbffe32d` |
|   64 | `"Markus"`                                      | `c38c743806b043e3` |
|   64 | `"Anna"`                                        | `bd3b6c745f38b580` |

