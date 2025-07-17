xcopy /y .\buildnumber.txt .\Delivery-linux\
xcopy /y build\Desktop-Release\RahiTuber Delivery-linux\
rem TODO: Check if the libs are still necessary
xcopy /y RahiTuber\lib-linux\* Delivery-linux\lib\
xcopy /y RahiTuber\res\* Delivery-linux\res\*

pause

butler push Delivery-linux rahisaurus/rahituber:linux-beta-x64 --userversion-file buildnumber.txt