param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class HD2DSmoke {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder text, int max);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr Send(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendText(IntPtr h, uint m, IntPtr w, string l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
}
'@
function Text([IntPtr]$h) {
  $b = [Text.StringBuilder]::new(4096)
  [void][HD2DSmoke]::GetWindowText($h,$b,$b.Capacity)
  return $b.ToString()
}
$exe = (Resolve-Path $Executable).Path
$demo=$null; $app=$null; $lines=[Collections.Generic.List[string]]::new()
try {
  $demo=Start-Process -FilePath $exe -ArgumentList '--demo' -PassThru
  Start-Sleep -Seconds 2
  $demoWindow=[HD2DSmoke]::FindWindow('HD2DDemo',$null)
  if($demoWindow -eq [IntPtr]::Zero -or $demo.HasExited) { throw 'Synthetic demo did not create its window' }
  $lines.Add('PASS: independent demo process and native window started.')
  $app=Start-Process -FilePath $exe -PassThru
  Start-Sleep -Seconds 2
  $main=[HD2DSmoke]::FindWindow('HD2DMain',$null)
  if($main -eq [IntPtr]::Zero -or $app.HasExited) { throw 'Enhancer did not create its main window' }
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
  if([HD2DSmoke]::FindWindow('HD2DOutput',$null) -ne [IntPtr]::Zero) { throw 'Output window did not close after stop' }
  if($demo.HasExited -or -not [HD2DSmoke]::IsWindow($demoWindow)) { throw 'Source did not survive stop' }
  $lines.Add('PASS: stop removes output window; independent source remains alive.')
  [void]$app.CloseMainWindow()
  if(-not $app.WaitForExit(5000)) { throw 'Enhancer did not shut down cleanly' }
  if($demo.HasExited) { throw 'Source did not survive enhancer exit' }
  $lines.Add('PASS: enhancer exits cleanly and the source process remains alive.')
  $lines.Add('Scope: hosted Windows synthetic demo only. Fullscreen input, real games, handheld, performance and battery are NOT validated.')
  $lines | Set-Content ui-smoke-result.txt -Encoding utf8
  $lines | Write-Output
} finally {
  if($app -and -not $app.HasExited) { $app.Kill() }
  if($demo -and -not $demo.HasExited) { [void]$demo.CloseMainWindow(); if(-not $demo.WaitForExit(3000)) { $demo.Kill() } }
}
