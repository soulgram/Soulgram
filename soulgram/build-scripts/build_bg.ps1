# Runs build.cmd at BelowNormal priority and reports the linker's peak memory.
param([string]$Config = 'Release')

$log = 'F:\TBuild\logs\build.log'
$started = Get-Date
$p = Start-Process -FilePath cmd.exe `
	-ArgumentList "/c `"F:\TBuild\build.cmd $Config > $log 2>&1`"" `
	-PassThru -WindowStyle Hidden
$null = $p.Handle # keeps ExitCode available after the process exits
$p.PriorityClass = 'BelowNormal'

$peakWorkingSet = 0
$peakPrivate = 0
while (-not $p.HasExited) {
	foreach ($link in Get-Process link -ErrorAction SilentlyContinue) {
		try {
			$peakWorkingSet = [Math]::Max($peakWorkingSet, $link.PeakWorkingSet64)
			$peakPrivate = [Math]::Max($peakPrivate, $link.PeakPagedMemorySize64)
		} catch {}
	}
	Start-Sleep -Milliseconds 1000
}
$p.WaitForExit()

$duration = (Get-Date) - $started
'build exit code: {0}, duration: {1:hh\:mm\:ss}' -f $p.ExitCode, $duration
'linker peak: working set {0:N1} GB, private {1:N1} GB' -f `
	($peakWorkingSet / 1GB), ($peakPrivate / 1GB)
'--- tail ---'
Get-Content $log -Tail 15
exit $p.ExitCode
