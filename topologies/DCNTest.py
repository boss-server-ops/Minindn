import sys
import os
from time import sleep
from mininet.log import setLogLevel, info
from minindn.minindn import Minindn
from minindn.util import MiniNDNCLI
from minindn.apps.app_manager import AppManager
from minindn.apps.nfd import Nfd
from minindn.apps.nlsr import Nlsr

if __name__ == '__main__':
    setLogLevel('info')
    
    # 读取配置文件参数
    config_file = sys.argv[1] if len(sys.argv) > 1 else 'dcn.conf'
    
    # 初始化实验环境
    Minindn.cleanUp()
    ndn = Minindn(configFile=config_file)
    ndn.start()

    # 启动基础服务
    AppManager(ndn, ndn.net.hosts, Nfd)
    AppManager(ndn, ndn.net.hosts, Nlsr)
    sleep(20)  # 等待路由收敛

    # 获取节点实例
    consumer = ndn.net['con0']
    producers = [h for h in ndn.net.hosts if h.name.startswith('pro')]

    # 启动生产者（示例应用）
    for pro in producers:
        pro.cmd('ndnputchunks /%s < /dev/urandom > %s.log 2>&1 &' % (pro.name, pro.name))
        sleep(0.5)

    # 启动消费者测试
    sleep(5)  # 等待生产者就绪
    consumer.cmd('ndnpeek -p /pro0 > consumer.log 2>&1 &')

    # 保持运行（300秒后自动停止）
    sleep(300)
    MiniNDNCLI(ndn.net)
    ndn.stop()
