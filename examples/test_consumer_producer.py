import os
from time import sleep
from mininet.log import setLogLevel, info

from minindn.minindn import Minindn
from minindn.util import MiniNDNCLI
from minindn.apps.app_manager import AppManager
from minindn.apps.nfd import Nfd
from minindn.apps.nlsr import Nlsr
from minindn.apps.application import Application

if __name__ == '__main__':
    setLogLevel('info')

    Minindn.cleanUp()
    Minindn.verifyDependencies()

    ndn = Minindn()

    ndn.start()

    info('Starting NFD on nodes\n')
    nfds = AppManager(ndn, ndn.net.hosts, Nfd)
    info('Starting NLSR on nodes\n')
    nlsrs = AppManager(ndn, ndn.net.hosts, Nlsr)
    sleep(90)  # 等待路由收敛

    # 获取节点a和b
    node_a = ndn.net.get('a')
    node_b = ndn.net.get('c')

    # 获取consumer和producer的绝对路径
    consumer_path = os.path.abspath('./apps/consumer')
    producer_path = os.path.abspath('./apps/producer')
    print(f'Consumer absolute path: {consumer_path}')
    print(f'Producer absolute path: {producer_path}')

    # 在节点b上启动producer程序
    info('Starting Producer on node b\n')
    producer = Application(node_b)
    producer.start(producer_path, 'producer.log')  # 使用绝对路径
    node_b.cmd('ps aux | grep producer')  # 打印进程信息

    producerPrefix = "/example/testApp"
    node_b.cmd('nlsrc advertise {}'.format(producerPrefix))  # 在节点b上广告前缀
    sleep(5)  # 等待路由信息传播

    # 检查节点b的NLSR日志文件
    info('Checking NLSR log on node b\n')
    print(node_b.cmd('cat /tmp/minindn/b/log/nlsr.log'))

    # 在节点a上启动consumer程序
    info('Starting Consumer on node a\n')
    consumer = Application(node_a)
    consumer.start(consumer_path, 'consumer.log')  # 使用绝对路径
    node_a.cmd('ps aux | grep consumer')  # 打印进程信息

    MiniNDNCLI(ndn.net)

    # 停止consumer和producer程序
    consumer.stop()
    producer.stop()

    ndn.stop()