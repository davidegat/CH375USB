# Known issues / TODO

This file tracks known CH375USB bugs, limitations that may require code changes, and implementation TODOs that are intentionally deferred.

## Open

### BOT `SYNCHRONIZE CACHE` failure is not propagated to DOS

**Status:** TODO — deferred for a later driver revision.

When `SYNC_AFTER_WRITE = 1`, `drv_write` calls `storage_sync_cache` after all requested sectors have been written.

On the generic USB Mass Storage BOT path, `storage_sync_cache` sends SCSI `SYNCHRONIZE CACHE(10)`. If that operation fails, `storage_sync_cache` returns with Carry set, but `drv_write` currently does not check the Carry Flag and still returns `ST_DONE` to DOS.

This does **not** mean the preceding `WRITE(10)` data transfer was necessarily incorrect: the write itself already completed through BOT and its CSW was validated. The bug is that a failure of the final cache-flush operation can be reported to DOS as a successful write, so persistence of the just-written data cannot be guaranteed in that failure case.

**Planned fix:** after `call storage_sync_cache`, test Carry and branch to the existing write-failure path when synchronization fails.

The CH375 native `DISK_*` storage path is different: `storage_sync_cache` currently returns success there because native writes are considered complete after `DISK_WRITE` finishes with `USB_INT_SUCCESS`. Any future change to native flush semantics should be handled separately and tested on real hardware.

## Resolved

None yet.
