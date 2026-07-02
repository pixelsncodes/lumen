# grant_vst3_write.ps1 — one-time setup, run from an ELEVATED PowerShell:
#
#   powershell -ExecutionPolicy Bypass -File Tools\grant_vst3_write.ps1
#
# Grants the current user Modify rights on C:\Program Files\Common Files\VST3
# so normal (non-elevated) builds can copy Lumen.vst3 there after each build.

$ErrorActionPreference = 'Stop'
$vst3Dir = 'C:\Program Files\Common Files\VST3'

$identity  = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error 'This script must be run from an elevated (Run as Administrator) PowerShell.'
    exit 1
}

if (-not (Test-Path $vst3Dir)) {
    New-Item -ItemType Directory -Path $vst3Dir -Force | Out-Null
    Write-Host "Created $vst3Dir"
}

$user = "$env:USERDOMAIN\$env:USERNAME"
$acl  = Get-Acl $vst3Dir
$rule = New-Object System.Security.AccessControl.FileSystemAccessRule(
    $user, 'Modify', 'ContainerInherit,ObjectInherit', 'None', 'Allow')
$acl.AddAccessRule($rule)
Set-Acl $vst3Dir $acl

Write-Host "Granted Modify rights on $vst3Dir to $user."
Write-Host 'Normal builds can now copy Lumen.vst3 automatically.'
