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
    ndn = Minindn()
    ndn.start()
    
    info('Starting NFD\n')
    AppManager(ndn, ndn.net.hosts, Nfd)
    info('Starting NLSR\n')
    AppManager(ndn, ndn.net.hosts, Nlsr)
    
    # 配置所有节点的缓存
    for host in ndn.net.hosts:
        host.cmd('nfdc cs config serve on')  # 确保缓存服务开启
        host.cmd('nfdc cs config capacity 10000')  # 设置缓存大小
    
    # 清除所有节点的缓存以获得准确的测量结果
    info('清除所有节点的内容存储...\n')
    for host in ndn.net.hosts:
        host.cmd('nfdc cs erase all')
    
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
    
    # 运行一段时间以产生足够的缓存数据
    info('系统运行中，收集缓存命中率数据...\n')
    sleep(300)  # 5分钟，确保有足够的缓存活动
    
    # 收集并显示缓存命中率统计
    info('\n===================== 缓存命中率统计 =====================\n')
    
    # 消费者节点统计
    info('\n消费者节点缓存统计:\n')
    consumer_hits, consumer_misses, consumer_hit_rate = collect_cache_stats(consumer)
    info(f"  节点 {consumer.name} 缓存命中率: {consumer_hit_rate*100:.2f}%\n")
    info(f"    - 命中: {consumer_hits}, 未命中: {consumer_misses}\n")
    
    # 聚合器节点统计
    info('\n聚合器节点缓存统计:\n')
    agg_total_hits = 0
    agg_total_misses = 0
    
    for agg in aggregators:
        hits, misses, hit_rate = collect_cache_stats(agg)
        agg_total_hits += hits
        agg_total_misses += misses
        info(f"  节点 {agg.name} 缓存命中率: {hit_rate*100:.2f}%\n")
        info(f"    - 命中: {hits}, 未命中: {misses}\n")
    
    agg_hit_rate = agg_total_hits / (agg_total_hits + agg_total_misses) if (agg_total_hits + agg_total_misses) > 0 else 0
    info(f"  聚合器节点整体命中率: {agg_hit_rate*100:.2f}%\n")
    
    # 生产者节点统计
    info('\n生产者节点缓存统计:\n')
    pro_total_hits = 0
    pro_total_misses = 0
    
    for pro in producers:
        hits, misses, hit_rate = collect_cache_stats(pro)
        pro_total_hits += hits
        pro_total_misses += misses
        info(f"  节点 {pro.name} 缓存命中率: {hit_rate*100:.2f}%\n")
        info(f"    - 命中: {hits}, 未命中: {misses}\n")
    
    pro_hit_rate = pro_total_hits / (pro_total_hits + pro_total_misses) if (pro_total_hits + pro_total_misses) > 0 else 0
    info(f"  生产者节点整体命中率: {pro_hit_rate*100:.2f}%\n")
    
    # 计算总体命中率
    total_hits = consumer_hits + agg_total_hits + pro_total_hits
    total_misses = consumer_misses + agg_total_misses + pro_total_misses
    overall_hit_rate = total_hits / (total_hits + total_misses) if (total_hits + total_misses) > 0 else 0
    
    info(f"\n总体缓存命中率: {overall_hit_rate*100:.2f}%\n")
    info(f"总命中次数: {total_hits}, 总未命中次数: {total_misses}\n")
    info('==========================================================\n')
    
    # 运行CLI
    MiniNDNCLI(ndn.net)
    ndn.stop()