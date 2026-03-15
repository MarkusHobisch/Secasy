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

Expected output: `Results: 28/28 PASSED  — ALL VECTORS VERIFIED`

Alternatively, regenerate the raw output with:

```
cmake --build build --target SecasyTestVectors
./build/SecasyTestVectors
```

All 35 vectors must match exactly — any deviation indicates an implementation error.

---

## Reference Table

| Bits | Input                                           | Hash (Secasy)                                                                                                                      |
|-----:|-------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------|
|  512 | `""` *(empty)*                                  | `171201cadb67c9037cab4d0774f1f4efd853df21737c875319f9288a6bd4eff5b36037eba14c9bcc6c8ddf75faed794a8a9a29d9764a2917636501221db14740` |
|  512 | `"a"`                                           | `7a808caa3e4a79bc840618351a3d2c05080c19885554f34e03d92680b5e9e26924fd9211318a8da40b559b1ed9cbead7ba06e4172b9eefd83e94d814a9b0db12` |
|  512 | `"z"`                                           | `dca7959152e282f0f604fd560535d5161cf80e522224cd922a72b1d7ac938e392d47bc7471bf93a877ef480e049fb4698369ab1ccb34871e28cbf3c1945c95c0` |
|  512 | `"0"`                                           | `a71fc3757caf88fe054a1987c8efd3d987bcf075521f0299000769f7d56d0605c7dfe489ccaad36165f581bdd812b5818054f00f9a73a93e7a7e47a5d0a10b73` |
|  512 | `"abc"`                                         | `0fcdfe71b624b788623d1950214285172b73963011fb020a6ba436e1fb66ed357fd8c789933b08e638e6d9c8d0d86924a4e9985c7a7bc98b136afa2a88d320d4` |
|  512 | `"secasy"`                                      | `fbc6840e290b51b709998bfe48feb479f8fba2df646803cd586dbf5970908ce6c9f999556915dfbf13af7ad5a38427c68b43a1d51b9de5f1ae3c80054e2d9277` |
|  512 | `"1234567890"`                                  | `37f1a1e3e697a10af2e4e4e3fe90ec69894467df2c11f1d6f3d21c51758c600256298db3a49046f17f7803d23b3af9ef13174dfa67bd8f6655b1ac2a148970a1` |
|  512 | `"The quick brown fox jumps over the lazy dog"` | `1fe172638cf0cb765ec1dd61fb6e10a63022bfcfa910b7b8f949c03afa7310201d798d68986fb19784f06cbec6201e89974e017c59595a818efdda5ff427e1fc` |
|  512 | `"The quick brown fox jumps over the lazy cog"` | `459b054bbb41d8deed1614dbf507c0fb36e6419f6b2e4d27692b6300ecba1c8b15dad6c18888f9faac0f9c3ed1f08e7d4d719cc821ba2b96e97309304fc82a03` |
|  512 | `0x00` *(1 byte)*                               | `02c7e717c3c562956f8e6331c0c87c82a14b39adac337ce1bfb2dad6cdc6188dabdf2d7041d971f043a41a13cd6c7e141c9fc451909c294522fdb047cae62b63` |
|  512 | `0x01` *(1 byte)*                               | `304e386689f5ae2ae32ffee66086d68f2ef90f2c228e81fdd530970beab284c2ab7bc08d65d3913d31df51e572fbec56827cfc42cd208513b7e8545130ccc576` |
|  512 | `0x80` *(1 byte)*                               | `4f4f36598dbdb1bebb48d07514dcbc4e5601193ff1acf0170e247d5a049326f8bf83741c5f3b35d4cd7afa6927707115133154a6a91721c21d2548bc2b56f8f2` |
|  512 | `0xFF` *(1 byte)*                               | `af0be72a1c79357f87144c4632621d526e486f77698bccd3bf32715c236d8562886833d6e6de75de1b894e560030906d5cf5d91a13c16ec4cac40684940d0ddc` |
|  512 | `0x00 × 8`                                      | `de51092bb53e1ab98d1e1f050cddeb1b01cae6be95a353240789418035c1cd08582d5261536418aeeb403357dcfd35f18b5fa5f438db6a2b06fd79560c49f139` |
|  512 | `0x00 × 16`                                     | `41ad1d14dc9feaa44b7008a4d2c7a8573f1472729af48d0ff6d23cb9c4e6011d800390f49fba039a277379ef92807f1577179beee9e996607da73bce94a0696f` |
|  512 | `0x00 × 32`                                     | `131e877bc9f7e0c42f31caaf59c2485c50f4eb862f087362d40b0aa8d15600f9df160e3a16bead6fe67bf62e61464a4080b46dfa14f6ac52202a7bbc162b6cdc` |
|  512 | `0x00 × 64`                                     | `e639333a956c48d76141c0fe452a06bfe55244bfe0143ab35b27960b441343353895bb1a224e9bf573c92c0e9b39b405a3e8f3ee866325f6536720a2d3b1ed89` |
|  512 | `0x00 × 1024`                                   | `10eed0c4fc3b3830900e9ec68a7783027da578605c22bfa0b24275bbc87013a6c7223ec8414fc3b9d22f05419f0294c74cb067ad9868c5f5ba2840d61b637e7a` |
|  512 | `0xFF × 8`                                      | `76b1e8fc174db951b20e562ba056b41a69c61c44d43c6927e1a032e1146b3be63b1c59653978121a0b13f3c3b09b43e5174328b35b6f04324004ee0aebcff01c` |
|  512 | `0xFF × 64`                                     | `806d67e5875d0b62168f3c792fcaded1b5de7b63d5ebf86d65287a2e43ff83c18920fa976a34408677ed5a0a610ceb810f2c2291f6c4bc16f92c598e8bafb779` |
|  512 | `0x00..0xFF` *(256 bytes)*                      | `f0f39fd6afb6f44152035bd8ef1c833ba13823f9326230f2798949a77555e42fddccdbe75c88e999516edd92532caaf254b6812490540721bdc538dc09603465` |
|  512 | `0xAA55 × 512` *(1024 bytes)*                   | `8b35491d10686c8ea0a5247615c63802a5f0b9755e047b9d38f5fbee1ef3efd70050b92057684e8a0ef4170542acaf30fbf46e2153803bd3e6073bb50b4a428d` |
|  512 | `"Markus"`                                      | `e9d8503507996b04a2e828857579471351a2c1050cee893c8ea9672ad7dc6e9fef07d75c752746c9d057fa00c9cecdaf52a56da7aafab98fa286b88503c78c00` |
|  512 | `"Anna"`                                        | `32dce55effa5d1621608a4e659e7e39d94f19c87bfc396fd43296009b000158503acc3b46e2fd3fcf6717f0cfaec70163dcf8d3671a135e4870762c230db22d6` |
|  512 | `'a' × 55`                                      | `8331433cd316e8c27fe7099ca18c7e4a29ab3c2083bcbdefb57e9ad9b66c70461d3e3393452921870f5a38610c2e87c64771c6663e5f847b0b7572fd2cf32343` |
|  512 | `'a' × 56`                                      | `f06770ecaf3fa3cd2478084a5e2530a4a21ce5343862817351bed43cfd7fcc9d9daf7a392c08410aea5bd06aa60632ef6281e725c076c670b79c673db823b1b5` |
|  512 | `'a' × 64`                                      | `63fa58014459c0b65857c2e3fba88324508541f466c5b2f3415e823645748dcab5be2d06c92c62adc5d25278f1b938cee711b62bff5e3e027555ad6a5aeba743` |

---

## 64-Bit Vectors

| Bits | Input                                           | Hash (Secasy)      |
|-----:|-------------------------------------------------|--------------------|
|   64 | `""` *(empty)*                                  | `171201cadb67c903` |
|   64 | `"a"`                                           | `7a808caa3e4a79bc` |
|   64 | `"abc"`                                         | `0fcdfe71b624b788` |
|   64 | `"secasy"`                                      | `fbc6840e290b51b7` |
|   64 | `"The quick brown fox jumps over the lazy dog"` | `1fe172638cf0cb76` |
|   64 | `"The quick brown fox jumps over the lazy cog"` | `459b054bbb41d8de` |
|   64 | `"Markus"`                                      | `e9d8503507996b04` |
|   64 | `"Anna"`                                        | `32dce55effa5d162` |

