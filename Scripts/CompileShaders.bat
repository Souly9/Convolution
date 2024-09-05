SET BASEDIR=%~dp0
for /R "%BASEDIR%..\Shaders\Compiled" %%f in (*) do del "%%f"

for /R "%BASEDIR%..\Shaders\" %%f in (*) do "%BASEDIR%..\External\VulkanSDK\1.3.290.0\Bin\glslc.exe" "%%f" -o "%%f.spv"

for /R "%BASEDIR%..\Shaders\" %%f in (*.spv) do move "%%f" "%BASEDIR%..\Shaders\Compiled"
pause