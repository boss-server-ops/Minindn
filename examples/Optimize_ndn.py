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

    node_user = ndn.net.get('user')
    node_fwd = ndn.net.get('fwd')
    node_producer = ndn.net.get('producer')
    node_optimizer = ndn.net.get('optimizer')

    user_path = os.path.abspath('./yulongapp/ndn_client.py')
    forwarder_path = os.path.abspath('./yulongapp/ndn-forwarder.py')
    producer_path = os.path.abspath('./yulongapp/ndn-producer.py')
    optimizer_path = os.path.abspath('./yulongapp/ndn-optimizer.py')

    python_path = '../pythonenv/yulong/bin/python'  # 指定Python解释器路径

    # 启动程序时使用指定的Python解释器
    node_user.cmd('{} {}'.format(python_path, user_path), 'user.log')
    node_fwd.cmd('{} {}'.format(python_path, forwarder_path), 'forwarder.log')
    node_producer.cmd('{} {}'.format(python_path, producer_path), 'producer.log')
    node_optimizer.cmd('{} {}'.format(python_path, optimizer_path), 'optimizer.log')

    # 发布prefix
    prefix = "/ndn/video/content"
    node_producer.cmd('nlsrc advertise {}'.format(prefix))
    sleep(2)
    prefix = "/ndn/video"
    node_fwd.cmd('nlsrc advertise {}'.format(prefix))
    sleep(2)
    prefix = "/ndn/opt"
    node_optimizer.cmd('nlsrc advertise {}'.format(prefix))
    sleep(2)

    # 启动用户程序
    user = Application(node_user)
    user.start('{} {}'.format(python_path, user_path), 'user.log')

    MiniNDNCLI(ndn.net)

    # 停止consumer和producer程序
    ndn.stop()
