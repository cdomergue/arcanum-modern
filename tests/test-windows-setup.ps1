param([Parameter(Mandatory = $true)][string]$Setup)
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true') {
    throw 'Run this installation test only on a disposable GitHub Actions Windows runner.'
}
$Setup = (Resolve-Path $Setup).Path
$work = Join-Path $env:RUNNER_TEMP ('arcanum-setup-' + [guid]::NewGuid())
$game = Join-Path $work 'Arcanum test é'
New-Item -ItemType Directory -Path $game -Force | Out-Null

function Invoke-Setup([string]$Directory, [bool]$Success) {
    $process = Start-Process -FilePath $Setup -ArgumentList "/S /D=$Directory" -PassThru -Wait
    if (($process.ExitCode -eq 0) -ne $Success) {
        throw "Unexpected setup exit code: $($process.ExitCode)"
    }
}

try {
    Invoke-Setup $game $false
    if (Test-Path (Join-Path $game 'arcanum-modern.exe')) {
        throw 'Setup installed into a folder without the required game files.'
    }
    $originals = @('arcanum1.dat', 'arcanum2.dat', 'arcanum3.dat', 'tig.dat',
        'modules/Arcanum.dat', 'Arcanum.exe', 'arcanum.cfg',
        'modules/Arcanum/Save/Slot0001.tfaf', 'modern-ui/settings.ini',
        'modern-ui/graphics.ini', 'modern-ui/notebook-test.txt')
    $hashes = @{}
    foreach ($name in $originals) {
        $path = Join-Path $game $name
        New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null
        Set-Content -LiteralPath $path -Value "Preserve this fixture: $name"
        $hashes[$name] = (Get-FileHash -LiteralPath $path).Hash
    }
    Invoke-Setup $game $true
    Invoke-Setup $game $true
    $manifest = Get-Content (Join-Path $game 'ArcanumModern/manifest.json') -Raw | ConvertFrom-Json
    foreach ($entry in $manifest.sha256.PSObject.Properties) {
        if ((Get-FileHash (Join-Path $game $entry.Name)).Hash -ne $entry.Value) {
            throw "Incorrect installed file: $($entry.Name)"
        }
    }
    $shell = New-Object -ComObject WScript.Shell
    $links = @(
        (Join-Path ([Environment]::GetFolderPath('Desktop')) 'Arcanum Modern.lnk'),
        (Join-Path ([Environment]::GetFolderPath('Programs')) 'Arcanum Modern/Arcanum Modern.lnk')
    )
    foreach ($link in $links) {
        if (-not (Test-Path $link)) { throw "Missing shortcut: $link" }
        $shortcut = $shell.CreateShortcut($link)
        if ($shortcut.TargetPath -ne (Join-Path $game 'arcanum-modern.exe') -or
            $shortcut.WorkingDirectory -ne $game -or $shortcut.Arguments -ne '-geometry=1280x720') {
            throw "Invalid shortcut target, arguments or working directory: $link"
        }
    }
    $uninstaller = Join-Path $work 'uninstall-test.exe'
    Copy-Item (Join-Path $game 'Uninstall Arcanum Modern.exe') $uninstaller
    $process = Start-Process $uninstaller -ArgumentList "/S _?=$game" -PassThru -Wait
    if ($process.ExitCode -ne 0) { throw 'Uninstall failed.' }
    foreach ($entry in $manifest.sha256.PSObject.Properties) {
        if (Test-Path (Join-Path $game $entry.Name)) { throw "Mod file left behind: $($entry.Name)" }
    }
    foreach ($name in $originals) {
        if ((Get-FileHash (Join-Path $game $name)).Hash -ne $hashes[$name]) {
            throw "Original game data, save or preference changed: $name"
        }
    }
    foreach ($link in $links) {
        if (Test-Path $link) { throw "Shortcut left behind: $link" }
    }
    Write-Output 'PASS: invalid folder rejected; install, reinstall, hashes, shortcuts and safe uninstall.'
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force
}
