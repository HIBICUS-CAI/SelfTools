import datetime, subprocess, os, time, re

def sys(args: list[str]) -> tuple[int, list[str], list[str]]:
	result = subprocess.run(list(args), capture_output=True, text=True)
	return (result.returncode, result.stdout.splitlines(), result.stderr.splitlines())

def sout(*args) -> str:
	(_, stdout, _) = sys(list(args))
	return '\n'.join(stdout)

def cap(pattern: str, content: str, group_index: int=None) -> str:
	result = re.compile(pattern).search(content)
	if group_index == None:
		group_index = 1
	return result.group(group_index)

fan_rpm_file = ''
for i in range(0, 5):
	fan_rpm_file = f'/sys/devices/platform/cooling_fan/hwmon/hwmon{i}/fan1_input'
	if os.path.isfile(fan_rpm_file):
		break

try:
	while True:
		base_args = 'vcgencmd'

		temp_pattern = r'temp=(.+)\'C'
		temp = cap(temp_pattern, sout(base_args, 'measure_temp'))

		clock_pattern = r'frequency\(0\)=(.+)'
		cpu_c = float(cap(clock_pattern, sout(base_args, 'measure_clock', 'arm'))) / 1000000.0
		gpu_c = float(cap(clock_pattern, sout(base_args, 'measure_clock', 'core'))) / 1000000.0
		v3d_c = float(cap(clock_pattern, sout(base_args, 'measure_clock', 'v3d'))) / 1000000.0
		emmc_c = float(cap(clock_pattern, sout(base_args, 'measure_clock', 'emmc'))) / 1000000.0

		volt_pattern = r'volt=(.+)V'
		cpu_v = cap(volt_pattern, sout(base_args, 'measure_volts', 'core'))
		sdram_core_v = cap(volt_pattern, sout(base_args, 'measure_volts', 'sdram_c'))
		sdram_io_v = cap(volt_pattern, sout(base_args, 'measure_volts', 'sdram_i'))
		sdram_phy_v = cap(volt_pattern, sout(base_args, 'measure_volts', 'sdram_p'))

		throttled_pattern = r'throttled=0x(.+)'
		throttled_flag = int(cap(throttled_pattern, sout(base_args, 'get_throttled')), 16)
		throttled_flag_desc = [
			(0x1, 'undervoltage detected'),			(0x10000, 'undervoltage has occurred'),
			(0x2, 'arm frequency capped'),			(0x20000, 'arm frequency capping has occurred'),
			(0x4, 'currently throttled'),			(0x40000, 'throttling has occurred'),
			(0x8, 'soft temperature limit active'),	(0x80000, 'soft temperature limit has occurred'),
		]

		fan_rpm = 0
		with open(fan_rpm_file) as f:
			fan_rpm = f.read().rstrip()

		msg = f'''
{datetime.datetime.now()}

[Temperature]	InternalSensor	{temp}		'C
		FanSpeed	{fan_rpm}		rpm

[CPU]		Clock		{cpu_c}	MHz
		Voltage		{cpu_v}		V

[GPU]		Clock		{gpu_c}	MHz
		V3D.Clock	{v3d_c}	MHz

[SDRAM]		Voltage		{sdram_core_v}		V
		IO.Voltage	{sdram_io_v}		V
		Phy.Voltage	{sdram_phy_v}		V

[EMMC]		Clock		{emmc_c}	MHz

[System]	ThrottledState	{hex(throttled_flag)}
'''
		
		for flag in throttled_flag_desc:
			if throttled_flag & flag[0]:
				msg += f'\t\t\t\t{flag[1]}.\n'

		os.system('clear')
		print(msg)
		time.sleep(1)
except KeyboardInterrupt:
	pass