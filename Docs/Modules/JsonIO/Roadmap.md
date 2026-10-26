# Planned work

No additional features are scheduled. Streaming JSON and alternative backends are
outside the current API; add a concrete proposal here before extending it.

- Gzip reading: `Read` does not decompress. Supporting it needs a production inflater
  (sinfl is test-only) and a limit on the decompressed size against compression bombs.
- Gzip together with encryption (compress, then encrypt) is rejected until a caller needs it.
