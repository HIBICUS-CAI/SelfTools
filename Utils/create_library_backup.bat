@echo off

set YEAR=%DATE:~0,4%
set MONTH=%DATE:~5,2%
set DAY=%DATE:~8,2%

set BACKUP_ROOT_DIR=D:\backup\library
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

echo Backing up files...
scp -r msi:~/public/CG_TECH ./
move CG_TECH CG_TECH_backup
scp -r msi:~/public/BG ./
move BG BG_backup

popd

pushd %BACKUP_ROOT_DIR%
echo Creating tar archive...
echo tar -cf %YEAR%-%MONTH%-%DAY%.tar %YEAR%-%MONTH%-%DAY%
tar -cf %YEAR%-%MONTH%-%DAY%.tar %YEAR%-%MONTH%-%DAY%
echo Compressing tar archive...
echo zstd %YEAR%-%MONTH%-%DAY%.tar --rm --force -T0 --ultra -22
zstd %YEAR%-%MONTH%-%DAY%.tar --rm --force -T0 --ultra -22
rmdir /s /q %YEAR%-%MONTH%-%DAY%
popd

echo Backup completed successfully.
