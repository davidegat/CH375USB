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

### No user-visible DOS notification when a supported USB device is attached or detached

**Status:** TODO — feature not implemented yet.

CH375USB already detects hotplug internally for the currently supported device classes (USB mass storage, boot-protocol keyboard and boot-protocol mouse), but it does not currently notify the DOS user when one of those devices is connected or disconnected.

The driver therefore updates its internal state and re-enumerates/invalidate devices as required, but DOS does not display messages such as:

```text
CH375USB: USB storage connected.
CH375USB: USB storage disconnected.
CH375USB: USB keyboard connected.
CH375USB: USB keyboard disconnected.
CH375USB: USB mouse connected.
CH375USB: USB mouse disconnected.
```

Directly printing from the timer interrupt or from a USB transaction path should be avoided because it could be unsafe and could corrupt the display of a foreground DOS application.

**Planned implementation:** add a small resident hotplug-event queue in CH375USB and expose it through a private `INT 2Fh` API. The queue should preserve the device class before a detached slot is cleared, so disconnect events still identify whether the removed device was storage, keyboard or mouse.

A separate optional DOS utility, tentatively `CH375MON.COM`, could consume those events and display notifications without coupling console output to the low-level USB driver. The same API could later also be used by other DOS programs for status displays, logging or audible notifications.

## Resolved

None yet.
