import os
import argparse
from time import sleep
from mininet.log import setLogLevel, info
from minindn.minindn import Minindn
from minindn.util import MiniNDNCLI
from minindn.apps.app_manager import AppManager
from minindn.apps.nfd import Nfd
from minindn.apps.nlsr import Nlsr
from minindn.apps.application import Application

if __name__ == '__main__':
    # 解析命令行参数
    parser = argparse.ArgumentParser(description='运行ISP网络实验')
    parser.add_argument('--runtime', type=int, default=600,
                      help='实验运行时间（秒，默认600）')
    parser.add_argument('--work-dir', default='./isp-workdir',
                      help='工作目录')
    parser.add_argument('--agg-loss', type=float, default=0.01,
                      help='汇聚层丢包率覆盖（默认0.01%）')
    parser.add_argument('--fwd-loss', type=float, default=0.01,
                      help='转发层丢包率覆盖（默认0.01%）')
    parser.add_argument('--pro-loss', type=float, default=0.01,
                      help='生产者层丢包率覆盖（默认0.01%）')
    args = parser.parse_args()
    
    setLogLevel('info')
    Minindn.cleanUp()
    Minindn.verifyDependencies()
    ndn = Minindn()
    ndn.start()
    
    info('网络丢包率配置:\n')
    info(f'汇聚层丢包率: {args.agg_loss}%\n')
    info(f'转发层丢包率: {args.fwd_loss}%\n')
    info(f'生产者层丢包率: {args.pro_loss}%\n')
    
    # 启动基础服务
    info('Starting NFD\n')
    AppManager(ndn, ndn.net.hosts, Nfd)
    info('Starting NLSR\n')
    AppManager(ndn, ndn.net.hosts, Nlsr)
    sleep(30)  # 延长路由收敛时间
    
    # 节点分类
    consumer = ndn.net['con0']  # 核心节点兼消费者
    aggregators = [h for h in ndn.net.hosts if h.name.startswith('agg')]
    forwarders = [h for h in ndn.net.hosts if h.name.startswith('fwd')]
    producers = [h for h in ndn.net.hosts if h.name.startswith('pro')]
    
    # 启动生产者
    producer_path = os.path.abspath('./putapps/producer')
    for pro in producers:
        info(f'Starting Producer {pro.name}\n')
        Application(pro).start(
            f'{producer_path} --prefix /{pro.name}', 
            f'{pro.name}.log'
        )
        sleep(1)
    
    # 启动汇聚层服务
    aggregator_path = os.path.abspath('./aggapps/aggregator')
    for agg in aggregators:
        info(f'Starting Aggregator {agg.name}\n')
        Application(agg).start(
            f'{aggregator_path} --prefix /{agg.name}',
            f'{agg.name}.log'
        )
        sleep(1)
    
    # 配置核心节点缓存策略
    info(f'Configuring Core {consumer.name}\n')
    consumer.cmd('nfd-strategy-set / /localhost/nfd/strategy/best-route')
    consumer.cmd('nfdc cs config capacity 100000')  # 设置大缓存容量
    
    # 通告路由
    info('Advertising routes\n')
    for node in producers + aggregators + forwarders:
        node.cmd(f'nlsrc advertise /{node.name}')
        sleep(0.5)
    
    # 启动消费者(就是con0)
    info(f'Starting Consumer on {consumer.name}\n')
    consumer_path = os.path.abspath('./catapps/consumer')
    Application(consumer).start(consumer_path, 'consumer.log')
    
    # 实验运行时间
    info(f'实验将运行 {args.runtime} 秒...\n')
    sleep(args.runtime)
    MiniNDNCLI(ndn.net)
    ndn.stop()
