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
|  512 | `""` *(empty)*                                  | `b7f8505682b46bb6f46b3c43f5e8007c31b7d5dae7564298c3e777ff6daa72f9c470ddc2392ece703cafdbbb1f5750c31d91b88f79d4bada4242d9eaeeac6bfe` |
|  512 | `"a"`                                           | `291abc5b712cc388b5a1ea370365e216c6fbf9a6b07a37bc5cea23be0bfa26904ed1b454b764322fa241263da401375bed7f3722aa8ce2fa16e7dd1d6fe887cb` |
|  512 | `"z"`                                           | `f3c68bf71e6584e3a6759768bcd38d81d40cab1f17cde779cb9cf82d17712003e550fa98205141060e5d6e4e4748e7b48ecb3f24e68d04eb626a9f8663df8039` |
|  512 | `"0"`                                           | `cde76b0a6492506e6fc13da5d2d77f2b4ccb186598f7ff8cd2da97f218528c3cfe4bc89e120b7fdff3580f1b2e9b05ce83fc46de4dd2d3722624c5765bb45cc8` |
|  512 | `"abc"`                                         | `d1197317b6f4c6adc516b29549dcb9d274abdb64022dba8a003e3a5dd998986c2f29c20fca7b57d01e5852bdbd264ad9ec46f85f346cb93e6ab49209736472d8` |
|  512 | `"secasy"`                                      | `44cf221ec8fde8d06849176fdbe4709ce36c992019a690b5e356879b786cc3f2b9f63c883e89989e48666bbdacb739756e3d65d0b4acea70a703c575e7fe1a5a` |
|  512 | `"1234567890"`                                  | `d235c8a13224ae64207796ffbb5f28bc92eacd8caadfb8f8527565166764de9d498ff04f78f7ed20ae1fb4b4eb454110a956da4fc24861796dd3b24b937d8edc` |
|  512 | `"The quick brown fox jumps over the lazy dog"` | `73aed62572b95b33b1762710af4646e6aaf945bb1a835dbb19b5cb2603c49798b782a5571eb17d0b66fc3b418eeb3d0bbb589d68ca2b2b01dfc560664b648123` |
|  512 | `"The quick brown fox jumps over the lazy cog"` | `b49d9d9ac37b75b6e7a43e8f455c57edd2361749e41edb6c7823ebd0d2eaced8e84b634b9c6240fd54451818f74da471c5b80638d9a243d1ebbfe2cd40ad56e3` |
|  512 | `0x00` *(1 byte)*                               | `f7298e77c73b8ae638e86b1f8e6c761f6076b0a8d056a9a56a482644e00c97be149f38085369487e1c6a3165c06553260368ca2060a6aebeaeb54ed0446d904f` |
|  512 | `0x01` *(1 byte)*                               | `4d1e47018e6b172210b0c342b0ac5610927064762034eb79771cf509b2b686336364728331af379528c58db8055d03938e2b3f4e13204b39825954047c052414` |
|  512 | `0xFF` *(1 byte)*                               | `86e121ab1950265f39b0d32f9ca872b6adb42131efde18770c442e3222ce110342504c159a01e5e0871765b028e48e910a8252dd59e6f78351f046e657da075f` |
|  512 | `0x80` *(1 byte)*                               | `923dfc8031b6d2ef6b48d44a49d78f6521fdb2bf1dd229ea74c0eaf43aa05a1fb060eb61a2e4256cf6a376b2ef912eb283afb68fb228daa156997639ff92856a` |
|  512 | `0x00 × 8`                                      | `39cd2afff4c7cff4fd8c281a57c3ff330af4b0084249084d680c46d656ea49fea96bb058e022b6ddc6998790782b08cb3ebbb40167202b2ca1996a0f2f371133` |
|  512 | `0x00 × 16`                                     | `aac4bf3fb25ffc63025def038f4b385c3d6568ce5222259fe64d8c0d178299efa36fe2fe35de2be93f18edb9039979973adbef8ca06613a12d696b8458e4f96e` |
|  512 | `0x00 × 32`                                     | `579a76b2b0d4d715303ce08fbea37e8be05bbe45228f1e9ecd55a92c09851297aef64ace0248a961b603a3889de9cea375f57c29b01c0072a0efb143e4ae97a8` |
|  512 | `0x00 × 64`                                     | `74cfef3d8c78dff43c65d959d191881e5a1adbee92e6a01d73d830dcd36ab9f9117a2450740f286407a4b3bb6c02357d9ebd5480683990e8b675b3b71df7fbc6` |
|  512 | `0xFF × 8`                                      | `127c338fdfe68e2bf7e29a3ddd94e28781dc0d7345025e6187ba7432ae1a57102c56a3763faa77662da4340d7d6568de5f8824af034c853d3fbf191af2f24fe1` |
|  512 | `0xFF × 64`                                     | `3f297f42161f33649a36b35b11210b7b3d0d272054a18e4665774dcf1b40600e82f8e52d19da1cf3ffc67b1a7febcb143591d606ba6b29cc70d41751ab1e0790` |
|  512 | `0x00..0xFF` *(256 bytes)*                      | `f53ad8d99ac24d7051661eb63a797828566897b4ff81fe815a4fb8b02dbae6ba7be90935cfa23015b1bc5150a30c9d15f0e1b740560469da684af1bf7f302037` |
|  512 | `"Markus"`                                      | `1b18d19d83393932d0ec6a496d297dd4d1fa53bb4cee8fe516e1956fb8ca7a9935ef0c957518ead12a17415394b4ac59aec39ae2e4d68210472a3081970465f9` |
|  512 | `"Anna"`                                        | `bd3b6c745f38b580c83e60137a8b60f756b7ae02714bcfe358a77474830bbae24f4c48b58ae0b4b27b85ea99329333c720d0f9aaad32ecafc57711e995cb185d` |
|  512 | `'a' × 55`                                      | `97758ff95317ce9f4935fa8989697a4ff4a9d5abadec73d55c697e045384567a746f31d140e665bae527f61c6aef1d7129c624a1f44937e9e134445f53a67fa7` |
|  512 | `'a' × 56`                                      | `f36944042cae8140a6094b117a7e7fda09e389e8518530fdf23aa9f26d8731ad185d153afd2e436b520346e61c81cf4776f4c04650b0a7f0aa070c707b4df095` |
|  512 | `'a' × 64`                                      | `28d0470e8ba7b6eecdcb1656d4ec5a28c3999963916ee7395959a3bbdf0cf9a5d904948c42370f319387cfc6fd8908309a2db113ec9cec7bfa72be56b40553cc` |
|  512 | `0x00 × 1024`                                   | `f7a952f2c61b3d3184463a8ecc8a46f8feb6ef1423e7898927f0168b5a6164e88ea68c7920bfd5fb8d99f3d61d3a802af2a3ee21928b24df2d54258e71e49712` |
|  512 | `0xAA55 × 512` *(1024 bytes)*                   | `67721ea2186928b93d7c2d4c439d83271cb8e926317a39b089e234b59fcea02fb993b638e17c8a8c9a71b50d0baa9d2f1805c5a91cf0fd616d44b0e7f65356ef` |

---

## 64-Bit Vectors

| Bits | Input                                           | Hash (Secasy)      |
|-----:|-------------------------------------------------|--------------------|
|   64 | `""` *(empty)*                                  | `b7f8505682b46bb6` |
|   64 | `"a"`                                           | `291abc5b712cc388` |
|   64 | `"abc"`                                         | `d1197317b6f4c6ad` |
|   64 | `"secasy"`                                      | `44cf221ec8fde8d0` |
|   64 | `"The quick brown fox jumps over the lazy dog"` | `73aed62572b95b33` |
|   64 | `"The quick brown fox jumps over the lazy cog"` | `b49d9d9ac37b75b6` |
|   64 | `"Markus"`                                      | `1b18d19d83393932` |
|   64 | `"Anna"`                                        | `bd3b6c745f38b580` |

