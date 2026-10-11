import argparse, base64, os, platform, shutil, subprocess, sys
from common import TARGETS, host_target, publish_dir

parser = argparse.ArgumentParser(description='Install the published Hider for the current machine (overwrites existing files).')
parser.add_argument('-d', '--dest', help='Install folder. Default: %%LOCALAPPDATA%%\\Programs\\Hider on Windows.')
args = parser.parse_args()

target = host_target()
if target is None:
    sys.exit(f'This machine is not a supported target ({", ".join(TARGETS)}).')

is_windows = platform.system() == 'Windows'
exe_name = 'Hider.exe' if is_windows else 'Hider'
src_dir = publish_dir(target)
if not os.path.isfile(os.path.join(src_dir, exe_name)):
    sys.exit(f'No build found in {src_dir}, run "python build.py {target}" first.')


def copy_files(dest_dir):
    print(f'Copying {src_dir} -> {dest_dir}', flush=True)
    try:
        shutil.copytree(src_dir, dest_dir, dirs_exist_ok=True)
    except (shutil.Error, OSError) as e:
        sys.exit(f'Copy failed, close Hider if it is running and retry.\n{e}')


def install_windows(dest_dir):
    copy_files(dest_dir)
    # WScript.Shell is available on every Windows install, so no extra Python package is needed.
    # Paths go through environment variables and the script is passed encoded to avoid quoting issues.
    script = '; '.join([
        "$ErrorActionPreference = 'Stop'",
        "$link = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Hider.lnk'",
        '$shortcut = (New-Object -ComObject WScript.Shell).CreateShortcut($link)',
        '$shortcut.TargetPath = $env:HIDER_TARGET_PATH',
        '$shortcut.WorkingDirectory = $env:HIDER_WORKING_DIR',
        '$shortcut.Save()',
        "Write-Output ('Created shortcut: ' + $link)",
    ])
    env = os.environ.copy()
    env['HIDER_TARGET_PATH'] = os.path.join(dest_dir, exe_name)
    env['HIDER_WORKING_DIR'] = dest_dir
    encoded = base64.b64encode(script.encode('utf-16-le')).decode('ascii')
    command = ['powershell', '-NoProfile', '-NonInteractive', '-OutputFormat', 'Text', '-EncodedCommand', encoded]
    if subprocess.run(command, env=env).returncode != 0:
        sys.exit('Failed to create the desktop shortcut.')


if is_windows:
    dest = os.path.abspath(args.dest or os.path.join(os.environ['LOCALAPPDATA'], 'Programs', 'Hider'))
    install_windows(dest)
else:
    sys.exit('Installing on this OS is not supported yet.')

print(f'Installed to {dest}')
