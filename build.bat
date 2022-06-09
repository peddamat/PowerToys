REM git clean -xfd

msbuild -t:build -restore -p:RestorePackagesConfig=true /p:Configuration=Release /p:Platform=x64 PowerToys.sln

msbuild -t:build -restore -p:RestorePackagesConfig=true /p:Configuration=Release /p:Platform=x64 tools\BugReportTool\BugReportTool.sln

msbuild -t:build -restore -p:RestorePackagesConfig=true /p:Configuration=Release /p:Platform=x64 tools\WebcamReportTool\WebcamReportTool.sln

msbuild -t:build -restore -p:RestorePackagesConfig=true /p:Configuration=Release /p:Platform=x64 tools\StylesReportTool\StylesReportTool.sln

msbuild -t:build -restore -p:RestorePackagesConfig=true /p:Configuration=Release /p:Platform=x64 installer\PowerToysSetup.sln