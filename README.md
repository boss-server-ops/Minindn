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


# apps
1. catapps implements the consumer functionality, putapps implements the producer functionality, and aggapps serves as the aggregator.
2. In the context of gradient synchronization, mmconsumer acts as the consumer, while mmproducer serves as the producer. However, in the experiments, the roles of the consumer and producer are reversed during gradient synchronization compared to aggregation. Therefore, their positions have been swapped in the network topology.

# experiment setup
1. The CIFAR-10 dataset is used in the experiments.
The resnet18_project directory contains the required requirements.txt file for setting up the experimental environment.
It is important to ensure that the nodes on MiniNDN use the same Python environment as the local machine; otherwise, errors related to missing packages may occur.
2. The configurations for the consumer, producer, and aggregator are located in the experiments directory.
To use them, you need to move the relevant files to the designated working directory.

# examples
The examples directory contains MiniNDN scripts used to start MiniNDN processes and to run applications on each node.
Specifically for this project, these applications refer to the executable files compiled from each type of app.

# script
1. There are several *auto.py scripts in the main directory.
These scripts are designed to help automatically run multiple sets of experiments under different network topologies and link parameters.
2. The kill_process.sh script is used to terminate the consumer and other application processes,
because stopping a MiniNDN script does not necessarily terminate the running application processes on the nodes.
