SET BASEDIR=%~dp0
mkdir "%BASEDIR%..\bin\Resources"
mkdir "%BASEDIR%..\bin\Resources\Textures"
mkdir "%BASEDIR%..\bin\Resources\Models"
for /R "%BASEDIR%..\Resources\Textures" %%f in (*) do copy /y "%%f" "%BASEDIR%..\bin\Resources\textures\"
for /R "%BASEDIR%..\Resources\Models" %%f in (*) do copy /y "%%f" "%BASEDIR%..\bin\Resources\Models"