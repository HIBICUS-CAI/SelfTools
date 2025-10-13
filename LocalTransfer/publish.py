import platform, os, subprocess

runtime_prefix: str
if platform.system() == 'Windows':
    runtime_prefix = 'win'
elif platform.system() == 'Linux':
    runtime_prefix = 'linux'
runtime_suffix: str
if platform.machine() == 'x86_64' or platform.machine() == 'AMD64':
    runtime_suffix = '-x64'
elif platform.machine() == 'aarch64' or platform.machine() == 'ARM64':
    runtime_suffix = '-arm64'

config = os.environ.get('CONFIG', 'Release')
framework = os.environ.get('FRAMEWORK', 'net8.0')
output_dir = os.environ.get('OUTPUT_DIR', f'./dist/{config}/{framework}/{runtime_prefix}{runtime_suffix}')

print(f'Build Options:\n  Configuration: {config}\n  Framework: {framework}\n  Runtime: {runtime_prefix}{runtime_suffix}\n  Output Directory: {output_dir}')
print('Publishing...')

subprocess.run(['dotnet','publish',
                '-c', config,
                '-f', framework,
                '-r', f'{runtime_prefix}{runtime_suffix}',
                '-o', output_dir,
                '--disable-build-servers', '--force', '--nologo'], check=True)