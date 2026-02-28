import os, subprocess, argparse, github, shutil, platform

parser = argparse.ArgumentParser(description='Build DXC from source.')
parser.add_argument('-S', '--source-dir', required=True, help='Path to the DXC source directory.')
parser.add_argument('-B', '--build-dir', required=True, help='Path to the build output directory.')
parser.add_argument('-I', '--install-dir', required=True, help='Path to the installation directory, package will be installed here with a folder named as the target version.')
parser.add_argument('-T', '--target-version', default='latest', help='Target version for the build, latest or a specific tag, default is latest.')
parser.add_argument('--dxv', action='store_true', default=True, help='Build with DXV.')
args = parser.parse_args()

source_dir = os.path.abspath(args.source_dir)
build_dir = os.path.abspath(args.build_dir)
install_dir = os.path.abspath(args.install_dir)

tag_name = args.target_version
if args.target_version == 'latest':
    tag_name = github.Github().get_repo('microsoft/DirectXShaderCompiler').get_latest_release().tag_name
print(f'Building DXC version: {tag_name}')

if not os.path.exists(source_dir):
    print(f'Cloning DXC source to {source_dir}')
    subprocess.run(['git', 'clone', 'https://github.com/microsoft/DirectXShaderCompiler.git', source_dir], check=True)
else:
    print(f'Using existing DXC source at {source_dir} and updating to latest')
    subprocess.run(['git', 'fetch', '--all', '--tags'], cwd=source_dir, check=True)
print(f'Checking out tag: {tag_name}')
subprocess.run(['git', 'checkout', tag_name], cwd=source_dir, check=True)
subprocess.run(['git', 'submodule', 'update', '--init', '--recursive'], cwd=source_dir, check=True)

cmake_config_args = [
    'cmake',
    '-S', f'{source_dir}',
    '-B', f'{build_dir}',
    '-C', f'{source_dir}/cmake/caches/PredefinedParams.cmake',
    '-DCMAKE_BUILD_TYPE=Release',
    '-DHLSL_OFFICIAL_BUILD=ON',
]

cmake_build_args : list[str] = [
    'cmake',
    '--build', build_dir,
    '--target', 'dxc',
]

cmake_build_headers_args : list[str] = [
    'cmake',
    '--install', build_dir,
    '--prefix', os.path.join(build_dir, 'header_install'),
    '--component', 'dxc-headers',
]

if args.dxv:
    cmake_build_args.append('dxv')
    cmake_build_args.append('dxildll')

if platform.system() == 'Windows':
    cmake_config_args.append('-A')
    cmake_config_args.append('x64')
    cmake_build_args.append('--config')
    cmake_build_args.append('Release')
elif platform.system() == 'Linux':
    cmake_config_args.append('-G')
    cmake_config_args.append('Ninja')
    cmake_config_args.append('-DCMAKE_CXX_COMPILER=clang++')
    cmake_config_args.append('-DCMAKE_C_COMPILER=clang')


print(f'Preparing build directory at {build_dir}')
if os.path.exists(build_dir):
    shutil.rmtree(build_dir)
os.makedirs(build_dir)
if platform.system() == 'Windows':
    print('Generating TAEF...')
    hlsl_env = os.environ.copy()
    hlsl_env['HLSL_SRC_DIR'] = source_dir
    subprocess.run(['py', f'{source_dir}/utils/hct/hctgettaef.py'], check=True, env=hlsl_env)
print('Configuring the build with CMake...')
subprocess.run(cmake_config_args, check=True)
print('Building DXC...')
subprocess.run(cmake_build_args, check=True)
print('Installing headers...')
subprocess.run(cmake_build_headers_args, check=True)
print('Installing headers manually (copying ErrorCodes.h and hlsl headers) ...')
os.makedirs(os.path.join(build_dir, 'header_install', 'include', 'dxc', 'Support'), exist_ok=True)
shutil.copyfile(
    os.path.join(source_dir, 'include', 'dxc', 'Support', 'ErrorCodes.h'),
    os.path.join(build_dir, 'header_install', 'include', 'dxc', 'Support', 'ErrorCodes.h'),
)
shutil.copytree(
    os.path.join(source_dir, 'tools', 'clang', 'lib', 'Headers', 'hlsl'),
    os.path.join(build_dir, 'header_install', 'include', 'dxc', 'hlsl'),
)

print('Copying built DXC binaries to dxc_dist directory...')
os.makedirs(os.path.join(install_dir, tag_name), exist_ok=True)
shutil.copytree(
    os.path.join(build_dir, 'header_install', 'include'),
    os.path.join(install_dir, tag_name, 'include'),
)
shutil.copytree(
    os.path.join(build_dir, 'Release' if platform.system() == 'Windows' else '', 'bin'),
    os.path.join(install_dir, tag_name, 'bin'),
    ignore=shutil.ignore_patterns('*.pdb'),
    symlinks=True,
)
os.makedirs(os.path.join(install_dir, tag_name, 'lib'), exist_ok=True)
if platform.system() == 'Windows':
    shutil.copyfile(
        os.path.join(build_dir, 'Release', 'bin', 'dxil.dll'),
        os.path.join(install_dir, tag_name, 'lib', 'dxil.dll')
    )
    shutil.copyfile(
        os.path.join(build_dir, 'Release', 'bin', 'dxcompiler.dll'),
        os.path.join(install_dir, tag_name, 'lib', 'dxcompiler.dll')
    )
    shutil.copyfile(
        os.path.join(build_dir, 'Release', 'lib', 'dxil.lib'),
        os.path.join(install_dir, tag_name, 'lib', 'dxil.lib')
    )
    shutil.copyfile(
        os.path.join(build_dir, 'Release', 'lib', 'dxcompiler.lib'),
        os.path.join(install_dir, tag_name, 'lib', 'dxcompiler.lib')
    )
elif platform.system() == 'Linux':
    shutil.copyfile(
        os.path.join(build_dir, 'lib', 'libdxil.so'),
        os.path.join(install_dir, tag_name, 'lib', 'libdxil.so')
    )
    shutil.copyfile(
        os.path.join(build_dir, 'lib', 'libdxcompiler.so'),
        os.path.join(install_dir, tag_name, 'lib', 'libdxcompiler.so')
    )

print(f'Build and packaging completed!\nOutput available at {os.path.join(install_dir, tag_name)}')