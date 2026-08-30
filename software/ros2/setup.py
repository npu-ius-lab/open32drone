from setuptools import find_packages, setup

package_name = 'open32drone_driver'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', ['launch/open32drone.launch.py']),
        ('share/' + package_name + '/config', [
            'config/mavros_plugins.yaml',
            'config/open32drone.rviz',
        ]),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Open32Drone',
    maintainer_email='dev@open32drone.local',
    description='Minimal namespaced Open32Drone MAVROS control and telemetry bridge',
    url='https://github.com/osrbot/osrdrone',
    license='MIT',
    entry_points={
        'console_scripts': [
            'bench_test = open32drone_driver.bench_test:main',
            'flight_test = open32drone_driver.flight_test:main',
            'control = open32drone_driver.control_cli:main',
            'flight_manager = open32drone_driver.flight_manager_node:main',
            'interface_bridge = open32drone_driver.interface_bridge_node:main',
            'offboard_control = open32drone_driver.offboard_control_node:main',
            'rc_bridge = open32drone_driver.rc_bridge_node:main',
            'system = open32drone_driver.system_control:main',
            'ota_upload = open32drone_driver.ota_upload:main',
        ],
    },
)
