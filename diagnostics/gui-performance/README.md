# Moonlight GUI Performance Diagnostic

This executable isolates Qt Quick window and scene-graph performance from
Moonlight's networking, discovery, streaming, SDL, and settings backends.
It is intentionally excluded from the normal Moonlight build.

## Build

From a Visual Studio developer command prompt:

```powershell
mkdir build
cd build
qmake ..\gui-performance.pro
jom
```

## Stages

Use keys `1` through `5` while the window is focused:

1. Solid `QQuickWindow`
2. Material controls
3. Toolbar and `StackView`
4. Moonlight-like computer grid
5. Moonlight-like settings page

Press `B` to toggle the deterministic animation benchmark, `C` to toggle
continuous rendering, and `F1` to show configuration details. The title bar
reports FPS, 95th-percentile frame time, and worst frame time.

The FPS counter is most meaningful while resizing or when continuous rendering
is enabled. Ordinary window movement can be handled by DWM without requiring
the application to present new frames, so judge movement by visual smoothness.

## Startup Options

```text
--stage solid|material|shell|grid|settings
--render-loop default|basic|threaded
--graphics-api default|d3d11|d3d12|opengl|vulkan|software
--continuous
--benchmark
--d3d-legacy-swapchain
--d3d-max-frame-latency frames
--disable-vblank-virtualization
--disable-redirection-surface
--alpha-buffer
--smoke-test
```

Render-loop and graphics-API choices must be startup options because Qt reads
them before constructing the first window.

`--disable-redirection-surface` removes DWM's fallback bitmap and is intended
only for D3D testing. `--alpha-buffer` makes Qt use DirectComposition for D3D,
which stretches the last frame while the window and swap chain have different
sizes.

## Windows Resize Surfaces

Three different backgrounds can be visible during startup or an interactive
resize:

1. `QQuickWindow::color` is the scene graph's render-pass clear color. It only
   applies after Qt begins rendering a real frame.
2. A normal modern D3D Qt window uses `DXGI_SCALING_NONE`. While the HWND and
   swap chain temporarily have different sizes, DXGI exposes black outside the
   last completed frame.
3. DWM normally retains a separate redirection bitmap for the HWND. Unpainted
   or stale portions of that bitmap can appear white when the window shrinks.

The production Windows D3D configuration requests an alpha-capable Qt Quick
surface. Qt uses that request to select a DirectComposition swap chain with
`DXGI_SCALING_STRETCH`; the application itself remains opaque. It also sets
`QT_QPA_DISABLE_REDIRECTION_SURFACE=1`, which becomes the native
`WS_EX_NOREDIRECTIONBITMAP` window style. DirectComposition provides the actual
window visual, so the redundant DWM bitmap is unnecessary.

Explicit OpenGL and software GUI backends do not receive these D3D-specific
settings.

For Qt scene-graph timing details, also set:

```powershell
$env:QSG_RENDER_TIMING = "1"
```

The most useful first comparisons on Windows are:

```powershell
.\moonlight-gui-performance.exe --stage grid --benchmark
.\moonlight-gui-performance.exe --stage grid --benchmark --graphics-api d3d11 --d3d-legacy-swapchain
.\moonlight-gui-performance.exe --stage grid --benchmark --graphics-api d3d11 --d3d-max-frame-latency 1
.\moonlight-gui-performance.exe --stage grid --benchmark --graphics-api d3d12
.\moonlight-gui-performance.exe --stage solid --graphics-api d3d12 --disable-redirection-surface
.\moonlight-gui-performance.exe --stage solid --graphics-api d3d12 --alpha-buffer
.\moonlight-gui-performance.exe --stage grid --benchmark --disable-vblank-virtualization
.\moonlight-gui-performance.exe --stage grid --benchmark --graphics-api opengl
.\moonlight-gui-performance.exe --stage grid --benchmark --render-loop basic
```

To force Qt's software D3D adapter and verify the WARP fallback path:

```powershell
$env:QSG_RHI_PREFER_SOFTWARE_RENDERER = "1"
.\moonlight-gui-performance.exe --stage solid --graphics-api d3d12 --alpha-buffer --disable-redirection-surface
Remove-Item Env:QSG_RHI_PREFER_SOFTWARE_RENDERER
```

The diagnostic prints the actual DXGI adapter name and whether DXGI marks it
as software when the first frame begins rendering.

If the solid stage is slow, the bottleneck is below Moonlight's QML content.
If performance drops at a later stage, that stage identifies the first layer
of GUI complexity that matters.
