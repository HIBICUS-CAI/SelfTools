import os, platform

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT_FILE = os.path.join(PROJECT_DIR, 'Hider.csproj')
TARGETS = ['win-x64', 'linux-x64', 'linux-arm64']


def host_target():
    """Return the target matching this machine, or None if it is not a supported target."""
    system = platform.system()
    machine = platform.machine().lower()
    arch = {'amd64': 'x64', 'x86_64': 'x64', 'aarch64': 'arm64', 'arm64': 'arm64'}.get(machine)
    target = {'Windows': f'win-{arch}', 'Linux': f'linux-{arch}'}.get(system) if arch else None
    return target if target in TARGETS else None


def publish_dir(target):
    # Under bin/ so the SDK's default item globs do not pick the output up as project items.
    return os.path.join(PROJECT_DIR, 'bin', 'publish', target)
