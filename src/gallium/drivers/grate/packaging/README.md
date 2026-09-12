# Deploying the grate driver on Alpine / postmarketOS

This builds Mesa with the grate driver as a drop-in replacement for Alpine's
`mesa` package, for Tegra 2/3/4 devices (Tegra 3 is what it has been tested
on). Everything the GL stack uses lives in one `libgallium-<version>.so`, so
the whole set of mesa packages has to be replaced together rather than dropping
a single driver in beside the stock ones.

## Building

    abuild-keygen -a -i          # once, to sign your packages
    cp APKBUILD 0100-*.patch .   # into a build directory
    abuild checksum
    abuild -r

The result lands in `~/.local/share/abuild/<repo>/<arch>/`.

The recipe is Alpine's own `main/mesa` APKBUILD with two changes: the grate
patch is added to `source`, and `_gallium_drivers` is cut down to
`grate,llvmpipe,softpipe`. The stock armv7 list also builds radeonsi, nouveau,
zink and the other ARM drivers, none of which are any use on a Tegra and which
together are hours of build time on one. Put them back on that one line if you
want the full set. `tegra` must NOT be in the list: that is the Tegra K1
driver, and it claims the same DRM device, so the two cannot be enabled at
once.

Building on the device itself takes about an hour on a Tegra 3.

## Installing

    apk add ./mesa-*.apk ./mesa-dri-gallium-*.apk ./mesa-egl-*.apk \
            ./mesa-gl-*.apk ./mesa-gles-*.apk ./mesa-gbm-*.apk \
            ./mesa-dev-*.apk ./mesa-vulkan-*.apk

`pkgrel` is set to 100 so apk treats these as newer than Alpine's. That also
means a plain `apk upgrade` will not pull the stock mesa back over the top,
but it will once Alpine's own pkgrel passes 100, so pin it if you care.

`mesa-va-gallium` and `mesa-vulkan-ati` are not built (they need radeonsi).
Remove them first if they are installed, or apk will refuse the version
mismatch.

## Running weston

    apk add weston weston-backend-drm seatd
    seatd -g video &                 # weston needs a seat to open the DRM device

    weston --backend=drm --renderer=gl

Do NOT pass `--xwayland` yet. See below.

Over ssh, weston needs a seat to open the DRM device; `LIBSEAT_BACKEND=seatd`
with seatd running is the least painful way to get one.

## What works

OpenGL ES 2.0 on the GPU, glxgears at ~34fps on the panel through Xwayland,
and weston compositing its own clients. See ../tests/README.md for the test
suites and what they cover.

**Do not enable Xwayland.** glamor's shaders use fragment opcodes the driver
does not implement (FLR, FRC, RCP) and overflow its 19 scalar temporaries. The
register allocator falls back to a wrong register rather than failing the
compile, so a malformed program reaches the GPU, hangs it, and the kernel then
resets gr3d in a loop:

    [drm] tegra_drm_sched_timedout_job: 3d channel: pipes 0x2 (process:glxgears)
    tegra-gr3d 54180000.gr3d: [drm:tegra_drm_sched_timedout_job] resetting hardware

On a Tegra 3 that takes the whole machine down - running glxgears under
Xwayland reboots the device. Wayland clients are unaffected and stable. libGL,
GLX and the DRI3 buffer sharing all work; it is only glamor's shaders that are
beyond the fragment compiler today.
