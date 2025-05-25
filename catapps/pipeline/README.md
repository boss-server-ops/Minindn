这个文件夹内的代码大部分就是ndntools的了，是在ndntools上修改的代码，建议先看懂原来的ndntools的ndnchunk（ndnget）里的consumer的代码。

# statics-collector
暂时没用上，原本是用来收集一些数据的类，但是在新代码中要用比较麻烦，对代码本身的也没啥影响就没用上

# pipeliner
在原来的ndntool的代码里是consumer

# options.hpp
参数类，用来创建m_option对象传递给其他类，在main.cpp中读取配置文件初始化。

# pipeline-interests
基类，基本没改过

# data-fetcher discover-version
基本没改过

# pipeline-interests-adaptive
改了很多，这个类是包括重传等核心机制的实现，为了适应chunk，修改了很多。原来的代码就类似于只需要请求一个chunk，但是现在的要这个chunk发完兴趣包之后接着发下一个chunk，同时这个chunk当然有可能需要重传，所以每一个chunk都由一个单独的pipeliner对象来管理，这也就是为什么在chunk-interest-adaptive中每一个chunk都要创建一个新的pipeliner来管理。

# 各种拥塞控制算法
就两个函数的实现，如果要添加不一样逻辑的拥塞控制算法，而不是只有增加和减少窗口的逻辑。那可能需要对调用这两个函数的地方进行修改，也许基类也需要重写。

