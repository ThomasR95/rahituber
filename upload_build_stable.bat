@echo off 
echo THIS IS THE STABLE BUILD. ARE YOU SURE?
echo Uploading as version:
type Last_Delivery\buildnumber.txt
echo.
echo.
pause

butler push Last_Delivery rahisaurus/rahituber:win-stable --userversion-file Last_Delivery/buildnumber.txt