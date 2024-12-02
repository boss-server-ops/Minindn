# written by Yixiang Zhu


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
    sleep(60)  #wait for routing convergence

    node_a = ndn.net.get('a')
    node_b = ndn.net.get('b')

    # 获取consumer和producer的绝对路径
    consumer_path = os.path.abspath('./apps/consumer_test')
    producer_path = os.path.abspath('./apps/producer_test')

    # 在节点b上启动producer程序
    info('Starting Producer on node b\n')
    producer = Application(node_b)
    producer.start(producer_path, 'producertest.log')  

    producerPrefix = "/example/testApp"
    node_b.cmd('nlsrc advertise {}'.format(producerPrefix))  # 在节点b上广告前缀
    sleep(5)  # 等待路由信息传播
    # sleep(5)

    # 在节点a上启动consumer程序
    info('Starting Consumer on node a\n')
    consumer = Application(node_a)
    consumer.start(consumer_path, 'consumertest.log')  # 使用绝对路径
    node_a.cmd('ps aux | grep consumer')  # 打印进程信息

    MiniNDNCLI(ndn.net)

    # 停止consumer和producer程序
    consumer.stop()
    producer.stop()

    ndn.stop()