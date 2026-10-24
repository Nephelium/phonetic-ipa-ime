param([string]$ProcessName = 'Weixin')
$ErrorActionPreference = 'Stop'
# Read only the PE headers of this product's loaded TSF DLL, never host text.
if (!('IpaModuleHeaderProbe' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class IpaModuleHeaderProbe {
 [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] bytes, UIntPtr count, out UIntPtr read);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 public static uint[] Read(int pid, long address) {
  IntPtr process=OpenProcess(0x410,false,pid); if(process==IntPtr.Zero)throw new System.ComponentModel.Win32Exception();
  try {
   byte[] dos=new byte[64];UIntPtr read;
   if(!ReadProcessMemory(process,new IntPtr(address),dos,(UIntPtr)dos.Length,out read)||read.ToUInt64()!=64||dos[0]!=77||dos[1]!=90)throw new InvalidOperationException("Cannot read own module DOS header");
   int offset=BitConverter.ToInt32(dos,60);if(offset<64||offset>4096)throw new InvalidOperationException("Unexpected PE header offset");
   byte[] pe=new byte[88];
   if(!ReadProcessMemory(process,new IntPtr(address+offset),pe,(UIntPtr)pe.Length,out read)||read.ToUInt64()!=88||BitConverter.ToUInt32(pe,0)!=0x4550)throw new InvalidOperationException("Cannot read own module PE header");
   return new uint[]{BitConverter.ToUInt32(pe,8),BitConverter.ToUInt32(pe,80)};
  } finally {CloseHandle(process);}
 }
}
'@
}
foreach ($taskProcess in Get-Process -Name $ProcessName -ErrorAction SilentlyContinue) {
    foreach ($taskModule in $taskProcess.Modules | Where-Object ModuleName -eq 'PhoneticIpaTsf.dll') {
        $taskPath = [IO.Path]::GetFullPath($taskModule.FileName)
        if ($taskPath -notlike '*\ipa-prototype-workspace\output\runtime\tsf-*\PhoneticIpaTsf.dll' -and $taskPath -notlike '*\PhoneticIPA*\output\runtime\tsf-*\PhoneticIpaTsf.dll') { continue }
        $taskHeaders = [IpaModuleHeaderProbe]::Read($taskProcess.Id,$taskModule.BaseAddress.ToInt64())
        [PSCustomObject]@{Process=$taskProcess.ProcessName;Pid=$taskProcess.Id;Module=$taskPath;LoadedLinkTimestamp=$taskHeaders[0];LoadedImageSize=$taskHeaders[1]}
    }
}
