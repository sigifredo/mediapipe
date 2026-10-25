# Pruebas de selección de dispositivo de plexus_landmarks en Windows (solo CPU). Requiere .\build_plexus.ps1.
# Equivale a test_landmarks.sh sin los escenarios de GPU y EGL, que no existen en Windows.
# Uso: .\test_landmarks.ps1
$ErrorActionPreference = "Continue"
Set-Location $PSScriptRoot

$Src = "mediapipe/plexus/landmarks"
$Graphs = "$Src/graphs"
$Out = Join-Path ([System.IO.Path]::GetTempPath()) ("plexus_landmarks_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force $Out | Out-Null
$Failed = $false

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "No se encontró Visual Studio con las herramientas de C++" }
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
$env:PATH += ";" + (Split-Path $vswhere)

$smoke = Join-Path $Out "landmarks_smoke.exe"
& cmd /c "`"$vcvars`" >nul && cl /nologo /EHsc /std:c++17 /I$Src /Fo$Out\ /Fe$smoke $Src/LandmarksSmoke.cpp dist\plexus_landmarks.lib"
if ($LASTEXITCODE -ne 0) { throw "no se pudo compilar LandmarksSmoke.cpp" }

# El consumidor enlaza solo la fachada; las demás DLL se buscan junto a ella.
Copy-Item dist\plexus_landmarks.dll $Out

function Run-Case([string]$Name, [string]$Expect, [hashtable]$EnvVars, [string[]]$CaseArgs) {
    $saved = @{}
    foreach ($k in $EnvVars.Keys) { $saved[$k] = [Environment]::GetEnvironmentVariable($k); [Environment]::SetEnvironmentVariable($k, $EnvVars[$k]) }
    $stdout = & $smoke @CaseArgs 2>"$Out\stderr.txt"
    $code = $LASTEXITCODE
    foreach ($k in $saved.Keys) { [Environment]::SetEnvironmentVariable($k, $saved[$k]) }
    $text = ($stdout -join " ").Trim()
    if ($Expect -eq "FAIL") {
        if ($code -ne 0) { Write-Host "ok    $Name (falla como se espera)" }
        else { Write-Host "FALLA ${Name}: debía fallar"; $script:Failed = $true }
    }
    elseif ($code -eq 0 -and $text.Contains($Expect)) { Write-Host "ok    $Name -> $text" }
    else {
        Write-Host "FALLA ${Name}: salida='$text' código=$code"
        Get-Content "$Out\stderr.txt" -Tail 3
        $script:Failed = $true
    }
}

$Hand = "$Graphs/hand_{device}.pbtxt"
$Body = "$Graphs/body_{device}.pbtxt"
$LibDir = "$PWD\dist"
$base = @{ PLEXUS_LANDMARKS_LIBDIR = $LibDir; PLEXUS_LANDMARKS_RESOURCE_DIR = $LibDir }
$noEnv = $base

Run-Case "mano forzada a cpu"        "device=cpu" $noEnv @("hand", $Hand, "cpu")
Run-Case "cuerpo forzado a cpu"      "device=cpu" $noEnv @("body", $Body, "cpu")
Run-Case "env fuerza cpu sobre auto" "device=cpu" ($base + @{ PLEXUS_LANDMARKS_DEVICE = "cpu" }) @("hand", $Hand, "auto")
Run-Case "auto sin marcador usa cpu" "device=cpu" $noEnv @("hand", "$Graphs/hand_cpu.pbtxt", "auto")
Run-Case "cierre de entrada en cpu (mano)" "close_ok=1" $noEnv @("hand", $Hand, "cpu")

# Sin biblioteca gpu en el directorio: auto cae a cpu, gpu explícito falla.
$soloCpu = Join-Path $Out "solo_cpu"
New-Item -ItemType Directory -Force $soloCpu | Out-Null
Copy-Item dist\plexus_landmarks.dll, dist\plexus_landmarks_cpu.dll, dist\opencv_world*.dll $soloCpu
New-Item -ItemType Junction -Path "$soloCpu\mediapipe" -Target "$PWD\dist\mediapipe" | Out-Null
$solo = @{ PLEXUS_LANDMARKS_LIBDIR = $soloCpu; PLEXUS_LANDMARKS_RESOURCE_DIR = $LibDir }
Run-Case "auto sin plexus_landmarks_gpu" "device=cpu" $solo @("hand", $Hand, "auto")
Run-Case "gpu explícito sin biblioteca"  "FAIL"       $solo @("hand", $Hand, "gpu")

(Get-Item "$soloCpu\mediapipe").Delete()
Remove-Item -Recurse -Force $Out
if ($Failed) { exit 1 }
