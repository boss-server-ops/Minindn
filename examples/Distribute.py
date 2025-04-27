#!/usr/bin/env python3
# 点云传输脚本 - 支持动态识别多个节点

import os
import re  # 添加re模块用于解析缓存统计
from time import sleep
from mininet.log import setLogLevel, info

from minindn.minindn import Minindn
from minindn.util import MiniNDNCLI
from minindn.apps.app_manager import AppManager
from minindn.apps.nfd import Nfd
from minindn.apps.nlsr import Nlsr
from minindn.apps.application import Application

# 添加收集缓存统计的函数
def collect_cache_stats(node):
    """收集节点的缓存统计信息"""
    result = node.cmd('nfdc cs info')
    
    # 解析命中率数据 - 根据NFD输出格式调整
    hits = re.search(r'Hits: (\d+)', result)
    if not hits:
        hits = re.search(r'nHits=(\d+)', result)  # 尝试替代格式
        
    misses = re.search(r'Misses: (\d+)', result)
    if not misses:
        misses = re.search(r'nMisses=(\d+)', result)  # 尝试替代格式
    
    if hits and misses:
        hits = int(hits.group(1))
        misses = int(misses.group(1))
        hit_rate = hits / (hits + misses) if (hits + misses) > 0 else 0
        return hits, misses, hit_rate
    return 0, 0, 0

if __name__ == '__main__':
    setLogLevel('info')

    Minindn.cleanUp()
    Minindn.verifyDependencies()

    # 使用我们的PointCloud.conf配置文件
    ndn = Minindn()

    ndn.start()

    info('Starting NFD on nodes\n')
    nfds = AppManager(ndn, ndn.net.hosts, Nfd)
    info('Starting NLSR on nodes\n')
    nlsrs = AppManager(ndn, ndn.net.hosts, Nlsr)
    info('等待路由收敛...\n')
    sleep(60)  # 等待路由收敛

    # 动态配置所有节点的缓存
    for host in ndn.net.hosts:
        host.cmd('nfdc cs config serve on')  # 确保缓存服务开启
        host.cmd('nfdc cs config capacity 30000')  # 设置缓存大小

    # 动态识别所有节点类型
    consumers = [h for h in ndn.net.hosts if h.name.startswith('con')]
    producers = [h for h in ndn.net.hosts if h.name.startswith('pro')]
    forwarders = [h for h in ndn.net.hosts if h.name.startswith('fwd')]
    
    info(f'发现 {len(consumers)} 个消费者节点\n')
    info(f'发现 {len(producers)} 个生产者节点\n')
    info(f'发现 {len(forwarders)} 个转发节点\n')

    # 清除所有节点的缓存以获得准确的测量结果
    info('清除所有节点的内容存储...\n')
    for host in ndn.net.hosts:
        host.cmd('nfdc cs erase all')

    # 获取consumer和producer的绝对路径
    consumer_path = os.path.abspath('./mmconsumer/consumer-0')
    producer_path = os.path.abspath('./mmproducer/producer')

    # 启动所有生产者
    producer_apps = []
    for i, pro in enumerate(producers):
        info(f'Starting Producer on node {pro.name}\n')
        app = Application(pro)
        app.start(producer_path + f' --prefix /PointCloudService --datasetId {i+1}', f'{pro.name}.log')
        producer_apps.append(app)
        sleep(10)
        
        # 注册前缀
        pro.cmd('nlsrc advertise /PointCloudService')
        sleep(2)

    # 启动所有消费者
    consumer_apps = []
    for i, con in enumerate(consumers):
        info(f'Starting Consumer on node {con.name}\n')
        app = Application(con)
        app.start(consumer_path, f'{con.name}.log')
        consumer_apps.append(app)
        sleep(5)

    # 等待足够长的时间确保有缓存操作发生
    info('系统运行中，收集缓存命中率...\n')
    sleep(100)  # 增加到5分钟，让缓存有足够时间发生

    # 收集缓存命中率数据
    info('收集缓存命中率统计数据...\n')
    total_hits = 0
    total_misses = 0
    
    # 查看缓存状态和路由表，帮助诊断问题
    for host in forwarders:
        info(f"\n节点 {host.name} 的路由表:\n")
        info(host.cmd('nfdc route list') + "\n")
        
        info(f"节点 {host.name} 的缓存配置:\n")
        info(host.cmd('nfdc cs config') + "\n")
    
    # 收集缓存统计
    for host in ndn.net.hosts:
        hits, misses, hit_rate = collect_cache_stats(host)
        total_hits += hits
        total_misses += misses
        info(f"节点 {host.name} 缓存命中率: {hit_rate*100:.2f}%\n")
        info(f"  - 命中: {hits}, 未命中: {misses}\n")
    
    # 计算总体命中率
    overall_hit_rate = total_hits / (total_hits + total_misses) if (total_hits + total_misses) > 0 else 0
    info(f"\n总体缓存命中率: {overall_hit_rate*100:.2f}%\n")
    info(f"总命中次数: {total_hits}, 总未命中次数: {total_misses}\n\n")

    MiniNDNCLI(ndn.net)

    # 停止所有应用程序
    for app in consumer_apps:
        app.stop()
    
    for app in producer_apps:
        app.stop()

    # 停止MiniNDN
    ndn.stop()