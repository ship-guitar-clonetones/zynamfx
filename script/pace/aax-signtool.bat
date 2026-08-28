@echo off
:: Shim handed to PACE wraptool via --signtool (see sign-aax-windows.ps1).
:: wraptool invokes this instead of signtool.exe; the Python script rebuilds
:: the command line for Azure Artifact Signing (dlib) and calls the real
:: signtool. Batch is only the entry point because --signtool must be a
:: cmd-executable file; all logic lives in aax-signtool.py.
python "%~dp0aax-signtool.py" %*
exit /b %errorlevel%
