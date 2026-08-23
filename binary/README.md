# Prebuilt binaries — CH375USB 0.5.1

`CH375USB.SYS` is intentionally not published here at the moment.

The previously published 0.5.1 binary predates the storage write-pointer fix documented in [`../documentation/STORAGE_WRITE_FIX_0.5.1.md`](../documentation/STORAGE_WRITE_FIX_0.5.1.md) and must not be used for USB-storage writes because it can corrupt filesystem metadata.

The corrected 0.5.1 source is published in the repository and has been validated on real Pocket386 hardware. A new `CH375USB.SYS` should be published here only after it is rebuilt from the corrected 0.5.1 source.

`CH375MOU.DRV` and `MTEST.COM` are unaffected by the storage-write bug and remain published.
