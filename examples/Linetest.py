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
    sleep(20)  #wait for routing convergence

    node_con0 = ndn.net.get('con0')
    node_agg0 = ndn.net.get('agg0')
    node_pro0 = ndn.net.get('pro0')
    node_pro1 = ndn.net.get('pro1')

    # 获取consumer和producer的绝对路径
    consumer_path = os.path.abspath('./catapps/consumer')
    producer_path = os.path.abspath('./putapps/producer')
    aggregator_path = os.path.abspath('./aggapps/aggregator')

    info('Starting Producer on node pro0\n')
    producer = Application(node_pro0)
    producer.start(producer_path + ' --prefix /pro0' ,'producer.log')  
    # sleep(200)
    # sleep(30)
    sleep(10)

    info('Starting Producer on node pro1\n')
    producer = Application(node_pro1)
    producer.start(producer_path + ' --prefix /pro1' ,'producer.log')  
    # sleep(200)
    # sleep(30)
    sleep(10)



    # 在节点agg0上启动producer程序
    info('Starting Aggregator on node agg0\n')
    aggregator = Application(node_agg0)
    aggregator.start(aggregator_path+' --prefix /agg0' , 'aggregator.log')
    sleep(5)





    prefix = "/pro0"
    node_pro0.cmd('nlsrc advertise {}'.format(prefix))
    sleep(2)

    prefix = "/pro1"
    node_pro1.cmd('nlsrc advertise {}'.format(prefix))
    sleep(2)




    prefix = "/agg0"
    node_agg0.cmd('nlsrc advertise {}'.format(prefix))
    sleep(2)



    # node_con0.cmd('tcpdump -i con0-eth0 -w ../packets/con0-eth0.pcap &')
    
    # 在节点a上启动consumer程序
    info('Starting Consumer on node con0\n')
    consumer = Application(node_con0)
    consumer.start(consumer_path, 'consumer.log')  

    sleep(300)

    MiniNDNCLI(ndn.net)

    # 停止consumer和producer程序
    consumer.stop()
    producer.stop()
    aggregator.stop()

    ndn.stop()