param([string]$Exe = 'build-msvc\Release\CaeciliaNotchIsland.exe')
$ErrorActionPreference = 'Stop'
# Interactive desktop test: moves pointer, sends demo keys, temporarily disables client animation.
# Never persists the animation setting; restores it and the pointer in finally.
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class DesktopCheck {
  [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr w);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr w,out Rect r);
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out Point p);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern void mouse_event(uint f,uint x,uint y,uint d,UIntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte k,byte s,uint f,UIntPtr e);
  [DllImport("user32.dll",EntryPoint="SystemParametersInfoW")] public static extern bool GetAnimation(uint a,uint p,out int v,uint f);
  [DllImport("user32.dll",EntryPoint="SystemParametersInfoW")] public static extern bool SetAnimation(uint a,uint p,IntPtr v,uint f);
}
'@
[DesktopCheck]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
if (Get-Process CaeciliaNotchIsland -ErrorAction SilentlyContinue) { throw 'Close the existing island before testing.' }
$out = Join-Path (Get-Location) 'artifacts\m1-validation\desktop'
New-Item -ItemType Directory -Force $out | Out-Null
$config = Join-Path $out 'config.json'
'{"hoverDelayMs":150,"monitorIndex":0}' | Set-Content $config -Encoding UTF8
$original = New-Object DesktopCheck+Point
[DesktopCheck]::GetCursorPos([ref]$original) | Out-Null
$animation = 0
if (-not [DesktopCheck]::GetAnimation(0x1042,0,[ref]$animation,0)) { throw 'Cannot read animation setting' }
$screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$cx = $screen.X + [int]($screen.Width / 2)
$scale = 1.0
$process = $null
$backdrop = New-Object System.Windows.Forms.Form
$backdrop.FormBorderStyle = 'None'
$backdrop.BackColor = [System.Drawing.Color]::White
$backdrop.Bounds = New-Object System.Drawing.Rectangle ($cx - 500), $screen.Y, 1000, 500
$backdrop.Show()
$backdrop.Activate()
[System.Windows.Forms.Application]::DoEvents()
$results = [System.Collections.Generic.List[string]]::new()
function MovePointer([double]$dx, [double]$y) {
    $x = $cx + [int]($dx * $scale); $py = $screen.Y + [int]($y * $scale)
    [DesktopCheck]::mouse_event(0x8001,[uint32](($x * 65535)/($screen.Width-1)),[uint32](($py * 65535)/($screen.Height-1)),0,[UIntPtr]::Zero)
}
function Key([byte]$k) {
    [DesktopCheck]::keybd_event($k,0,0,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 20
    [DesktopCheck]::keybd_event($k,0,2,[UIntPtr]::Zero)
}
function Click { [DesktopCheck]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 20; [DesktopCheck]::mouse_event(4,0,0,0,[UIntPtr]::Zero) }
function Shot($name) {
    [System.Windows.Forms.Application]::DoEvents()
    $rect = New-Object DesktopCheck+Rect
    if (-not [DesktopCheck]::GetWindowRect($process.MainWindowHandle,[ref]$rect)) { throw 'Cannot get overlay rectangle' }
    $width = $rect.Right-$rect.Left; $height = $rect.Bottom-$rect.Top
    if ($width -ne [Math]::Round(492*$scale) -or $height -ne [Math]::Round(193*$scale,0,[MidpointRounding]::AwayFromZero)) { throw "Unexpected envelope: $width x $height" }
    $bmp = New-Object System.Drawing.Bitmap $width,$height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try { $g.CopyFromScreen($rect.Left,$rect.Top,0,0,$bmp.Size); $bmp.Save((Join-Path $out "$name.png")) }
    finally { $g.Dispose(); $bmp.Dispose() }
    if ([DesktopCheck]::GetForegroundWindow() -eq $process.MainWindowHandle) { throw 'Overlay stole foreground focus' }
    $results.Add("CAPTURE $name envelope=${width}x${height} dpi=$($scale*96) overlayNotForeground=true")
}
try {
    foreach ($reduced in @($false,$true)) {
        if (-not [DesktopCheck]::SetAnimation(0x1043,0,[IntPtr]([int](-not $reduced)),2)) { throw 'Cannot set client animation' }
        $mode = if ($reduced) { 'reduced' } else { 'normal' }
        MovePointer 0 350
        $process = Start-Process (Resolve-Path $Exe) -ArgumentList '--demo','--verify-device-recovery','--config',('"'+$config+'"') -PassThru
        Start-Sleep -Milliseconds 1500
        $process.Refresh()
        if ($process.HasExited -or $process.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Overlay startup failed' }
        $scale = [DesktopCheck]::GetDpiForWindow($process.MainWindowHandle)/96.0
        Shot "$mode-01-dormant"
        MovePointer 0 10; Start-Sleep -Milliseconds 800; Shot "$mode-02-hover-expanded"
        MovePointer 0 350; Start-Sleep -Milliseconds 800; Shot "$mode-03-collapsed"
        MovePointer 0 10; Click; Start-Sleep -Milliseconds 800
        MovePointer 0 350; Start-Sleep -Milliseconds 400; Shot "$mode-04-pinned"
        MovePointer -58 22; Click; Start-Sleep -Milliseconds 700; Shot "$mode-05-telemetry"
        MovePointer 26 22; Click; Start-Sleep -Milliseconds 700; Shot "$mode-06-tray"
        MovePointer -142 22; Click; Start-Sleep -Milliseconds 700; Shot "$mode-07-now-playing"
        MovePointer 0 6; Click; Start-Sleep -Milliseconds 800; Shot "$mode-08-empty-unpin"
        Key 0x32; Start-Sleep -Milliseconds 800; Shot "$mode-09-compact-a"
        Start-Sleep -Milliseconds 350; Shot "$mode-10-compact-b"
        Key 0x34; Start-Sleep -Milliseconds 2100; Shot "$mode-11-pinned-hud"
        Key 0x42; Start-Sleep -Milliseconds 650; Shot "$mode-12-brightness"
        Key 0x50; Start-Sleep -Milliseconds 650; Shot "$mode-13-pomodoro"
        Start-Sleep -Milliseconds 1500; Shot "$mode-14-hud-expired"
        Key 0x31; Start-Sleep -Milliseconds 800; Shot "$mode-15-key1"
        Key 0x33; Start-Sleep -Milliseconds 800; Shot "$mode-16-key3"
        Key 0x43; Start-Sleep -Milliseconds 800; Shot "$mode-17-close-media-hovered"
        MovePointer 0 350; Start-Sleep -Milliseconds 300
        MovePointer 0 10; Click; Start-Sleep -Milliseconds 800
        MovePointer 0 350; Click; Start-Sleep -Milliseconds 800; Shot "$mode-18-outside-unpin"
        if (-not $process.CloseMainWindow() -or -not $process.WaitForExit(3000)) { throw 'Overlay did not shut down cleanly' }
        if ($process.ExitCode -ne 0) { throw "Overlay exit code $($process.ExitCode)" }
        $process = $null
    }
    $results | Set-Content (Join-Path $out 'results.txt')
    $results
} finally {
    if ($process -and -not $process.HasExited) { $process.CloseMainWindow() | Out-Null; $process.WaitForExit(3000) | Out-Null }
    [DesktopCheck]::SetAnimation(0x1043,0,[IntPtr]$animation,2) | Out-Null
    [DesktopCheck]::SetCursorPos($original.X,$original.Y) | Out-Null
    $backdrop.Close(); $backdrop.Dispose()
}
