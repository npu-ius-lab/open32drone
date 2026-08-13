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
        ('share/' + package_name + '/launch', ['launch/open32drone_mavros.launch.py']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Open32Drone',
    maintainer_email='dev@open32drone.local',
    description='Open32Drone 驱动包：MJPEG 图传接收 + MAVROS 飞控连接（open32drone_v3 固件）',
    license='MIT',
    entry_points={
        'console_scripts': [
            'camera = open32drone_driver.mjpeg_camera_node:main',
        ],
    },
)
