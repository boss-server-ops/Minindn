import re

input_file = "/home/dd/mini-ndn/chunkworkdir/con0_dcndis/logs/throughput_bw30_delay0_queue10000_loss0.0_splitsize.txt"
output_file = input_file.replace(".txt", "_processed.txt")

with open(input_file, 'r') as f_in, open(output_file, 'w') as f_out:
    for line in f_in:
        # 跳过文件路径注释行
        if line.startswith('//'):
            f_out.write(line)
            continue
            
        # 使用正则表达式匹配时间戳和单位
        match = re.match(r'(\d+\.\d+|\d+)\s+(milliseconds:.+)', line)
        if match:
            timestamp = float(match.group(1))
            rest_of_line = match.group(2)
            
            # 减去1200
            new_timestamp = timestamp - 1200
            
            # 写入新行
            f_out.write(f"{new_timestamp} {rest_of_line}\n")
        else:
            # 如果不匹配格式，保持原样
            f_out.write(line)

print(f"处理完成，结果保存到 {output_file}")