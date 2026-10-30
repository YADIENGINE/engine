# Encrypted JSON format

The module keeps the engine's existing XChaCha20-Poly1305 envelope unchanged.

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 8 | ASCII `ENGSAVE!` |
| 8 | 4 | Envelope version 1, little endian |
| 12 | 24 | Fresh OS-generated nonce |
| 36 | 16 | Poly1305 authentication tag |
| 52 | remaining | Ciphertext containing UTF-8 JSON |

The complete 12-byte header is authenticated associated data. Header validation and
authentication precede JSON parsing. The envelope version is independent of any
application-defined version inside the JSON document.

The caller supplies a 32-byte key for each encrypted read/write. The module never
stores keys or decides which key belongs to a document. Plaintext operations never
consult a key provider. Windows nonces use BCryptGenRandom; Linux uses
getrandom. Unsupported random providers fail explicitly.

Changing the cryptography library while keeping this algorithm and wire format
requires only a CryptoCodec adapter change. Changing the algorithm or file format
requires an explicit envelope-version/compatibility decision as well.
