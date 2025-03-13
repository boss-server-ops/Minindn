#!/usr/bin/env python3

import os
import re
import matplotlib.pyplot as plt
import numpy as np
import glob

def parse_throughput_file(file_path):
    """Parse throughput file, return time and throughput data"""
    times = []
    throughputs = []
    
    with open(file_path, 'r') as f:
        for line in f:
            # Match format like "1000.04 milliseconds: 37.257216 Mbit/s"
            # Also support scientific notation like "9.5e-05 milliseconds: 77.594624 Mbit/s"
            match = re.match(r'([\d\.]+(?:[eE][\-\+]?\d+)?)\s+milliseconds:\s+([\d\.]+(?:[eE][\-\+]?\d+)?)\s+Mbit/s', line)
            if match:
                time_ms = float(match.group(1))
                throughput = float(match.group(2))
                # Convert to seconds
                times.append(time_ms / 1000)
                throughputs.append(throughput)
    
    return np.array(times), np.array(throughputs)

def extract_params_from_filename(filename):
    """Extract parameter values from filename"""
    params = {}
    # Match parameters like bw20_delay0_queue10000_loss0_splitsize
    param_matches = re.findall(r'([a-z]+)(\d+)', filename)
    for param, value in param_matches:
        params[param] = int(value)
    return params

def main():
    # Get all throughput files in current directory
    throughput_files = glob.glob("throughput_*.txt")
    
    if not throughput_files:
        print("No throughput files found")
        return
    
    # Process each file separately
    for file_path in throughput_files:
        # Extract filename without path and extension
        filename = os.path.basename(file_path)
        base_name = os.path.splitext(filename)[0]
        
        # Extract parameters from filename
        params = extract_params_from_filename(base_name)
        
        # Create parameter string for title
        param_str = ", ".join([f"{k}={v}" for k, v in params.items()])
        
        # Read data
        times, throughputs = parse_throughput_file(file_path)
        
        # Debug output for first few points
        print(f"File: {filename}")
        for i in range(min(3, len(times))):
            print(f"  Point {i}: time={times[i]}s, throughput={throughputs[i]} Mbit/s")
        
        # Create new figure for each file
        plt.figure(figsize=(10, 6))
        
        # Plot with markers
        plt.plot(times, throughputs, 
                 color='b', 
                 linestyle='-',
                 marker='o',  # Use circle markers
                 markersize=6,
                 linewidth=2)
        
        # Set chart properties
        plt.xlabel('Time (seconds)', fontsize=14)
        plt.ylabel('Throughput (Mbit/s)', fontsize=14)
        plt.title(f'NDN Network Throughput ({param_str})', fontsize=16)
        plt.grid(True)
        
        # Set y-axis starting from 0
        plt.ylim(bottom=0)
        
        # Add some padding to the top
        y_max = max(throughputs) * 1.1
        plt.ylim(top=y_max)
        
        # Save individual plot
        output_filename = f"{base_name}_plot.png"
        plt.tight_layout()
        plt.savefig(output_filename, dpi=300, bbox_inches='tight')
        plt.close()  # Close the figure to free memory
        
        print(f"Chart generated: {output_filename}")

if __name__ == "__main__":
    main()