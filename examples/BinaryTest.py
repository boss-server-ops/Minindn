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
    
    info('Starting NFD\n')
    AppManager(ndn, ndn.net.hosts, Nfd)
    info('Starting NLSR\n')
    AppManager(ndn, ndn.net.hosts, Nlsr)
    sleep(20)
    
    # 获取节点
    consumer = ndn.net['con0']
    aggregators = [h for h in ndn.net.hosts if h.name.startswith('agg')]
    producers = [h for h in ndn.net.hosts if h.name.startswith('pro')]
    
    # 启动生产者
    producer_path = os.path.abspath('./putapps/producer')
    for pro in producers:
        info(f'Starting Producer {pro.name}\n')
        Application(pro).start(f'{producer_path} --prefix /{pro.name}', f'{pro.name}.log')
        sleep(5)
    
    # 启动聚合器
    agg_path = os.path.abspath('./aggapps/aggregator')
    for agg in aggregators:
        info(f'Starting Aggregator {agg.name}\n')
        Application(agg).start(f'{agg_path} --prefix /{agg.name}', f'{agg.name}.log')
        sleep(5)
    
    # 通告路由
    for node in producers + aggregators:
        node.cmd(f'nlsrc advertise /{node.name}')
        sleep(2)
    
    # 启动消费者
    info('Starting Consumer\n')
    consumer_path = os.path.abspath('./catapps/consumer')
    Application(consumer).start(consumer_path, 'consumer.log')
    
    sleep(300)
    MiniNDNCLI(ndn.net)
    ndn.stop()
