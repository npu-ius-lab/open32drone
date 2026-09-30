"""ROS 2 topic exercise isolated from all aircraft command interfaces."""
import argparse
import rclpy
from rclpy.node import Node
from std_msgs.msg import String


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('pub','sub'))
    mode = parser.parse_args().mode
    rclpy.init(args=[])
    node = Node('sample_publisher' if mode == 'pub' else 'sample_subscriber', namespace='course')
    if mode == 'pub':
        publisher = node.create_publisher(String, 'sample', 10)
        count = 0
        def publish():
            nonlocal count
            publisher.publish(String(data=f'seq={count}'))
            count += 1
        node.create_timer(0.1, publish)
    else:
        node.create_subscription(String, 'sample', lambda msg: node.get_logger().info(msg.data), 10)
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
