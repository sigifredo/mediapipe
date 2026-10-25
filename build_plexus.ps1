# Construye las bibliotecas de landmarks de Plexus en Windows y las deja en dist\:
#   plexus_landmarks.dll / plexus_landmarks.lib   fachada (sin dependencia de MediaPipe)
#   plexus_landmarks_cpu.dll                       motor CPU (solo CPU: MediaPipe desactiva la GPU en Windows)
#   opencv_world4120.dll                           dependencia del motor
# Requiere Visual Studio 2022 (Build Tools con C++), OpenCV en C:\opencv\build y Bazel 7.7.0.
# Uso: .\build_plexus.ps1 [-Bazel <ruta a bazel.exe>]
param(
    [string]$Bazel = ""
)

# Bazel escribe su progreso en stderr; con "Stop" PowerShell 5.1 lo trataría como error. Se valida $LASTEXITCODE.
$ErrorActionPreference = "Continue"
Set-Location $PSScriptRoot

$TargetDir = "mediapipe/plexus/landmarks"
$Dist = "dist"

if (-not $Bazel) {
    $versioned = Join-Path $env:USERPROFILE ".bazel\bin\bazel-$((Get-Content .bazelversion).Trim()).exe"
    if (Test-Path $versioned) { $Bazel = $versioned } else { $Bazel = "bazel" }
}

# Sin JDK en el sistema, rules_java 7.10 genera una etiqueta inexistente y el análisis falla.
# Se usa el JDK que trae el propio Bazel.
if (-not $env:JAVA_HOME) {
    $installBase = ((& $Bazel info install_base 2>$null) | Select-Object -Last 1).Trim()
    $env:JAVA_HOME = Join-Path $installBase "embedded_tools\jdk"
}

$common = @("-c", "opt", "--config=windows", "--define", "MEDIAPIPE_DISABLE_GPU=1", "--platform_suffix=-cpu")

& $Bazel build @common "//$TargetDir`:libplexus_landmarks.so" "//$TargetDir`:libplexus_landmarks_engine.so" "//$TargetDir`:models"
if ($LASTEXITCODE -ne 0) { throw "bazel build falló ($LASTEXITCODE)" }

$out = "bazel-out/x64_windows-opt--cpu/bin"
New-Item -ItemType Directory -Force $Dist | Out-Null

# Bazel conserva el nombre libplexus_landmarks*.so aunque el archivo sea una DLL.
Copy-Item -Force "$out/$TargetDir/libplexus_landmarks.so" "$Dist/plexus_landmarks.dll"

# El .lib de Bazel apunta a "libplexus_landmarks.so" (nombre del target). Se regenera desde los
# exports de la DLL para que los consumidores enlacen contra plexus_landmarks.dll.
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "No se encontró Visual Studio con las herramientas de C++" }
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
$exports = & cmd /c "set PATH=%PATH%;$(Split-Path $vswhere) && `"$vcvars`" >nul && dumpbin /nologo /exports $Dist\plexus_landmarks.dll"
$symbols = $exports | ForEach-Object { if ($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)') { $Matches[1] } }
if (-not $symbols) { throw "plexus_landmarks.dll no exporta símbolos" }
$def = Join-Path $Dist "plexus_landmarks.def"
(@("LIBRARY plexus_landmarks.dll", "EXPORTS") + $symbols) | Set-Content -Encoding ascii $def
Remove-Item -Force "$Dist\plexus_landmarks.lib" -ErrorAction SilentlyContinue
& cmd /c "set PATH=%PATH%;$(Split-Path $vswhere) && `"$vcvars`" >nul && lib/nologo /machine:x64 /def:$def /out:$Dist\plexus_landmarks.lib"
if ($LASTEXITCODE -ne 0) { throw "lib falló ($LASTEXITCODE)" }
Remove-Item $def, "$Dist\plexus_landmarks.exp" -ErrorAction SilentlyContinue
Copy-Item -Force "$out/$TargetDir/libplexus_landmarks_engine.so" "$Dist/plexus_landmarks_cpu.dll"

# La DLL de OpenCV debe estar junto al motor.
Get-ChildItem "$out/$TargetDir" -Filter "opencv_world*.dll" -ErrorAction SilentlyContinue | Copy-Item -Destination $Dist -Force
if (-not (Test-Path "$Dist/opencv_world*.dll")) {
    Get-ChildItem "C:\opencv\build\x64\vc16\bin" -Filter "opencv_world[0-9]*.dll" | Where-Object { $_.Name -notmatch 'd\.dll$' } | Copy-Item -Destination $Dist -Force
}

# Modelos en dist\mediapipe\modules\...: el motor los busca junto a su biblioteca.
Push-Location $out
Get-ChildItem -Recurse -Path "mediapipe/modules" -Filter "*.tflite" | ForEach-Object {
    $rel = (Resolve-Path -Relative $_.FullName).TrimStart(".\")
    $dest = Join-Path (Join-Path $PSScriptRoot $Dist) $rel
    New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
    Copy-Item -Force $_.FullName $dest
}
Pop-Location
$hand = Join-Path $Dist "mediapipe/modules/hand_landmark"
New-Item -ItemType Directory -Force $hand | Out-Null
Copy-Item -Force "mediapipe/modules/hand_landmark/handedness.txt" $hand

Get-ChildItem $Dist | Format-Table Name, Length
