Mini-NDN
========

If you are new to the NDN community of software generally, read the
[Contributor's Guide](https://github.com/named-data/.github/blob/master/CONTRIBUTING.md).

### What is Mini-NDN?

Mini-NDN is a lightweight networking emulation tool that enables testing, experimentation, and
research on the NDN platform based on [Mininet](https://github.com/mininet/mininet).
Mini-NDN uses the NDN libraries, NFD, NLSR, and tools released by the
[NDN project](http://named-data.net/codebase/platform/) to emulate an NDN network on a single system.

Mini-NDN is open and free software licensed under the GPL 3.0 license. Mini-NDN is free to all
users and developers. For more information about licensing details and limitations,
please refer to [COPYING.md](COPYING.md).

The first release of Mini-NDN is developed by members of the NSF-sponsored NDN project team.
Mini-NDN is open to contribution from the public.
For more details, please refer to [AUTHORS.rst](AUTHORS.rst).
Bug reports and feedback are highly appreciated and can be made through our
[Redmine site](http://redmine.nadmed-data.net/projects/mini-ndn) and the
[mini-ndn mailing list](http://www.lists.cs.ucla.edu/mailman/listinfo/mini-ndn).

### Documentation

Please refer to http://minindn.memphis.edu/ or [docs/index.rst](docs/index.rst) for installation, usage, and other documentation.
The documentation can be built using:

    ./docs/build.sh

and is available under `docs/_build/html`.

# Introduction
## apps
1. catapps implements the consumer functionality, putapps implements the producer functionality, and aggapps serves as the aggregator.
2. In the context of gradient synchronization, mmconsumer acts as the consumer, while mmproducer serves as the producer. However, in the experiments, the roles of the consumer and producer are reversed during gradient synchronization compared to aggregation. Therefore, their positions have been swapped in the network topology.

## experiment setup
1. The CIFAR-10 dataset is used in the experiments.
The resnet18_project directory contains the required requirements.txt file for setting up the experimental environment.
It is important to ensure that the nodes on MiniNDN use the same Python environment as the local machine; otherwise, errors related to missing packages may occur.
2. The configurations for the consumer, producer, and aggregator are located in the experiments directory.
To use them, you need to move the relevant files to the designated working directory.

## examples
The examples directory contains MiniNDN scripts used to start MiniNDN processes and to run applications on each node.
Specifically for this project, these applications refer to the executable files compiled from each type of app.

## script
1. There are several *auto.py scripts in the main directory.
These scripts are designed to help automatically run multiple sets of experiments under different network topologies and link parameters.
2. The kill_process.sh script is used to terminate the consumer and other application processes,
because stopping a MiniNDN script does not necessarily terminate the running application processes on the nodes.

## chunkworkdir, dcn-workdir, isp-workdir and so on
这些是“工作目录”，是由自己创建的，当运行脚本的时候，创建的节点的文件，比如日志等，都在该文件夹下。比如sudo python ./examples/mnndn.py --work-dir ./chunkworkdir ./topologies/xx.py（没有实际意义，只是运行脚本的例子）这样就代表将工作目录指定为chunkworkdir，那么运行时的各个节点的状态，日志等都在这些文件夹下了


## aggapps catapps putapps
就分别是aggregator，consumer，producer了，每个app都有一个makefile用来编译得到可执行程序，其实这些app的源文件与minindn本身是无关的，要用到的只有编译得到的可执行文件如consumer，aggregator，producer这些文件，minindn的脚本里在节点上运行的都是这个可执行文件。

## core
ndntool提供的文件夹，包含一些宏定义等，一般不修改。

## dl
包含的是minindn关联的一些工具，不需要修改

## examples
minindn的脚本放置的地方，这个脚本并不是写每个节点的收发行为的地方，而是每个节点的路由管理，以及该运行什么样的应用程序的地方，这个脚本是按顺序执行的，由于有些代码的执行并不能立马执行完，且是非阻塞的，所以有些代码后面需要sleep一定的时间来保证完成。

## experiments
放置的是一些配置文件，只是为了方便放在这，其实没有用到，要使用的话需要将这些配置文件放到工作目录里（自己创建的各种workdir下）。


## topologies
这是拓扑文件（.conf）放的地方，如果不涉及到wifi，一般就只有文件里[nodes]和[links]。nodes就是自己设置的节点的名字，links表示的是连接的方式，两两连接，可以设置的参数有bw（bandwidth），delay等，具体可以参照其他拓扑文件里面的设置方式，如果想看源代码，就得在minindn文件夹下找到minindn.py，在这个文件里有解析拓扑文件的代码

## minindn文件夹
一般也不用改，属于是minindn工具的实现代码，如果只是使用就不用改

## mmconsumer,mmproducer
是MM那篇文章代码的改编版，如果之后还要做梯度同步的操作，建议重写。但是以现在的情况来说,跑实验也没啥问题

## resnet18_project
就是一个普通的训练的project

## Binaryauto.py
测试二叉树拓扑的自动化脚本，可以修改比如节点数量，丢包率，拥塞控制算法等参数，代码的末尾有可以执行DCN拓扑的代码（现在是被注释的状态），这样可以在测试完二叉树拓扑之后自动测试DCN拓扑。当然这种脚本最好重新写，因为测试的拓扑结构不一样。

## DCNauto.py Ringauto.py
同理

## kill_process.sh
这个脚本用来杀死进程，minindn的脚本运行结束，有时候并不意味着每个节点上运行的应用程序都已经停止，所以需要kill来确保确实终止了进程。

## 关于实验experiment
建议直接打开我的chunkworkdir文件夹下的experiment复制一份，因为涉及到大文件，我也没法上传到github上。比如hello.txt是用来传输的文件。

### aggregator*.ini
代表的是aggregator的配置，有两个配置文件，因为aggregator其实是consumer和producer的集合体。

## 1这个文件夹
梯度同步是MM那个实验残留的保存要传输的文件方式，所以要传输的文件就放在1下命名为MetaData作为传输的数据了。

## changechunk.py
这是改变hello.txt大小的脚本，同时也会根据设置的大小自动调整conconfig.ini配置里的chunk的大小和数量。

## hello.txt
是聚合过程中要传输的数据

## conconfig for syn.ini
梯度同步过程的配置文件

## 梯度同步过程
脚本就是examples/Distribute.py,命令行运行示例sudo python ./examples/Distribute.py --work-dir ./chunkworkdir/ ./topologies/Distributed_dcn.conf，如果要用其他拓扑自行替换就好了。现在不用改名配置文件了，只要像chunkworkdir里的experiments那样命名就好了。

## local training
在minindn上运行和直接本地运行没有什么区别，好的能够让minindn里的节点的能够直接用本机相同的环境的方法我也没找到。
