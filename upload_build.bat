xcopy /y .\buildnumber.txt .\Delivery\
xcopy /y .\LICENSE.txt .\Delivery\
xcopy /y build\RahiTuber\Release\RahiTuber_64.exe Delivery\
xcopy /y RahiTuber\res\* Delivery\res\*

pause

butler push Delivery rahisaurus/rahituber:win --userversion-file buildnumber.txt