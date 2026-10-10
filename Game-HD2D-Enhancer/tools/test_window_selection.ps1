param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference='Stop'
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class WindowSelectionTest {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc fn, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,StringBuilder s,int n);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
  [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h,uint cmd);
  [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h,uint kind);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int mode);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll",EntryPoint="GetWindowLongPtrW")] static extern IntPtr GetStyle(IntPtr h,int index);
  [DllImport("user32.dll",EntryPoint="SetWindowLongPtrW")] static extern IntPtr SetStyle(IntPtr h,int index,IntPtr value);
  [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int w,int height,uint flags);
  [DllImport("user32.dll",EntryPoint="SendMessageW")] public static extern IntPtr Send(IntPtr h,uint msg,IntPtr w,IntPtr l);
  [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="SendMessageW")] public static extern IntPtr Read(IntPtr h,uint msg,IntPtr w,StringBuilder text);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h,uint msg,IntPtr w,IntPtr l);
  public static void MakeFixturePopup(IntPtr h,uint expectedPid) {
    uint pid; GetWindowThreadProcessId(h,out pid);
    var cls=new StringBuilder(256); GetClassName(h,cls,256);
    if(pid!=expectedPid || cls.ToString()!="HD2DDemo" || GetWindow(h,4)==IntPtr.Zero)
      throw new InvalidOperationException("Refusing to change a non-fixture window");
    // GetParent follows an owner for WS_POPUP, not every owned overlapped window.
    // Add the missing popup style to our independently launched synthetic fixture.
    // The application under test never changes the user's game window styles.
    SetStyle(h,-16,new IntPtr(GetStyle(h,-16).ToInt64() | 0x80000000L));
    SetWindowPos(h,IntPtr.Zero,0,0,0,0,0x37);
    if((GetStyle(h,-16).ToInt64() & 0x80000000L)==0)
      throw new InvalidOperationException("Fixture popup style was not applied");
  }
  public static IntPtr Find(uint pid,string cls) {
    IntPtr found=IntPtr.Zero;
    EnumWindows((h,l)=>{
      uint p; GetWindowThreadProcessId(h,out p);
      if(p!=pid) return true;
      var s=new StringBuilder(256); GetClassName(h,s,256);
      if(s.ToString()==cls) { found=h; return false; }
      return true;
    },IntPtr.Zero);
    return found;
  }
}
'@
$exe=(Resolve-Path $Executable).Path
$started=[Collections.Generic.List[Diagnostics.Process]]::new()
$lines=[Collections.Generic.List[string]]::new()
$app=$null
function Note([string]$s) { $lines.Add($s); Write-Output $s }
function Launch([string]$arg) {
  if($arg) { $p=Start-Process -FilePath $exe -ArgumentList $arg -WorkingDirectory (Split-Path $exe) -PassThru }
  else { $p=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru }
  $started.Add($p)
  return $p
}
function Wait-Window($p,[string]$cls) {
  for($i=0;$i -lt 50;$i++) {
    $p.Refresh()
    if($p.HasExited) { throw "Process exited before $cls appeared: exit code $($p.ExitCode)" }
    $h=[WindowSelectionTest]::Find([uint32]$p.Id,$cls)
    if($h -ne [IntPtr]::Zero) { return $h }
    Start-Sleep -Milliseconds 100
  }
  throw "No window $cls in expected process $($p.Id)"
}
function Read-Text([IntPtr]$h) {
  $b=[Text.StringBuilder]::new(8192)
  [void][WindowSelectionTest]::Read($h,0x000D,[IntPtr]$b.Capacity,$b)
  return $b.ToString()
}
function Entries {
  $count=[WindowSelectionTest]::Send($box,0x0146,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32()
  $entries=[Collections.Generic.List[string]]::new()
  for($i=0;$i -lt $count;$i++) {
    $b=[Text.StringBuilder]::new(8192)
    [void][WindowSelectionTest]::Read($box,0x0148,[IntPtr]$i,$b)
    $entries.Add($b.ToString())
  }
  return ,$entries
}
function Row-For($p) {
  $e=Entries
  for($i=0;$i -lt $e.Count;$i++) { if($e[$i].Contains("PID $($p.Id) /")) { return $i } }
  throw "Window absent from real dropdown: PID $($p.Id)"
}
function Click([int]$id) {
  [void][WindowSelectionTest]::PostMessageW($main,0x0111,[IntPtr]$id,[WindowSelectionTest]::GetDlgItem($main,$id))
  Start-Sleep -Milliseconds 350
}
function Choose($p) {
  $row=Row-For $p
  [void][WindowSelectionTest]::Send($box,0x014E,[IntPtr]$row,[IntPtr]::Zero)
  [void][WindowSelectionTest]::Send($main,0x0111,[IntPtr](101 -bor (1 -shl 16)),$box)
}
try {
  $owned=Launch '--demo-owned'; $ownerWindow=Wait-Window $owned 'HD2DDemo'
  $helper=[WindowSelectionTest]::GetWindow($ownerWindow,4)
  if($helper -eq [IntPtr]::Zero -or [WindowSelectionTest]::IsWindowVisible($helper)) { throw 'Owned fixture must have an invisible owner' }
  [WindowSelectionTest]::MakeFixturePopup($ownerWindow,[uint32]$owned.Id)
  $root=[WindowSelectionTest]::GetAncestor($ownerWindow,2)
  $rootOwner=[WindowSelectionTest]::GetAncestor($ownerWindow,3)
  if($root -ne $ownerWindow -or $rootOwner -eq $ownerWindow -or $rootOwner -ne $helper) {
    throw "Fixture failed to reproduce popup parent/owner distinction: window=$ownerWindow root=$root rootOwner=$rootOwner owner=$helper"
  }
  Note 'PASS: fixture reproduces visible legacy-style popup with invisible owner; old owner filter would reject it.'
  $tool=Launch '--demo-tool'; $toolWindow=Wait-Window $tool 'HD2DDemo'
  $untitled=Launch '--demo-untitled'; $untitledWindow=Wait-Window $untitled 'HD2DDemo'
  $app=Launch ''; $main=Wait-Window $app 'HD2DMain'
  Start-Sleep -Milliseconds 600
  foreach($id in @(101,102,103,104,105,106,107,108,109,110,111,209,210)) {
    if([WindowSelectionTest]::GetDlgItem($main,$id) -eq [IntPtr]::Zero) { throw "UI control missing: $id" }
  }
  $box=[WindowSelectionTest]::GetDlgItem($main,101)
  $status=[WindowSelectionTest]::GetDlgItem($main,209)
  if([WindowSelectionTest]::Send($box,0x0147,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne -1) { throw 'An unrelated first window was selected automatically' }
  if([WindowSelectionTest]::IsWindowEnabled([WindowSelectionTest]::GetDlgItem($main,106))) { throw 'Start must remain disabled until explicit source selection' }
  $display=[WindowSelectionTest]::GetDlgItem($main,104)
  if([WindowSelectionTest]::Send($display,0x0147,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 1) { throw 'Preview must be the default' }
  Note 'PASS: no automatic first-window selection, Start initially disabled, preview default.'
  foreach($p in @($owned,$tool,$untitled)) { [void](Row-For $p) }
  Note 'PASS: owned, tool-style and untitled fixtures all appear in the real window list.'
  Choose $owned
  if(-not [WindowSelectionTest]::IsWindowEnabled([WindowSelectionTest]::GetDlgItem($main,106))) { throw 'Explicit selection did not enable Start' }
  Click 102
  $sel=[WindowSelectionTest]::Send($box,0x0147,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32()
  if($sel -ne (Row-For $owned)) { throw 'Refresh lost the explicit HWND/PID selection' }
  Note 'PASS: refresh preserves the explicitly selected window by identity.'
  [void][WindowSelectionTest]::ShowWindow($toolWindow,6)
  Click 102
  $toolRow=Row-For $tool
  if(-not (Entries)[$toolRow].Contains('已最小化')) { throw 'Minimized candidate not clearly labelled' }
  [void][WindowSelectionTest]::ShowWindow($toolWindow,4)
  Note 'PASS: minimized candidate is retained and labelled; user window not automatically restored.'
  Click 110
  Click 107
  if(-not [WindowSelectionTest]::IsWindowEnabled([WindowSelectionTest]::GetDlgItem($main,110))) { throw 'Picker cancellation left controls disabled' }
  Note 'PASS: three-second picker can be cancelled without starting capture.'
  Click 110
  [void][WindowSelectionTest]::SetForegroundWindow($ownerWindow)
  Start-Sleep -Milliseconds 250
  if([WindowSelectionTest]::GetForegroundWindow() -eq $ownerWindow) {
    Start-Sleep -Seconds 4
    if(-not (Read-Text ([WindowSelectionTest]::GetDlgItem($main,210))).Contains("PID $($owned.Id) /")) { throw 'Foreground picker followed invisible owner or failed to select exact source' }
    Note 'PASS: timed foreground picker selects the visible owned window, not its invisible owner.'
  } else {
    Click 107
    Note 'NOT VALIDATED: hosted desktop did not grant foreground activation for picker timing; no pass claimed.'
  }
  Choose $owned
  for($cycle=1;$cycle -le 3;$cycle++) {
    Click 106
    $ok=$false
    for($i=0;$i -lt 20;$i++) {
      Start-Sleep -Milliseconds 500
      $app.Refresh()
      if($app.HasExited) { throw 'Enhancer exited while starting owned-window capture' }
      $s=Read-Text $status
      if($s.Contains('增强中')) { $ok=$true; break }
    }
    if(-not $ok) { throw ('Owned-window WGC preview did not reach running state: '+(Read-Text $status)) }
    Note "PASS: owned-window WGC/NIS preview cycle $cycle reached running state."
    Click 107
    if([WindowSelectionTest]::Find([uint32]$app.Id,'HD2DOutput') -ne [IntPtr]::Zero) { throw 'Output window survived Stop' }
    $owned.Refresh()
    if($owned.HasExited -or -not [WindowSelectionTest]::IsWindow($ownerWindow)) { throw 'Source did not survive Stop' }
  }
  Note 'PASS: three capture/stop cycles release output while independent owned source stays alive.'
  Choose $tool
  [void][WindowSelectionTest]::PostMessageW($toolWindow,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)
  if(-not $tool.WaitForExit(3000)) { throw 'Fixture failed to close' }
  Click 102
  if([WindowSelectionTest]::Send($box,0x0147,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne -1) { throw 'Stale selection was silently replaced with another window' }
  Note 'PASS: a closed source clears selection instead of selecting another program.'
  [void][WindowSelectionTest]::PostMessageW($main,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)
  if(-not $app.WaitForExit(5000)) { throw 'Enhancer did not exit cleanly' }
  $owned.Refresh(); $untitled.Refresh()
  if($owned.HasExited -or $untitled.HasExited) { throw 'Independent sources did not survive enhancer exit' }
  Note 'PASS: enhancer exits cleanly; independent source processes continue.'
  Note 'Scope: synthetic hosted Windows fixtures. TSOnline device-specific capture, full-screen input, frame pacing, latency and battery remain unverified.'
} catch {
  Note ('FAIL: '+$_.Exception.Message)
  throw
} finally {
  $lines | Set-Content window-selection-test.txt -Encoding utf8
  # Stop only processes this test created, never unrelated windows/processes.
  foreach($p in $started) {
    $p.Refresh()
    if(-not $p.HasExited) {
      [void]$p.CloseMainWindow()
      if(-not $p.WaitForExit(2000)) { $p.Kill(); $p.WaitForExit() }
    }
  }
}
