xcopy /y .\buildnumber.txt .\Delivery\
xcopy /y Release\x64\RahiTuber_64.exe Delivery\
xcopy /y Release\Win32\RahiTuber.exe Delivery\

pause

butler push Delivery rahisaurus/rahituber:win --userversion-file buildnumber.txt