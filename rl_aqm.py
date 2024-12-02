import logging
import ns3ai_gym_env
import gymnasium as gym
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
import random
from collections import namedtuple, deque

# Logging yapılandırması
logging.basicConfig(level=logging.INFO)

# Deneyim (state, action, next_state, reward)
Transition = namedtuple('Transition', ('state', 'action', 'next_state', 'reward'))

# Replay Memory
class ReplayMemory:
    def __init__(self, capacity):
        self.memory = deque([], maxlen=capacity)

    def push(self, *args):
        self.memory.append(Transition(*args))

    def sample(self, batch_size):
        return random.sample(self.memory, batch_size)

    def __len__(self):
        return len(self.memory)

# DQN Modeli
class DQN(nn.Module):
    def __init__(self, state_size, action_size):
        super(DQN, self).__init__()
        self.fc1 = nn.Linear(state_size, 64)  # Gözlemlerin boyutuna uygun giriş katmanı
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

        self.optimizer = optim.Adam(self.policy_net.parameters())

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

        non_final_mask = torch.tensor(tuple(map(lambda s: s is not None, batch.next_state)), device=self.device, dtype=torch.bool)
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
            if param.grad is not None:
                param.grad.data.clamp_(-1, 1)
        self.optimizer.step()

    def update_target_net(self):
        self.target_net.load_state_dict(self.policy_net.state_dict())

# Ortam başlatma
env = gym.make("ns3ai_gym_env/Ns3-v0", targetName="rl-aqm", ns3Path="/home/aglamazlarefe/ns-allinone-3.43/ns-3.43")
print(env.observation_space)  # Gözlem alanının boyutlarını yazdır
print(env.action_space)       # Aksiyon alanının boyutlarını yazdır

state_size = env.observation_space.shape[0]  # Gözlem boyutunu al
action_size = env.action_space.n            # Aksiyon sayısını al
agent = RlAqmAgent(state_size, action_size)

# Eğitim döngüsü
for episode in range(1000):
    state, info = env.reset()
    state = torch.tensor(state, device=agent.device, dtype=torch.float32).unsqueeze(0)  # Şekillendirme
    done = False
    rewards = 0.0

    while not done:
        action = agent.select_action(state)
        next_state, reward, done, _, info = env.step(action.item())
        next_state = torch.tensor(next_state, device=agent.device, dtype=torch.float32).unsqueeze(0)  # Şekillendirme
        reward = torch.tensor([reward], device=agent.device, dtype=torch.float32)

        agent.memory.push(state, action, next_state, reward)
        state = next_state
        agent.optimize_model()
        rewards += reward.item()

    agent.update_target_net()
    print(f"Episode {episode + 1}, Toplam ödül: {rewards:.2f}")

# Test döngüsü
state,info = env.reset()
done = False
while not done:
    try:
        action = agent.select_action(torch.tensor(state, device=agent.device, dtype=torch.float32))
        next_state, reward, done, _, info = env.step(action.item())
        logging.debug(f"Test Durumu: {state}, Aksiyon: {action.item()}, Ödül: {reward:.2f}")
        state = next_state
    except Exception as e:
        logging.error("Test sırasında bir hata oluştu.", exc_info=True)
