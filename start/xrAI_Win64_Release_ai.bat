set currDir=%cd%\..\bins\Win64\Release\
Pushd "E:\Program Files\X-Ray CoP SDK\editors"
"%currDir%xrAI.exe" -f test_sdk_level -keep_temp_files
pause