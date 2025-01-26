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

    node_con0 = ndn.net.get('con0')
    node_fwd0 = ndn.net.get('fwd0')
    node_fwd1 = ndn.net.get('fwd1')
    node_agg0 = ndn.net.get('agg0')
    node_pro0 = ndn.net.get('pro0')

    # 获取consumer和producer的绝对路径
    consumer_path = os.path.abspath('./apps/consumer_test')
    producer_path = os.path.abspath('./apps/producer_test')
    aggregator_path = os.path.abspath('./apps/aggregator_test')


    # 在节点b上启动producer程序
    info('Starting Producer on node pro0\n')
    producer = Application(node_pro0)
    producer.start(producer_path + ' /pro0' ,'producertest.log')  

    info('Starting Aggregator on node agg0\n')
    aggregator = Application(node_agg0)
    aggregator.start(aggregator_path + ' /agg0' ,'aggregatortest.log')

    prefix = "/pro0"
    node_pro0.cmd('nlsrc advertise {}'.format(prefix))
    sleep(2)

    prefix = "/agg0"
    node_agg0.cmd('nlsrc advertise {}'.format(prefix))
    sleep(2)
    
    # 在节点a上启动consumer程序
    info('Starting Consumer on node con0\n')
    consumer = Application(node_con0)
    consumer.start(consumer_path + ' /agg0/pro0/data', 'consumertest.log')  

    MiniNDNCLI(ndn.net)

    # 停止consumer和producer程序
    consumer.stop()
    producer.stop()

    ndn.stop()

