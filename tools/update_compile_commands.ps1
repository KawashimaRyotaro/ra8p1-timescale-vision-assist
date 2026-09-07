$Root = Split-Path -Parent $PSScriptRoot

$Files = Get-ChildItem `
    -Path "$Root/firmware/evaluations" `
    -Filter "compile_commands.json" `
    -Recurse |
    Where-Object {
        $_.FullName -match '[\\/]Debug[\\/]compile_commands\.json$'
    }

$All = @()

foreach ($File in $Files) {
    Write-Host "Adding $($File.FullName)"

    $Commands =
        Get-Content $File.FullName -Raw |
        ConvertFrom-Json

    $All += $Commands
}

$Output =
    "$Root/.vscode/compile_commands.json"

$All |
    ConvertTo-Json -Depth 20 |
    Set-Content $Output -Encoding UTF8

Write-Host "Generated: $Output"
Write-Host "Entries: $($All.Count)"