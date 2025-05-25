# aggtree.hpp & aggtree.cpp
是有关聚合树的部分，读取拓扑然后构造这个兴趣的名称，全部是自己写的。构造的名称方式是agg0(pro0+pro1)这样的，括号里代表的就是agg0的子节点

# splitter
main函数创建的对象就是splitter，算是main函数与核心代码的连接的桥梁。其实是对ndntool里的ndnchunk（现改名为ndnget）的代码的结构的模仿。


# split-interests
基类，主要是run和ondata。

# split-interests-adaptive
核心代码，包括分配不同的face（face数量在配置文件中指定，虽然用多face的方式和之前比有好转，但是face太多也不行）以及发送初始的initdata也在该类中实现。