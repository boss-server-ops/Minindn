import os
import getpass
import sys
import time
import argparse
import shutil  # 添加shutil模块用于文件操作

def validate_float(value, name):
    try:
        float_val = float(value)
        if float_val < 0:
            raise ValueError
        return float_val
    except ValueError:
        print(f"错误：{name}必须为非负数")
        sys.exit(1)

def update_ring_ini(full_file, primary_file, pipeline_type):
    """更新环形拓扑专用配置文件"""
    ini_files = [
        "./ring-workdir/experiments/conconfig.ini",
        "./ring-workdir/experiments/aggregatorcat.ini"
    ]
    
    for ini_file in ini_files:
        # 确保目录存在
        os.makedirs(os.path.dirname(ini_file), exist_ok=True)
        
        # 准备新的路径值
        new_topo_path = f"../../topologies/{full_file}"
        new_primary_topo_path = f"../../topologies/{primary_file}"
        
        # 如果文件不存在，则创建新文件
        if not os.path.exists(ini_file):
            with open(ini_file, "w") as f:
                f.write(f"topofilepath = {new_topo_path}\n")
                f.write(f"primarytopofilepath = {new_primary_topo_path}\n")
                if "conconfig.ini" in ini_file:
                    f.write(f"pipeline-type = {pipeline_type}\n")
            continue
            
        # 读取现有内容并修改指定行
        with open(ini_file, "r") as file:
            lines = file.readlines()
        
        found_topo = False
        found_primary = False
        found_pipeline = False
        
        with open(ini_file, "w") as file:
            for line in lines:
                if line.strip().startswith("topofilepath"):
                    file.write(f"topofilepath = {new_topo_path}\n")
                    found_topo = True
                elif line.strip().startswith("primarytopofilepath"):
                    file.write(f"primarytopofilepath = {new_primary_topo_path}\n")
                    found_primary = True
                elif line.strip().startswith("pipeline-type") and "conconfig.ini" in ini_file:
                    file.write(f"pipeline-type = {pipeline_type}\n")
                    found_pipeline = True
                else:
                    file.write(line)
            
            # 如果没找到对应配置项，添加到文件末尾
            if not found_topo:
                file.write(f"topofilepath = {new_topo_path}\n")
            if not found_primary:
                file.write(f"primarytopofilepath = {new_primary_topo_path}\n")
            if "conconfig.ini" in ini_file and not found_pipeline:
                file.write(f"pipeline-type = {pipeline_type}\n")
    
    print(f"INI 文件已更新为使用 {full_file}，pipeline-type = {pipeline_type}")

