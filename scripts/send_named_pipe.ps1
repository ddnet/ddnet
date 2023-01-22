# This PowerShell script connects to a Named Pipe server,
# sends one message and then disconnects again.
# The first argument is the name of the pipe.
# The second argument is the message to send.
if ($args.length -lt 2) {
	Write-Output "Usage: ./send_named_pipe.ps1 <pipename> <message> [message] ... [message]"
	return
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
$Wrapper.Pipe.Connect(5000)
if (!$?) {
	return
}
$Wrapper.Reader = New-Object System.IO.StreamReader($Wrapper.Pipe)
$Wrapper.Writer = New-Object System.IO.StreamWriter($Wrapper.Pipe)
$Wrapper.Writer.AutoFlush = $true
for ($i = 1; $i -lt $args.length; $i++) {
	$Wrapper.Writer.WriteLine($args[$i])
}
# We need to wait because the lines will not be written if we close the pipe immediately
Start-Sleep -Seconds 1.5
$Wrapper.Pipe.Close()
