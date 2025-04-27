# -*- Mode:python; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
#
# Copyright (C) 2015-2020, The University of Memphis,
#                          Arizona Board of Regents,
#                          Regents of the University of California.
#
# This file is part of Mini-NDN.
# See AUTHORS.md for a complete list of Mini-NDN authors and contributors.
#
# Mini-NDN is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Mini-NDN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Mini-NDN, e.g., in COPYING.md file.
# If not, see <http://www.gnu.org/licenses/>.

from mininet.log import setLogLevel, info

from minindn.minindn import Minindn
from minindn.util import MiniNDNCLI
from minindn.apps.app_manager import AppManager
from minindn.apps.nfd import Nfd
from minindn.apps.nlsr import Nlsr

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
    node_producer = ndn.net.get('pro0')
    # 修改Resnet18.py中的代码
    # 找到可用节点
    info('可用节点列表: %s\n' % [h.name for h in ndn.net.hosts])

    # 使用第一个可用节点
    node_producer = ndn.net.hosts[0]
    info('使用节点: %s\n' % node_producer.name)

    # 使用完整路径运行Python
    python_cmd = "/usr/bin/python"  # 系统Python路径

    node_producer.cmd(f'cd /home/dd/mini-ndn/resnet18_project && {python_cmd} main.py > producer.log 2>&1 &')
    # node_producer.cmd('python main.py > producer.log 2>&1')
    MiniNDNCLI(ndn.net)

    ndn.stop()
