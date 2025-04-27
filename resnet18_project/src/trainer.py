import os
import time
import torch
import torch.nn as nn
import torch.optim as optim
from torch.optim.lr_scheduler import StepLR, CosineAnnealingLR
from tqdm import tqdm
import numpy as np
from datetime import datetime

class Trainer:
    def __init__(self, model, train_loader, test_loader, config):
        self.model = model
        self.train_loader = train_loader
        self.test_loader = test_loader
        self.config = config
        
        # 设置设备
        self.device = self._setup_device()
        self.model = self.model.to(self.device)
        
        # 设置优化器和学习率调度器
        self.criterion = nn.CrossEntropyLoss()
        self.optimizer = optim.SGD(
            self.model.parameters(),
            lr=config['training']['learning_rate'],
            momentum=config['training']['momentum'],
            weight_decay=config['training']['weight_decay']
        )
        
        # 设置学习率调度器
        if config['training']['lr_scheduler'] == 'step':
            self.scheduler = StepLR(
                self.optimizer,
                step_size=config['training']['lr_steps'][0],
                gamma=config['training']['lr_gamma']
            )
        else:  # cosine
            self.scheduler = CosineAnnealingLR(
                self.optimizer,
                T_max=config['training']['epochs']
            )
        
        # 创建检查点目录和日志目录
        os.makedirs(config['training']['checkpoint_dir'], exist_ok=True)
        os.makedirs(config['training']['log_dir'], exist_ok=True)
        
        # 训练统计
        self.start_epoch = 0
        self.best_acc = 0
        
    def _setup_device(self):
        if self.config['cuda']['use_cuda'] and torch.cuda.is_available():
            device = torch.device(f"cuda:{self.config['cuda']['gpu_id']}")
            print(f"Using GPU: {torch.cuda.get_device_name(device)}")
        else:
            device = torch.device("cpu")
            print("Using CPU")
        return device
    
    def train(self):
        """完整的训练流程"""
        
        print(f"Starting training for {self.config['training']['epochs']} epochs...")
        start_time = time.time()
        
        # 打开日志文件
        log_file = os.path.join(
            self.config['training']['log_dir'],
            f"training_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        )
        
        with open(log_file, 'w') as f:
            f.write(f"Training started at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Model: {self.config['model']['name']}\n")
            f.write(f"Dataset: {self.config['data']['dataset']}\n")
            f.write(f"Batch size: {self.config['data']['train_batch_size']}\n")
            f.write(f"Learning rate: {self.config['training']['learning_rate']}\n")
            f.write("-" * 50 + "\n")
            
            for epoch in range(self.start_epoch, self.config['training']['epochs']):
                # 训练一个epoch
                train_loss, train_acc = self._train_epoch(epoch)
                
                # 在测试集上评估
                test_loss, test_acc = self._validate()
                
                # 更新学习率
                self.scheduler.step()
                
                # 记录当前学习率
                current_lr = self.optimizer.param_groups[0]['lr']
                
                # 打印和记录结果
                log_message = (f"Epoch: {epoch+1}/{self.config['training']['epochs']} | "
                              f"Train Loss: {train_loss:.4f} | Train Acc: {train_acc:.2f}% | "
                              f"Test Loss: {test_loss:.4f} | Test Acc: {test_acc:.2f}% | "
                              f"LR: {current_lr:.6f}")
                
                print(log_message)
                f.write(log_message + "\n")
                
                # 保存模型检查点
                if (epoch + 1) % self.config['training']['save_frequency'] == 0:
                    self._save_checkpoint(epoch, test_acc)
                
                # 保存最佳模型
                if test_acc > self.best_acc:
                    self.best_acc = test_acc
                    self._save_checkpoint(epoch, test_acc, is_best=True)
                    print(f"New best accuracy: {test_acc:.2f}%")
                    f.write(f"New best accuracy: {test_acc:.2f}%\n")
        
        total_time = time.time() - start_time
        print(f"Training completed in {total_time/60:.2f} minutes")
        print(f"Best accuracy: {self.best_acc:.2f}%")
        
        # 记录训练完成信息
        with open(log_file, 'a') as f:
            f.write("-" * 50 + "\n")
            f.write(f"Training completed at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Total training time: {total_time/60:.2f} minutes\n")
            f.write(f"Best accuracy: {self.best_acc:.2f}%\n")
    
    def _train_epoch(self, epoch):
        """训练一个epoch"""
        self.model.train()
        running_loss = 0.0
        correct = 0
        total = 0
        
        pbar = tqdm(self.train_loader, desc=f"Epoch {epoch+1}/{self.config['training']['epochs']}")
        
        for inputs, targets in pbar:
            inputs, targets = inputs.to(self.device), targets.to(self.device)
            
            # 清零梯度
            self.optimizer.zero_grad()
            
            # 前向传播
            outputs = self.model(inputs)
            loss = self.criterion(outputs, targets)
            
            # 反向传播和优化
            loss.backward()
            self.optimizer.step()
            
            # 统计
            running_loss += loss.item()
            _, predicted = outputs.max(1)
            total += targets.size(0)
            correct += predicted.eq(targets).sum().item()
            
            # 更新进度条
            pbar.set_postfix({
                'loss': running_loss / (pbar.n + 1),
                'acc': 100. * correct / total
            })
        
        train_loss = running_loss / len(self.train_loader)
        train_acc = 100. * correct / total
        
        return train_loss, train_acc
    
    def _validate(self):
        """在测试集上评估模型"""
        self.model.eval()
        running_loss = 0.0
        correct = 0
        total = 0
        
        with torch.no_grad():
            for inputs, targets in self.test_loader:
                inputs, targets = inputs.to(self.device), targets.to(self.device)
                
                # 前向传播
                outputs = self.model(inputs)
                loss = self.criterion(outputs, targets)
                
                # 统计
                running_loss += loss.item()
                _, predicted = outputs.max(1)
                total += targets.size(0)
                correct += predicted.eq(targets).sum().item()
        
        test_loss = running_loss / len(self.test_loader)
        test_acc = 100. * correct / total
        
        return test_loss, test_acc
    
    def _save_checkpoint(self, epoch, acc, is_best=False):
        """保存模型检查点"""
        checkpoint = {
            'epoch': epoch + 1,
            'state_dict': self.model.state_dict(),
            'optimizer': self.optimizer.state_dict(),
            'scheduler': self.scheduler.state_dict(),
            'best_acc': self.best_acc
        }
        
        # 保存常规检查点
        checkpoint_path = os.path.join(
            self.config['training']['checkpoint_dir'],
            f"checkpoint_epoch_{epoch+1}.pth"
        )
        torch.save(checkpoint, checkpoint_path)
        
        # 保存最佳模型
        if is_best:
            best_model_path = os.path.join(
                self.config['training']['checkpoint_dir'],
                "best_model.pth"
            )
            torch.save(checkpoint, best_model_path)