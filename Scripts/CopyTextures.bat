SET BASEDIR=%~dp0
for /R "%BASEDIR%..\Resources\Textures" %%f in (*) do copy /y "%%f" "%BASEDIR%..\bin\Resources\Textures"