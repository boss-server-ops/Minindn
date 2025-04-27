import os
import argparse
from src.data_loader import get_data_loaders
from src.model import create_model
from src.trainer import Trainer
from src.utils import load_config, set_seed

def main():
    parser = argparse.ArgumentParser(description='Train ResNet18 on CIFAR-10')
    parser.add_argument('--config', type=str, default='./config/config.yaml',
                        help='Path to the config file')
    parser.add_argument('--seed', type=int, default=42,
                        help='Random seed for reproducibility')
    args = parser.parse_args()
    
    # 设置随机种子
    set_seed(args.seed)
    
    # 加载配置
    config = load_config(args.config)
    
    # 准备数据加载器
    train_loader, test_loader = get_data_loaders(config)
    
    # 创建模型
    model = create_model(config)
    
    # 创建训练器
    trainer = Trainer(model, train_loader, test_loader, config)
    
    # 开始训练
    trainer.train()

if __name__ == '__main__':
    main()