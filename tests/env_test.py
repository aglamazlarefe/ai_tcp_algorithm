import logging
import ns3ai_gym_env
import gymnasium as gym
import sys
import traceback
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
import random
from collections import namedtuple, deque


try:
    env = gym.make("ns3ai_gym_env/Ns3-v0", targetName="rl-aqm",
                   ns3Path="/home/aglamazlarefe/ns-allinone-3.43/ns-3.43")
    state = env.reset()
    print(f"Initial State: {state}")
    for step in range(5):
        action = 0  # Sabit bir aksiyon gönderin
        next_state, reward, done, _, info = env.step(action)
        print(f"Step {step}: State={next_state}, Reward={reward}, Done={done}")
        if done:
            break
except Exception as e:
    print(f"Step sırasında hata: {e}")
