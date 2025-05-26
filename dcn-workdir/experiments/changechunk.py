#!/usr/bin/env python3

import sys
import os
import re
import configparser

def parse_size(size_str):
    """将1MB、1GB等格式转换为字节数"""
    units = {
        'B': 1,
        'KB': 1024,
        'MB': 1024 * 1024,
        'GB': 1024 * 1024 * 1024,
        'TB': 1024 * 1024 * 1024 * 1024
    }
    
    match = re.match(r'^(\d+(?:\.\d+)?)\s*([A-Za-z]+)$', size_str.strip())
    if not match:
        raise ValueError(f"无法解析大小: {size_str}")
    
    value, unit = match.groups()
    value = float(value)
    unit = unit.upper()
    
    if unit not in units:
        raise ValueError(f"未知单位: {unit}")
    
    return int(value * units[unit])

def update_config_files(chunk_size_bytes, total_size_bytes):
    # 计算总chunk数
    total_chunks = (total_size_bytes + chunk_size_bytes - 1) // chunk_size_bytes  # 向上取整
    
    # 更新proconfig.ini
    pro_config_path = './proconfig.ini'
    pro_config = configparser.ConfigParser(allow_no_value=True)
    pro_config.read(pro_config_path)
    
    # 更新chunk-size而非size
    pro_config['General']['chunk-size'] = str(chunk_size_bytes)
    
    # 写入修改后的proconfig.ini
    with open(pro_config_path, 'w') as f:
        pro_config.write(f)
    
    # 更新conconfig.ini
    con_config_path = './conconfig.ini'
    con_config = configparser.ConfigParser(allow_no_value=True)
    con_config.read(con_config_path)
    
    # 更新chunk-size
    con_config['General']['chunk-size'] = str(chunk_size_bytes)
    # 更新totalchunksnumber
    con_config['General']['totalchunksnumber'] = str(total_chunks)
    
    # 写入修改后的conconfig.ini
    with open(con_config_path, 'w') as f:
        con_config.write(f)
        
    return total_chunks

def generate_hello_txt(size_bytes):
    """生成指定大小的hello.txt文件"""
    hello_path = './hello.txt'
    
    # 创建目录（如果不存在）
    os.makedirs(os.path.dirname(hello_path), exist_ok=True)
    
    # 每次写入1MB数据以避免内存问题
    chunk_size = 1024 * 1024
    remaining = size_bytes
    
    with open(hello_path, 'wb') as f:
        while remaining > 0:
            write_size = min(chunk_size, remaining)
            f.write(b'A' * write_size)  # 使用字符'A'填充文件
            remaining -= write_size
    
    print(f"已生成文件：{hello_path}（大小：{size_bytes}字节）")

def update_aggregatorcat_config(chunk_size_bytes, total_chunks):
    """更新aggregatorcat.ini文件"""
    aggregatorcat_config_path = './aggregatorcat.ini'
    aggregatorcat_config = configparser.ConfigParser(allow_no_value=True)
    aggregatorcat_config.read(aggregatorcat_config_path)
    
    # 更新chunk-size和totalchunksnumber
    aggregatorcat_config['General']['chunk-size'] = str(chunk_size_bytes)
    aggregatorcat_config['General']['totalchunksnumber'] = str(total_chunks)
    
    # 写入修改后的aggregatorcat.ini
    with open(aggregatorcat_config_path, 'w') as f:
        aggregatorcat_config.write(f)
    
    print(f"已更新文件：{aggregatorcat_config_path}")

# 修改main函数，调用新的更新函数
def main():
    if len(sys.argv) != 3:
        print("用法: python3 changechunk.py <chunk大小> <总文件大小>")
        print("例如: python3 changechunk.py 1MB 10MB")
        sys.exit(1)
        
    chunk_size_str = sys.argv[1]
    total_size_str = sys.argv[2]
    
    try:
        chunk_size_bytes = parse_size(chunk_size_str)
        total_size_bytes = parse_size(total_size_str)
    except ValueError as e:
        print(f"错误: {e}")
        sys.exit(1)
    
    # 更新配置文件
    total_chunks = update_config_files(chunk_size_bytes, total_size_bytes)
    
    # 更新aggregatorcat.ini
    update_aggregatorcat_config(chunk_size_bytes, total_chunks)
    
    # 生成hello.txt
    generate_hello_txt(total_size_bytes)
    
    print(f"配置已更新:")
    print(f"  Chunk大小: {chunk_size_bytes} 字节")
    print(f"  总文件大小: {total_size_bytes} 字节")
    print(f"  总Chunk数: {total_chunks}")

if __name__ == "__main__":
    main()