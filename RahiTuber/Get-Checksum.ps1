Get-FileHash RahiTuber_64.exe -A SHA256

Write-Output ""
$otherHash = Read-Host "Please paste the SHA-256 file hash found on the itch.io DevLog for this version"

if ( (Get-FileHash RahiTuber_64.exe -A SHA256).hash -eq $otherHash )
{
	Write-Output ""
	Write-Output "The hash codes match, this copy is genuine."
}
else
{
	Write-Output ""
	Write-Output "The hash codes do not match! Please double-check you got the right version."
}