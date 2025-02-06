xcopy /y .\buildnumber.txt .\Delivery-linux\
xcopy /y build-gcc\Desktop_GCC-Release\RahiTuber Delivery-linux\
xcopy /y lib-linux\* Delivery-linux\lib\
xcopy /y res\* Delivery-linux\res\*

pause

butler push Delivery-linux rahisaurus/rahituber:linux-beta-x64 --userversion-file buildnumber.txt