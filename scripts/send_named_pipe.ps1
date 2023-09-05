# This PowerShell script connects to a Named Pipe server,
# sends one message and then disconnects again.
# The first argument is the name of the pipe.
# The second argument is the message to send.
if ($args.length -lt 2) {
	Write-Output "Usage: ./send_named_pipe.ps1 <pipename> <message> [message] ... [message]"
	exit -1
}

$Wrapper = [pscustomobject]@{
	Pipe = new-object System.IO.Pipes.NamedPipeClientStream(
		".",
		$args[0],
		[System.IO.Pipes.PipeDirection]::InOut,
		[System.IO.Pipes.PipeOptions]::None,
		[System.Security.Principal.TokenImpersonationLevel]::Impersonation
	)
	Reader = $null
	Writer = $null
}
try {
	$Wrapper.Pipe.Connect(5000)
	$Wrapper.Reader = New-Object System.IO.StreamReader($Wrapper.Pipe)
	$Wrapper.Writer = New-Object System.IO.StreamWriter($Wrapper.Pipe)
	$Wrapper.Writer.AutoFlush = $true
	for ($i = 1; $i -lt $args.length; $i++) {
		$Wrapper.Writer.WriteLine($args[$i])
	}
	# Wait for pipe contents to be read.
	$Wrapper.Pipe.WaitForPipeDrain()
	# Dispose the pipe, which also calls Flush and Close.
	$Wrapper.Pipe.Dispose()
	# Explicity set error level 0 for success, as otherwise the current error level is kept.
	exit 0
} catch [TimeoutException] {
	Write-Output "Timeout connecting to pipe"
	exit 1
} catch [System.IO.IOException] {
	Write-Output "Broken pipe"
	exit 2
}
