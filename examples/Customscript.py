import os

def parse_conf(conf_path):
    producers = []
    with open(conf_path, 'r') as conf_file:
        lines = conf_file.readlines()
        for line in lines:
            if line.startswith('pro'):
                producers.append(line.split(':')[0])
    return producers

def generate_script(producers, output_path):
    with open(output_path, 'w') as script_file:
        script_file.write("""\
# written by Script Generator

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

    info('Starting NFD on nodes\\n')
    nfds = AppManager(ndn, ndn.net.hosts, Nfd)
    info('Starting NLSR on nodes\\n')
    nlsrs = AppManager(ndn, ndn.net.hosts, Nlsr)
    sleep(20)  # wait for routing convergence

    node_con0 = ndn.net.get('con0')
""")
        for producer in producers:
            script_file.write(f"    node_{producer} = ndn.net.get('{producer}')\n")

        script_file.write("""
    # 获取consumer和producer的绝对路径
    consumer_path = os.path.abspath('./catapps/consumer')
    producer_path = os.path.abspath('./putapps/producer')
""")

        for producer in producers:
            script_file.write(f"""
    info('Starting Producer on node {producer}\\n')
    producer_{producer} = Application(node_{producer})
    producer_{producer}.start(producer_path + ' --prefix /{producer}' ,'producer.log')
    sleep(10)
""")

        for producer in producers:
            script_file.write(f"""
    prefix = "/{producer}"
    node_{producer}.cmd('nlsrc advertise {{}}'.format(prefix))
    sleep(2)
""")

        script_file.write("""
    # 在节点con0上启动consumer程序
    info('Starting Consumer on node con0\\n')
    consumer = Application(node_con0)
    consumer.start(consumer_path, 'consumer.log')

    sleep(300)

    MiniNDNCLI(ndn.net)

    # 停止consumer和producer程序
    consumer.stop()
""")

        for producer in producers:
            script_file.write(f"    producer_{producer}.stop()\n")

        script_file.write("""
    ndn.stop()
""")

if __name__ == '__main__':
    conf_path = '../topologies/Customtest.conf'
    output_path = './GeneratedLinetest.py'
    producers = parse_conf(conf_path)
    generate_script(producers, output_path)
    print(f"Script generated at {output_path}")