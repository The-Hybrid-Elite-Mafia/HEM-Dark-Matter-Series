<#
  CI: proves the installer works on a clean Windows machine:
  silent install -> plug-in bundle, self-tests, guide and licences in place -> the INSTALLED plug-in
  passes the test host and the installed engine tests pass -> a user preset survives a reinstall and an
  uninstall -> uninstall removes the program and the plug-in.
#>
param([Parameter(Mandatory = $true)][string]$Installer)
$ErrorActionPreference = 'Stop'

$app     = Join-Path $env:ProgramFiles 'Hybrid Elite Mafia\HEM Dark Matter'
$vst3    = Join-Path $env:CommonProgramFiles 'VST3\HEM Dark Matter.vst3'
$presets = Join-Path $env:APPDATA 'HEM\Dark Matter'
$fails   = 0
function Check([bool]$ok, [string]$what) { if ($ok) { Write-Host "PASS  $what" } else { Write-Host "FAIL  $what"; $script:fails++ } }
function Install() { (Start-Process -FilePath $Installer -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-' -Wait -PassThru).ExitCode }

Check ((Install) -eq 0) 'silent install exit code 0'
foreach ($f in 'bin\dm_tests.exe','bin\dm_vst3_hosttest.exe','HOW-TO-TEST.txt','THIRD-PARTY-NOTICES.txt',
              'licenses\HEM-Dark-Matter-GPLv3.txt','licenses\Steinberg-VST3-SDK-LICENSE.txt','licenses\Barlow-OFL.txt','licenses\stb-LICENSE.txt','unins000.exe') {
    Check (Test-Path (Join-Path $app $f)) "installed $f"
}
$dll = Get-ChildItem $vst3 -Recurse -Filter '*.vst3' -File -ErrorAction SilentlyContinue | Select-Object -First 1
Check ($null -ne $dll) "plug-in installed in $vst3"
foreach ($f in 'Contents\Resources\fonts\Barlow-Regular.ttf','Contents\Resources\faceplates\Main-Faceplate.png','Contents\Resources\faceplates\HEM-Frame-4K.jpg') {
    Check (Test-Path (Join-Path $vst3 $f)) "faceplate art in the bundle: $f"
}
Check (Test-Path $presets) 'presets folder created'

$out = & (Join-Path $app 'bin\dm_vst3_hosttest.exe') $vst3 2>&1 | Out-String
Check ($LASTEXITCODE -eq 0 -and $out -match ' checks, 0 failed') 'INSTALLED plug-in passes the test host'
($out -split "`n" | Select-Object -Last 2) -join ' ' | Write-Host
$out = & (Join-Path $app 'bin\dm_tests.exe') 2>&1 | Out-String
Check ($LASTEXITCODE -eq 0 -and $out -match ' checks, 0 failed') 'installed engine self-test passes'
($out -split "`n" | Select-Object -Last 2) -join ' ' | Write-Host

$mine = Join-Path $presets 'SyndicateRack'
New-Item -ItemType Directory -Force -Path $mine | Out-Null
$marker = Join-Path $mine 'CI Keep Me.hempreset'
"HEMDM1`nsaturate=50`n" | Out-File $marker -Encoding ascii
Check ((Install) -eq 0) 'reinstall over the top (upgrade path) exit code 0'
Check (Test-Path $marker) 'user preset kept across reinstall'

$p = Start-Process -FilePath (Join-Path $app 'unins000.exe') -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -Wait -PassThru
Start-Sleep -Seconds 3
Check ($p.ExitCode -eq 0) 'silent uninstall exit code 0'
Check (-not (Test-Path $vst3)) 'plug-in removed on uninstall'
Check (-not (Test-Path (Join-Path $app 'bin\dm_tests.exe'))) 'program files removed on uninstall'
Check (Test-Path $marker) 'user preset kept after uninstall'

Write-Host ""
Write-Host "installer verification: $fails failure(s)"
exit $fails
