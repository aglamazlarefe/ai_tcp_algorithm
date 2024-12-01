import unittest
import torch
import numpy as np
from collections import deque
from rl_aqm import ReplayMemory, DQN, RlAqmAgent, gym  # Kodun modüler hale geldiğini varsayıyoruz.

class TestReplayMemory(unittest.TestCase):

    def test_push_and_sample(self):
        memory = ReplayMemory(10)
        sample_data = (torch.tensor([1.0]), 1, torch.tensor([2.0]), 1.0)
        
        # Belleğe veri ekle
        memory.push(*sample_data)
        self.assertEqual(len(memory), 1)
        
        # Örneklemenin doğru olup olmadığını kontrol et
        sampled_data = memory.sample(1)
        self.assertEqual(sampled_data[0], sample_data)

    def test_memory_limit(self):
        memory = ReplayMemory(5)
        for i in range(10):
            memory.push(torch.tensor([i]), i, torch.tensor([i+1]), i * 0.1)
        self.assertEqual(len(memory), 5)  # Maksimum kapasiteyi aşmamalı

class TestDQNModel(unittest.TestCase):

    def test_forward_pass(self):
        model = DQN(4, 2)
        input_tensor = torch.rand(1, 4)  # Rastgele bir girdi
        output = model(input_tensor)
        self.assertEqual(output.shape, (1, 2))  # Çıkış boyutu doğru mu?

class TestRlAqmAgent(unittest.TestCase):

    def test_action_selection(self):
        agent = RlAqmAgent(4, 2)
        state = torch.rand(1, 4)

        # Epsilon-greedy stratejisi: Rastgele veya model tabanlı aksiyon
        action = agent.select_action(state)
        self.assertIn(action.item(), [0, 1])

    def test_optimize_model(self):
        agent = RlAqmAgent(4, 2)
        
        # Belleğe birkaç deneyim ekle
        for _ in range(agent.batch_size):
            agent.memory.push(torch.rand(4), torch.tensor([[0]]), torch.rand(4), torch.tensor([1.0]))
        
        # Optimizasyonun hata vermeden çalışması gerekir
        try:
            agent.optimize_model()
        except Exception as e:
            self.fail(f"Optimize model test failed with exception: {e}")

class TestEnvironment(unittest.TestCase):

    def test_env_initialization(self):
        try:
            env = gym.make("CartPole-v1")  # Basit bir ortam örneği
            self.assertIsNotNone(env)
        except Exception as e:
            self.fail(f"Environment initialization failed: {e}")

    def test_env_interaction(self):
        env = gym.make("CartPole-v1")
        state = env.reset()
        self.assertIsNotNone(state)
        action = env.action_space.sample()
        next_state, reward, done, info = env.step(action)
        self.assertIsNotNone(next_state)
        self.assertIsInstance(reward, float)
