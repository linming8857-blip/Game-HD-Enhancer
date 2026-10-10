param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class HD2DSmoke {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
  public static IntPtr FindClass(string cls) { return FindWindow(cls, null); }
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder text, int max);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder text, int max);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr Send(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendText(IntPtr h, uint m, IntPtr w, string l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageTimeoutW")] static extern IntPtr ReadText(IntPtr h, uint m, IntPtr w, StringBuilder l, uint flags, uint timeout, out IntPtr result);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr l);
  public static string Text(IntPtr h) {
    var b = new StringBuilder(4096); IntPtr result;
    ReadText(h, 0x000D, new IntPtr(b.Capacity), b, 2, 1000, out result);
    return b.ToString();
  }
  public static string Describe(uint pid) {
    var lines = new List<string>();
    EnumWindows((h,l) => {
      uint p; GetWindowThreadProcessId(h,out p);
      if(p != pid) return true;
      var cls = new StringBuilder(256); GetClassName(h,cls,256);
      lines.Add("Window " + cls + ": " + Text(h));
      EnumChildWindows(h,(child,data) => { var t=Text(child); if(t.Length>0) lines.Add("  Control: " + t); return true; },IntPtr.Zero);
      return true;
    },IntPtr.Zero);
    return string.Join("\n",lines);
  }
}
'@
function Text([IntPtr]$h) { return [HD2DSmoke]::Text($h) }
$exe = (Resolve-Path $Executable).Path
$demo=$null; $app=$null; $lines=[Collections.Generic.List[string]]::new()
function Wait-Window([string]$Class,[Diagnostics.Process]$Process) {
  for($attempt=0;$attempt -lt 20;$attempt++) {
    $window=[HD2DSmoke]::FindClass($Class)
    if($window -ne [IntPtr]::Zero) { return $window }
    $Process.Refresh()
    if($Process.HasExited) { throw "$Class process exited: code $($Process.ExitCode)" }
    Start-Sleep -Milliseconds 500
  }
  $lines.Add([HD2DSmoke]::Describe([uint32]$Process.Id))
  throw "$Class was not found after 10 seconds"
}
try {
  $lines.Add('Environment UserInteractive: '+[Environment]::UserInteractive)
  $demo=Start-Process -FilePath $exe -ArgumentList '--demo' -PassThru
  $demoWindow=Wait-Window 'HD2DDemo' $demo
  $lines.Add('PASS: independent demo process and native window started.')
  $app=Start-Process -FilePath $exe -PassThru
  $main=Wait-Window 'HD2DMain' $app
  Start-Sleep -Milliseconds 500
  $lines.Add('PASS: enhancer main window started.')
  $status=[HD2DSmoke]::GetDlgItem($main,209)
  $lines.Add('Initial status: '+(Text $status))
  foreach($id in @(101,102,103,104,105,106,107,108,109,209)) {
    if([HD2DSmoke]::GetDlgItem($main,$id) -eq [IntPtr]::Zero) { throw "Missing UI control $id" }
  }
  $lines.Add('PASS: all ten required UI controls exist.')
  $source=[HD2DSmoke]::GetDlgItem($main,101)
  $index=[HD2DSmoke]::SendText($source,0x014C,[IntPtr](-1),'HD2D').ToInt32()
  if($index -lt 0) { throw 'Synthetic source was absent from the real window list' }
  [void][HD2DSmoke]::Send($source,0x014E,[IntPtr]$index,[IntPtr]::Zero)
  [void][HD2DSmoke]::Send([HD2DSmoke]::GetDlgItem($main,104),0x014E,[IntPtr]1,[IntPtr]::Zero)
  $lines.Add('PASS: selected synthetic source in preview mode through actual UI controls.')
  [void][HD2DSmoke]::PostMessage($main,0x0111,[IntPtr]106,[HD2DSmoke]::GetDlgItem($main,106))
  $capturePassed=$false
  for($i=0;$i -lt 15;$i++) {
    Start-Sleep -Seconds 1
    $s=Text $status
    if($s.Contains('增强中')) { $capturePassed=$true; break }
    if($app.HasExited) { throw 'Enhancer exited during capture startup' }
  }
  if($capturePassed) {
    $lines.Add('PASS: live WGC -> GPU enhancement -> preview presentation reached running state.')
  } else {
    $lines.Add('NOT VALIDATED: capture did not reach running state in this hosted desktop. Do not treat this as a capture pass.')
  }
  $lines.Add('Capture status: '+(Text $status))
  $dialog=[HD2DSmoke]::FindWindow('#32770','Game-HD2D-Enhancer · 测试版')
  if($dialog -ne [IntPtr]::Zero) { [void][HD2DSmoke]::PostMessage($dialog,0x0010,[IntPtr]::Zero,[IntPtr]::Zero); Start-Sleep -Seconds 1 }
  [void][HD2DSmoke]::PostMessage($main,0x0111,[IntPtr]107,[HD2DSmoke]::GetDlgItem($main,107))
  Start-Sleep -Seconds 1
  if([HD2DSmoke]::FindClass('HD2DOutput') -ne [IntPtr]::Zero) { throw 'Output window did not close after stop' }
  if($demo.HasExited -or -not [HD2DSmoke]::IsWindow($demoWindow)) { throw 'Source did not survive stop' }
  $lines.Add('PASS: stop removes output window; independent source remains alive.')
  [void][HD2DSmoke]::PostMessage($main,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)
  if(-not $app.WaitForExit(5000)) { throw 'Enhancer did not shut down cleanly' }
  if($demo.HasExited) { throw 'Source did not survive enhancer exit' }
  $lines.Add('PASS: enhancer exits cleanly and the source process remains alive.')
  $lines.Add('Scope: hosted Windows synthetic demo only. Fullscreen input, real games, handheld, performance and battery are NOT validated.')
} catch {
  $lines.Add('FAIL: '+$_.Exception.Message)
  foreach($process in @($demo,$app)) {
    if($process) {
      $process.Refresh()
      $lines.Add("Process $($process.Id): exited=$($process.HasExited)")
      if($process.HasExited) { $lines.Add("Exit code: $($process.ExitCode)") }
      else { $lines.Add([HD2DSmoke]::Describe([uint32]$process.Id)) }
    }
  }
  throw
} finally {
  $lines | Set-Content ui-smoke-result.txt -Encoding utf8
  $lines | Write-Output
  if($app -and -not $app.HasExited) { $app.Kill() }
  if($demo -and -not $demo.HasExited) { [void]$demo.CloseMainWindow(); if(-not $demo.WaitForExit(3000)) { $demo.Kill() } }
}
