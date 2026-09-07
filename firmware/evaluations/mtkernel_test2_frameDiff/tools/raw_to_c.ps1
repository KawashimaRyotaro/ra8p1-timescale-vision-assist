$projectRoot = Split-Path -Parent $PSScriptRoot

$inputFile = Join-Path $projectRoot "Application\assets\video_128x72_rgb565.raw"
$outputC   = Join-Path $projectRoot "Application\assets\video_data.c"
$outputH   = Join-Path $projectRoot "Application\assets\video_data.h"

$bytes = [System.IO.File]::ReadAllBytes($inputFile)

$expectedSize = 128 * 72 * 2 * 10

if ($bytes.Length -ne $expectedSize)
{
    throw "Unexpected RAW size: $($bytes.Length) bytes. Expected $expectedSize bytes."
}

$header = @"
#ifndef VIDEO_DATA_H
#define VIDEO_DATA_H

#include <stdint.h>

#define VIDEO_WIDTH        (128U)
#define VIDEO_HEIGHT       (72U)
#define VIDEO_FRAME_COUNT  (10U)
#define VIDEO_FRAME_BYTES  (VIDEO_WIDTH * VIDEO_HEIGHT * 2U)
#define VIDEO_DATA_SIZE    (VIDEO_FRAME_BYTES * VIDEO_FRAME_COUNT)

extern const uint8_t g_video_data[VIDEO_DATA_SIZE];

#endif
"@

[System.IO.File]::WriteAllText($outputH, $header)

$sb = New-Object System.Text.StringBuilder

[void] $sb.AppendLine('#include "video_data.h"')
[void] $sb.AppendLine('')
[void] $sb.AppendLine('const uint8_t g_video_data[VIDEO_DATA_SIZE] =')
[void] $sb.AppendLine('{')

for ($i = 0; $i -lt $bytes.Length; $i++)
{
    if (($i % 16) -eq 0)
    {
        [void] $sb.Append("    ")
    }

    [void] $sb.AppendFormat("0x{0:X2}", $bytes[$i])

    if ($i -ne ($bytes.Length - 1))
    {
        [void] $sb.Append(", ")
    }

    if (($i % 16) -eq 15)
    {
        [void] $sb.AppendLine()
    }
}

if (($bytes.Length % 16) -ne 0)
{
    [void] $sb.AppendLine()
}

[void] $sb.AppendLine('};')

[System.IO.File]::WriteAllText($outputC, $sb.ToString())

Write-Host "Generated:"
Write-Host $outputC
Write-Host $outputH
Write-Host "Size:" $bytes.Length "bytes"