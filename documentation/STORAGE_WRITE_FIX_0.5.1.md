# CH375USB 0.5.1 storage write corruption fix

**Status:** fixed in the 0.5.1 source line after real-hardware validation on Pocket386.  
**Date:** 2026-08-23

## Symptom

The affected 0.5.1 write path could report successful DOS writes while corrupting FAT16 filesystem metadata. In the reproduced failure, copying only a small number of files could leave the USB volume appearing empty in both DOS and Linux; a read-only `fsck.fat` check reported both FAT copies as corrupt.

The same USB device and filesystem remained writable with the older FreddyV/WCH-derived `CH375286.SYS`, providing an important control that separated the failure from the flash drive itself.

## Root cause

The corruption was caused by incorrect far-pointer handling in the CH375USB write-buffer copy path.

The affected sequence changed `DS` to the caller's transfer-buffer segment and only afterwards loaded `xfer_off`:

```asm
mov ax,[xfer_seg]
mov ds,ax
mov si,[xfer_off]
```

`xfer_seg` and `xfer_off` are resident driver variables. Once `DS` had been changed, the final load no longer addressed the driver's `xfer_off`; it read an unrelated word from the caller's segment and could therefore transmit bytes from the wrong offset while the CH375 transaction itself still completed successfully.

The same ordering bug existed in both storage backends:

- CH375 native `DISK_WRITE` path (`native_write_chunk64`);
- generic USB Mass Storage BOT/SCSI outbound-buffer path (`far_to_usb_buf`).

Equivalent far-pointer ordering in the write-verification comparison paths was corrected at the same time.

## Fix

The driver now reads both resident transfer-pointer components before changing `DS`, with an explicit `CS` segment override:

```asm
mov ax,[cs:xfer_seg]
mov si,[cs:xfer_off]
mov ds,ax
```

This correction is applied consistently to the native and BOT write paths and their verification code.

## Additional 0.5.1 safety hardening

Every completed 512-byte sector write is now immediately read back through the active storage backend and compared with the caller's source sector before the write is allowed to return success to DOS.

This is intentionally conservative. It reduces write performance, but a transport/buffer error is converted into a DOS write failure instead of being silently accepted and later surfacing as filesystem corruption.

`SYNC_AFTER_WRITE` remains enabled. The BOT backend can issue SCSI `SYNCHRONIZE CACHE(10)`; the CH375 native backend has no separate cache-flush command in the current implementation, so its native sync helper remains a successful no-op after the per-sector read-back verification.

## Validation

The patched test build was validated on the Pocket386 and file copying works correctly. The older `CH375286.SYS` also copies files correctly on the same setup and was used as the known-working storage control.

The earlier observation that a FAT16 volume became corrupt must therefore not be attributed to FAT16 geometry alone. The 1 GiB FAT16 layout remains the project's conservative, tested interoperability baseline, but the destructive write failure reproduced here was a CH375USB write-path bug.

## FAT16 formatting note

To request the documented 1 GiB FAT16 layout without `mkfs.fat` automatically realigning the reserved/root areas, use explicit no-alignment and reserved-sector options:

```sh
sudo mkfs.fat -a -F 16 -S 512 -s 64 -f 2 -R 1 -r 512 -n DOSUSB /dev/sdX1
```

This produces the intended 512-byte sectors, 32 KiB clusters, two FATs, one reserved sector and 512 root-directory entries. Partition creation remains separate from filesystem creation.

## Regression rule

Storage-write changes are not considered validated merely because DOS reports that a copy completed. At minimum, future storage regression testing must include:

1. copying files to the CH375USB drive under DOS;
2. reading/copying them back;
3. rebooting or moving the device to another OS;
4. confirming the files remain visible and byte-correct;
5. checking the filesystem read-only when storage code has changed.

Never diagnose a filesystem-layout problem before first comparing the same media with a known-working CH375 driver or direct host access.
