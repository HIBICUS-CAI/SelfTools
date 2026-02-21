@echo off

set YEAR=%DATE:~0,4%
set MONTH=%DATE:~5,2%
set DAY=%DATE:~8,2%

set BACKUP_ROOT_DIR=D:\backup
if not exist %BACKUP_ROOT_DIR% mkdir %BACKUP_ROOT_DIR%

set BACKUP_DIR=%BACKUP_ROOT_DIR%\%YEAR%-%MONTH%-%DAY%
echo Backing up to %BACKUP_DIR%...

if exist %BACKUP_DIR% (
    rmdir /s /q %BACKUP_DIR%
    echo Existing backup directory removed.
)
mkdir %BACKUP_DIR%
echo Backup directory created.
pushd %BACKUP_DIR%
mkdir %BACKUP_DIR%\local
mkdir %BACKUP_DIR%\server
mkdir %BACKUP_DIR%\server\msi

echo Backing up server files...
pushd %BACKUP_DIR%\server\msi
scp msi:/etc/samba/smb.conf ./
move smb.conf etc_samba_smb.conf.backup
scp -r msi:~/.ssh ./
move .ssh ssh_backup
scp -r msi:~/.config ./
move .config config_backup
scp -r msi:~/.gitconfig ./
move .gitconfig gitconfig_backup
scp -r msi:~/.zshrc ./
move .zshrc zshrc_backup
scp -r msi:~/.bashrc ./
move .bashrc bashrc_backup
popd

echo Backing up local files...
pushd %BACKUP_DIR%\local
xcopy /e /y C:\Users\%USERNAME%\.ssh\ .\ssh_backup\
xcopy /e /y C:\Users\%USERNAME%\.ssh\ .\ssh_backup\
copy C:\Users\%USERNAME%\Documents\PowerShell\Microsoft.PowerShell_profile.ps1 .\Microsoft.PowerShell_profile.ps1.backup
copy C:\Users\%USERNAME%\AppData\Roaming\Code\User\settings.json .\vscode_settings.json.backup
copy C:\Users\%USERNAME%\AppData\Local\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json .\windows_terminal_settings.json.backup
xcopy /e /y D:\doc\ .\doc_backup\
xcopy /e /y D:\scripts\ .\scripts_backup\
popd

pushd %BACKUP_ROOT_DIR%
echo Creating tar archive...
echo tar -cf %YEAR%-%MONTH%-%DAY%.tar %YEAR%-%MONTH%-%DAY%
tar -cf %YEAR%-%MONTH%-%DAY%.tar %YEAR%-%MONTH%-%DAY%
echo Compressing tar archive...
echo zstd %YEAR%-%MONTH%-%DAY%.tar --rm --force -T0 --ultra -20
zstd %YEAR%-%MONTH%-%DAY%.tar --rm --force -T0 --ultra -20
rmdir /s /q %YEAR%-%MONTH%-%DAY%
popd

echo Backup completed successfully.
