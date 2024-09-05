SET BASEDIR=%~dp0
for /R "%BASEDIR%..\Shaders\Compiled" %%f in (*) do copy /y "%%f" "%BASEDIR%..\bin\Shaders"