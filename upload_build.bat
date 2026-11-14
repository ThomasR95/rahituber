@echo off 

echo DID YOU PUSH THE STABLE BUILD WITH THE PREVIOUS VERSION NUMBER YET?

echo Uploading as version:
type buildnumber.txt
echo.
echo Currently in Last_Delivery:
type Last_Delivery\buildnumber.txt
echo.

echo.


pause

REM xcopy /y .\buildnumber.txt .\Delivery\
xcopy /y .\LICENSE.txt .\Delivery\
xcopy /y build\RahiTuber\Release\RahiTuber_64.exe Delivery\
xcopy /y RahiTuber\res\* Delivery\res\*



pause

butler push Delivery rahisaurus/rahituber:win --userversion-file buildnumber.txt

echo Pushed. backing up current delivery to Last_Delivery

xcopy /y .\buildnumber.txt .\Last_Delivery\
xcopy /y .\LICENSE.txt .\Last_Delivery\
xcopy /y build\RahiTuber\Release\RahiTuber_64.exe Last_Delivery\
xcopy /y RahiTuber\res\* Last_Delivery\res\