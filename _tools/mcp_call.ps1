param(
    [Parameter(Mandatory=$true)][string]$Tool,
    [string]$ArgFile = ""
)
$ErrorActionPreference = 'Stop'
$arguments = @{}
if ($ArgFile -ne "" -and (Test-Path $ArgFile)) {
    $arguments = Get-Content -Raw $ArgFile | ConvertFrom-Json
}
$payload = @{
    jsonrpc = "2.0"
    id      = [int](Get-Random -Maximum 100000)
    method  = "tools/call"
    params  = @{ name = $Tool; arguments = $arguments }
} | ConvertTo-Json -Depth 20

$r = Invoke-WebRequest -Uri 'http://127.0.0.1:8089/mcp' -Method POST -Body ([System.Text.Encoding]::UTF8.GetBytes($payload)) -ContentType 'application/json; charset=utf-8' -Headers @{ 'Accept' = 'application/json, text/event-stream' } -UseBasicParsing -TimeoutSec 300
$content = $r.Content
# Strip SSE framing if present
if ($content -match '(?s)data:\s*(\{.*\})') { $content = $Matches[1] }
$j = $content | ConvertFrom-Json
if ($null -ne $j.error) {
    Write-Output ("MCP-ERROR: " + ($j.error | ConvertTo-Json -Depth 10))
    exit 1
}
foreach ($block in $j.result.content) {
    if ($block.type -eq 'text') { Write-Output $block.text }
}
if ($j.result.isError) { exit 1 }