if __name__ == "__main__":
    try:
        # 添加命令行参数解析
        parser = argparse.ArgumentParser(description='自动化环形拓扑测试')
        parser.add_argument('--password', help='sudo密码', default=None)
        args = parser.parse_args()
        # 固定参数组
        parameter_sets = [
            # {"agg_num": "3", "pro_per_agg": "2", "bw_param": "30", "loss_param": "0.0"},
            # {"agg_num": "3", "pro_per_agg": "2", "bw_param": "30", "loss_param": "0.01"},
            # {"agg_num": "3", "pro_per_agg": "2", "bw_param": "30", "loss_param": "0.1"},
            # {"agg_num": "3", "pro_per_agg": "2", "bw_param": "30", "loss_param": "0.5"},
            # {"agg_num": "3", "pro_per_agg": "2", "bw_param": "30", "loss_param": "1.0"},
            {"agg_num": "6", "pro_per_agg": "5", "bw_param": "30", "loss_param": "0.0"},
            # {"agg_num": "7", "pro_per_agg": "6", "bw_param": "30", "loss_param": "0.0"},
            # {"agg_num": "4", "pro_per_agg": "3", "bw_param": "30", "loss_param": "0.0"},
            # {"agg_num": "5", "pro_per_agg": "4", "bw_param": "30", "loss_param": "0.0"}
        ]
        
        # 获取用户密码（优先使用命令行参数）
        password = args.password
        if password is None:
            password = getpass.getpass("请输入sudo密码：")
        
        # 确保工作目录存在
        os.makedirs("./ring-workdir/experiments", exist_ok=True)
        
        # 循环执行所有参数组
        for params in parameter_sets:
            agg_num = params["agg_num"]
            pro_per_agg = params["pro_per_agg"]
            bw_param = params["bw_param"]
            loss_param = params["loss_param"]
            
            loss_val = validate_float(loss_param, "丢包率")
            formatted_loss = f"{loss_val:.1f}"  # 统一保留1位小数
            
            # 生成标准化文件名
            base_name = f"a{agg_num}_p{pro_per_agg}_b{bw_param}_l{formatted_loss}"
            full_file = f"ring_full_{base_name}.conf"
            primary_file = f"ring_primary_{base_name}.conf"
            
            print(f"\n===== 开始测试环形拓扑参数组: agg_num={agg_num}, pro_per_agg={pro_per_agg}, bw={bw_param}, loss={formatted_loss} =====\n")
            
            # 生成拓扑文件
            gen_cmd = (
                f"cd ./topologies && "
                f"python ./RingTopoGenerator.py "
                f"--agg-num {agg_num} "
                f"--pro-per-agg {pro_per_agg} "
                f"--bw {bw_param} "
                f"--loss {formatted_loss} "
                f"&& cd -"
            )
            if os.system(gen_cmd) != 0:
                print(f"拓扑生成失败：{full_file}")
                continue  # 跳过当前参数组，继续下一组
            
            # 循环执行四种不同的拥塞控制算法
            # algorithms = ["aimd", "bic", "cubic", "hybla"]
            algorithms = ["aimd","bic","cubic","hybla"]

            for algorithm in algorithms:
                print(f"\n======== 开始测试 {algorithm} 算法 ========\n")
                
                # 更新 .ini 文件，设置拓扑和拥塞控制算法
                update_ring_ini(full_file, primary_file, algorithm)
                
                # 构建并运行 ring_test.py 命令
                cmd = (
                    f"echo {password} | sudo -S "
                    f"python ./examples/dcn_test.py "
                    f"--work-dir ./ring-workdir "
                    f"./topologies/{full_file}"
                )
                print(f"执行命令: {cmd}")
                os.system(cmd)
                
                # 执行kill_process.sh终止所有相关进程
                print(f"清理实验进程...")
                os.system(f"echo {password} | sudo -S bash ./kill_process.sh")
                
                # 等待一段时间，确保上一个实验完全结束
                print(f"\n{algorithm} 测试完成，等待20秒后开始下一个算法...\n")
                # time.sleep(20)
            
            # 备份日志文件夹
            logs_dir = "./ring-workdir/con0/logs"
            if os.path.exists(logs_dir):
                # 创建新的日志文件夹名称，基于当前参数
                new_logs_dir = f"./ring-workdir/con0/ring_a{agg_num}_p{pro_per_agg}_b{bw_param}_l{formatted_loss}_logs"
                
                # 如果目标目录已存在，先删除
                if os.path.exists(new_logs_dir):
                    print(f"删除旧的日志目录: {new_logs_dir}")
                    shutil.rmtree(new_logs_dir)
                
                # 重命名日志文件夹
                print(f"备份日志: {logs_dir} -> {new_logs_dir}")
                shutil.move(logs_dir, new_logs_dir)
            else:
                print(f"警告: 日志目录 {logs_dir} 不存在，无法备份")
            
            print(f"\n参数组 agg_num={agg_num}, pro_per_agg={pro_per_agg} 测试完成\n")
            # time.sleep(10)  # 参数组之间多等待10秒
        
        print("\n所有环形拓扑测试已完成！\n")
        
    except Exception as e:
        print(f"环形拓扑测试失败：{str(e)}")
        sys.exit(1)