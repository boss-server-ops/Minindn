import argparse
import os

def generate_conf(k, bw, loss):
    nodes = []
    links = []
    
    # 根节点
    nodes.append("con0:_")
    
    # 计算节点数量
    total_agg_nodes = sum(2**i for i in range(1, k-1))
    total_pro_nodes = 2**(k-1)
    
    # 添加中间层和叶节点
    for i in range(total_agg_nodes):
        nodes.append(f"agg{i}:_")
    
    for i in range(total_pro_nodes):
        nodes.append(f"pro{i}:_")
    
    # 生成链接
    node_queue = ["con0"]  # 使用队列来管理节点遍历
    next_agg = 0
    next_pro = 0
    
    for i in range(k-1):  # 处理每一层
        level_size = len(node_queue)
        for j in range(level_size):  # 处理当前层的每个节点
            current = node_queue.pop(0)
            
            # 确定子节点类型和名称
            if i < k-2:  # 非叶子层，连接到聚合器
                left = f"agg{next_agg}"
                next_agg += 1
                right = f"agg{next_agg}"
                next_agg += 1
            else:  # 叶子层，连接到生产者
                left = f"pro{next_pro}"
                next_pro += 1
                right = f"pro{next_pro}"
                next_pro += 1
            
            # 添加链接
            links.append(f"{current}:{left} bw={bw} delay=0 max_queue_size=10000 loss={loss}")
            links.append(f"{current}:{right} bw={bw} delay=0 max_queue_size=10000 loss={loss}")
            
            # 将子节点添加到队列中，为下一层做准备
            if i < k-2:
                node_queue.append(left)
                node_queue.append(right)
    
    conf = "[nodes]\n" + "\n".join(nodes) + "\n\n[links]\n" + "\n".join(links) + "\n\n# Generated by script"
    return conf

def generate_test_script():
    return """import os
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
    
    info('Starting NFD\\n')
    AppManager(ndn, ndn.net.hosts, Nfd)
    info('Starting NLSR\\n')
    AppManager(ndn, ndn.net.hosts, Nlsr)
    sleep(20)
    
    # 获取节点
    consumer = ndn.net['con0']
    aggregators = [h for h in ndn.net.hosts if h.name.startswith('agg')]
    producers = [h for h in ndn.net.hosts if h.name.startswith('pro')]
    
    # 启动生产者
    producer_path = os.path.abspath('./putapps/producer')
    for pro in producers:
        info(f'Starting Producer {pro.name}\\n')
        Application(pro).start(f'{producer_path} --prefix /{pro.name}', f'{pro.name}.log')
        sleep(5)
    
    # 启动聚合器
    agg_path = os.path.abspath('./aggapps/aggregator')
    for agg in aggregators:
        info(f'Starting Aggregator {agg.name}\\n')
        Application(agg).start(f'{agg_path} --prefix /{agg.name}', f'{agg.name}.log')
        sleep(5)
    
    # 通告路由
    for node in producers + aggregators:
        node.cmd(f'nlsrc advertise /{node.name}')
        sleep(2)
    
    # 启动消费者
    info('Starting Consumer\\n')
    consumer_path = os.path.abspath('./catapps/consumer')
    Application(consumer).start(consumer_path, 'consumer.log')
    
    sleep(40)
    MiniNDNCLI(ndn.net)
    ndn.stop()
"""

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="生成满二叉树拓扑和测试脚本")
    parser.add_argument('layers', type=int, help="树的层数（如3层生成4个叶节点）")
    parser.add_argument('--bw', type=int, default=100, help="链路带宽（默认100Mbps）")
    parser.add_argument('--loss', type=float, default=0.0, help="链路丢包率（默认0.0）")
    args = parser.parse_args()
    
    k = args.layers
    bw = args.bw
    loss = args.loss
    if k < 2:
        print("层数必须大于等于2")
        exit(1)
    
    if loss < 0 or loss > 1:
        print("丢包率必须在0到1之间")
        exit(1)
    
    # 生成配置文件
    conf = generate_conf(k, bw, loss)
    with open(f'binary_tree_{k}_bw{bw}_loss{loss}.conf', 'w') as f:
        f.write(conf)
    
    # 生成测试脚本
    os.makedirs('../examples', exist_ok=True)
    with open('../examples/BinaryTest.py', 'w') as f:
        f.write(generate_test_script())
    
    print(f"生成成功！配置文件: binary_tree_{k}_bw{bw}_loss{loss}.conf，测试脚本: ../examples/BinaryTest.py")