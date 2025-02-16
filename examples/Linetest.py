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
from minindn.helpers.ip_routing_helper import IPRoutingHelper

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
    node_pro0 = ndn.net.get('pro0')


    # 获取consumer和producer的绝对路径
    consumer_path = os.path.abspath('./apps/ndn-consumer-INA')
    producer_path = os.path.abspath('./apps/ndn-producer')










        # 在节点b上启动producer程序
    info('Starting Producer on node pro0\n')
    producer0 = Application(node_pro0)
    producer0.start(producer_path + ' /pro0' ,'pro0.log')
    

    # 在节点b上广告前缀

    prefix = "/pro0"
    node_pro0.cmd('nlsrc advertise {}'.format(prefix))
    sleep(2)


    # node_con0.cmd('tcpdump -i con0-eth0 -w ../packets/con0-eth0.pcap &')

    # 必须在广告前缀之后启动consumer程序，否则consumer无法将兴趣包发送到producer
    # info('Starting Consumer on node node_con0\n')
    consumer = Application(node_con0)
    consumer.start(consumer_path, 'con0.log')  
    IPRoutingHelper.calcAllRoutes(ndn.net)


    MiniNDNCLI(ndn.net)

    # 停止consumer和producer程序
    consumer.stop()
    producer0.stop()


    ndn.stop()

