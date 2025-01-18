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
    node_c = ndn.net.get('c')
    node_d = ndn.net.get('d')

    # 获取consumer和producer的绝对路径
    consumer_path = os.path.abspath('./apps/SConsumer')
    producer_path = os.path.abspath('./apps/MProducer')

    # 在节点b上启动producer程序
    info('Starting Producer on node b\n')
    producer = Application(node_b)
    producer.start(producer_path + ' 1' ,'Mproducer.log')  



    info('Starting Producer on node c\n')
    producer = Application(node_c)
    producer.start(producer_path + ' 2', 'Mproducer.log') 


    info('Starting Producer on node d\n')
    producer = Application(node_d)
    producer.start(producer_path + ' 3', 'Mproducer.log') 

    
      # 在节点b上广告前缀
    for i in range(0, 3):
        prefix = "/producer_1/iteration_{}".format(i)
        node_b.cmd('nlsrc advertise {}'.format(prefix))
        sleep(2)  # 等待路由信息传播
    for i in range(0, 3):
        prefix = "/producer_2/iteration_{}".format(i)
        node_c.cmd('nlsrc advertise {}'.format(prefix))
        sleep(2)  # 等待路由信息传播
    for i in range(0, 3):
        prefix = "/producer_3/iteration_{}".format(i)
        node_d.cmd('nlsrc advertise {}'.format(prefix))
        sleep(2)  # 等待路由信息传播        
    # sleep(5)

    # 在节点a上启动consumer程序
    info('Starting Consumer on node a\n')
    consumer = Application(node_a)
    consumer.start(consumer_path + ' 3', 'consumertest.log')  # 使用绝对路径

    MiniNDNCLI(ndn.net)

    # 停止consumer和producer程序
    consumer.stop()
    producer.stop()

    ndn.stop()

