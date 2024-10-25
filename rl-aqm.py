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


# Deneyim (state, action, next_state, reward)
Transition = namedtuple('Transition',
                        ('state', 'action', 'next_state', 'reward'))


# Deneyimleri saklamak için Replay Belleği
class ReplayMemory(object):

    def __init__(self, capacity):
        self.memory = deque([], maxlen=capacity)

    def push(self, *args):
        """Bir deneyimi kaydet"""
        self.memory.append(Transition(*args))

    def sample(self, batch_size):
        return random.sample(self.memory, batch_size)

    def __len__(self):
        return len(self.memory)

# Deep Q-Network (DQN) modeli
class DQN(nn.Module):

    def __init__(self, state_size, action_size):
        super(DQN, self).__init__()
        self.fc1 = nn.Linear(state_size, 64)
        self.fc2 = nn.Linear(64, 64)
        self.fc3 = nn.Linear(64, action_size)

    def forward(self, x):
        x = torch.relu(self.fc1(x))
        x = torch.relu(self.fc2(x))
        return self.fc3(x)

class RlAqmAgent:

    def __init__(self, state_size, action_size):
        self.state_size = state_size
        self.action_size = action_size
        self.memory = ReplayMemory(10000)
        self.batch_size = 64
        self.gamma = 0.99
        self.eps_start = 0.9
        self.eps_end = 0.05
        self.eps_decay = 200
        self.steps_done = 0
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

        self.policy_net = DQN(state_size, action_size).to(self.device)
        self.target_net = DQN(state_size, action_size).to(self.device)
        self.target_net.load_state_dict(self.policy_net.state_dict())
        self.target_net.eval()

        self.optimizer = optim.adam.Adam(self.policy_net.parameters())# optim.Adam yerine torch.optim.Adam kullanın

    def select_action(self, state): 
        sample = random.random()
        eps_threshold = self.eps_end + (self.eps_start - self.eps_end) * \
            np.exp(-1. * self.steps_done / self.eps_decay)
        self.steps_done += 1
        if sample > eps_threshold:
            with torch.no_grad():
                return self.policy_net(state).max(1)[1].view(1, 1)
        else:
            return torch.tensor([[random.randrange(self.action_size)]], device=self.device, dtype=torch.long)

    def optimize_model(self):
        if len(self.memory) < self.batch_size:
            return
        transitions = self.memory.sample(self.batch_size)
        batch = Transition(*zip(*transitions))
    
        non_final_mask = torch.tensor(tuple(map(lambda s: s is not None,
                                                batch.next_state)), device=self.device, dtype=torch.bool)
        non_final_next_states = torch.cat([s for s in batch.next_state if s is not None]).to(self.device)
        state_batch = torch.cat(batch.state).to(self.device)
        action_batch = torch.cat(batch.action).to(self.device)
        reward_batch = torch.cat(batch.reward).to(self.device)
    
        state_action_values = self.policy_net(state_batch).gather(1, action_batch)
        next_state_values = torch.zeros(self.batch_size, device=self.device)
        next_state_values[non_final_mask] = self.target_net(non_final_next_states).max(1)[0].detach()
        expected_state_action_values = (next_state_values * self.gamma) + reward_batch
    
        loss = nn.functional.smooth_l1_loss(state_action_values, expected_state_action_values.unsqueeze(1))
    
        self.optimizer.zero_grad()
        loss.backward()
        for param in self.policy_net.parameters():
            if param.grad is not None:  # Check if the gradient exists
                param.grad.data.clamp_(-1, 1)
        self.optimizer.step()


    def update_target_net(self):
        self.target_net.load_state_dict(self.policy_net.state_dict())

# NS-3 AI Gym Ortamını başlat   
env= gym.make("ns3ai_gym_env/Ns3-v0", targetName="rl-aqm",
               ns3Path="/home/aglamazlarefe/ns-3-dev", )  # Ns3AIGymEnv yerine Ns3Env kullanın
state_size = env.observation_space
action_size = env.action_space

# RL-AQM Ajanını başlat
agent = RlAqmAgent(state_size, action_size)

# Eğitim döngüsü
for episode in range(1000):
    state = env.reset()
    done = False
    rewards = 0.0


    while not done:
        action = agent.select_action(torch.tensor(state, device=agent.device, dtype=torch.float32))
        next_state, reward, done, _,info  = env.step(action.item())
        rewards += float(reward)
        agent.memory.push(torch.tensor(state, device=agent.device, dtype=torch.float32),
                         action, torch.tensor(next_state, device=agent.device, dtype=torch.float32),
                         torch.tensor([reward], device=agent.device, dtype=torch.float32))
        state = next_state
        agent.optimize_model()
    agent.update_target_net()
    print(f'Episode {episode+1}, Reward: {rewards:.2f}')

# Test döngüsü
state = env.reset()
done = False
while not done:
    action = agent.select_action(torch.tensor(state, device=agent.device, dtype=torch.float32))
    next_state, reward, done, _ , info = env.step(action.item())
    state = next_state
    print(f'State: {state}, Action: {action.item()}, Reward: {reward:.2f}')