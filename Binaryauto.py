import os
import getpass
import time
import shutil

# 更新 .ini 文件中的 topofilepath
def update_ini_files(binary_file_name, pipeline_type):
    ini_files = [
        "./chunkworkdir/experiments/aggregatorcat.ini",
        "./chunkworkdir/experiments/conconfig.ini"
    ]
    new_topofilepath = f"../../topologies/{binary_file_name}"

    for ini_file in ini_files:
        # 确保目录存在
        os.makedirs(os.path.dirname(ini_file), exist_ok=True)
        
        # 如果文件不存在，创建一个空文件
        if not os.path.exists(ini_file):
            open(ini_file, "w").close()
            
        # 读取现有内容
        with open(ini_file, "r") as file:
            lines = file.readlines()
        
        # 修改内容
        with open(ini_file, "w") as file:
            found_pipeline_type = False
            for line in lines:
                if line.strip().startswith("topofilepath"):
                    file.write(f"topofilepath = {new_topofilepath}\n")
                elif line.strip().startswith("primarytopofilepath"):
                    file.write(f"primarytopofilepath = {new_topofilepath}\n")
                elif line.strip().startswith("pipeline-type") and "conconfig.ini" in ini_file:
                    file.write(f"pipeline-type = {pipeline_type}\n")
                    found_pipeline_type = True
                else:
                    file.write(line)
                    
            # 如果没有找到pipeline-type配置项，则添加它
            if "conconfig.ini" in ini_file and not found_pipeline_type:
                file.write(f"pipeline-type = {pipeline_type}\n")
                
    print(f"INI 文件已更新为使用 {binary_file_name}，pipeline-type = {pipeline_type}")

# 生成拓扑文件（只执行一次）
def generate_topology(binary_param, bw_param, loss_param):
    binary_file_name = f"binary_tree_{binary_param}_bw{bw_param}_loss{loss_param}.conf"
    # 检查文件是否已存在
    if not os.path.exists(f"./topologies/{binary_file_name}"):
        # 切换到 topologies 目录，运行 Binary.py 脚本生成拓扑文件，然后切换回原目录
        os.system(f"cd ./topologies && python Binary.py {binary_param} --bw {bw_param} --loss {loss_param} && cd -")
    
    return binary_file_name

# 主程序
if __name__ == "__main__":
    # 固定参数组
    parameter_sets = [
        {"binary_param": "3", "bw_param": "30", "loss_param": "0.0"},
        # {"binary_param": "3", "bw_param": "30", "loss_param": "0.01"},
        # {"binary_param": "3", "bw_param": "30", "loss_param": "0.1"},
        # {"binary_param": "3", "bw_param": "30", "loss_param": "0.5"},
        # {"binary_param": "3", "bw_param": "30", "loss_param": "1.0"},
        # {"binary_param": "6", "bw_param": "30", "loss_param": "0.0"},
        # {"binary_param": "7", "bw_param": "30", "loss_param": "0.0"},
        # {"binary_param": "4", "bw_param": "30", "loss_param": "0.0"},
        # {"binary_param": "5", "bw_param": "30", "loss_param": "0.0"}
    ]
    
    # 获取用户密码（只询问一次）
    password = getpass.getpass("Enter your sudo password: ")
    
    # 确保工作目录存在
    os.makedirs("./chunkworkdir/experiments", exist_ok=True)
    
    # 循环执行所有参数组
    for params in parameter_sets:
        binary_param = params["binary_param"]
        bw_param = params["bw_param"]
        loss_param = params["loss_param"]
        
        print(f"\n===== 开始测试二叉树层数={binary_param}, 带宽={bw_param}, 丢包率={loss_param} =====\n")
        
        # 生成拓扑文件
        binary_file_name = generate_topology(binary_param, bw_param, loss_param)
        
        # 循环执行四种不同的拥塞控制算法
        # algorithms = ["aimd", "bic", "cubic", "hybla"]
        algorithms = ["bic"]
        
        for algorithm in algorithms:
            print(f"\n======== 开始测试 {algorithm} 算法 ========\n")
            
            # 更新 .ini 文件，设置拓扑和拥塞控制算法
            update_ini_files(binary_file_name, algorithm)
            
            # 构建并运行 BinaryTest.py 命令
            command = f"echo {password} | sudo -S python ./examples/dcn_test.py --work-dir ./chunkworkdir ./topologies/{binary_file_name}"
            print(f"执行命令: {command}")
            os.system(command)
            
            # 执行kill_process.sh终止所有相关进程
            print(f"清理实验进程...")
            os.system(f"echo {password} | sudo -S bash ./kill_process.sh")
            
            # 等待一段时间
            print(f"\n{algorithm} 测试完成，等待20秒后开始下一个算法...\n")
            # time.sleep(20)
        
        # 备份日志文件夹
        logs_dir = "./chunkworkdir/con0/logs"
        if os.path.exists(logs_dir):
            # 创建新的日志文件夹名称，基于当前参数
            new_logs_dir = f"./chunkworkdir/con0/binary_tree_{binary_param}_bw{bw_param}_loss{loss_param}_logs"
            
            # 如果目标目录已存在，先删除
            if os.path.exists(new_logs_dir):
                print(f"删除旧的日志目录: {new_logs_dir}")
                shutil.rmtree(new_logs_dir)
            
            # 重命名日志文件夹
            print(f"备份日志: {logs_dir} -> {new_logs_dir}")
            shutil.move(logs_dir, new_logs_dir)
        else:
            print(f"警告: 日志目录 {logs_dir} 不存在，无法备份")
        
        print(f"\n层数 {binary_param} 的所有测试已完成\n")
        # time.sleep(10)  # 参数组之间多等待10秒
    
    # 在Binaryauto.py脚本末尾
    print("\n所有Binary测试已完成！开始执行DCN测试...\n")

    # # 执行DCNauto.py脚本，传递密码参数
    # os.system(f"python ./DCNauto.py --password '{password}'")