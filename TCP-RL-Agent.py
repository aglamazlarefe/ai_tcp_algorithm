#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import math
import sys
import argparse

import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt

import tensorflow as tf

from ns3gym import ns3env
from tcp_base import TcpTimeBased, TcpEventBased
import os
os.environ["CUDA_VISIBLE_DEVICES"] = "-1"

# Dosya oluşturma ve loglama
try:
	w_file = open('run.log', 'w')
except:
	w_file = sys.stdout

# Argümanları ayarla
parser = argparse.ArgumentParser(description='Simülasyon betiğini başlatma/kapatma')
parser.add_argument('--start',
					type=int,
					default=1,
					help='ns-3 simülasyon betiğini başlatma 0/1, Varsayılan: 1')
parser.add_argument('--iterations',
					type=int,
					default=1,
					help='Tekrar sayısı, Varsayılan: 1')
parser.add_argument('--steps',
					type=int,
					default=100,
					help='Adım sayısı, Varsayılan: 100')

args = parser.parse_args()

startSim = bool(args.start)
iterationNum = int(args.iterations)
maxSteps = int(args.steps)

# ns-3 ortamını başlatmak için ayarları yap
port = 5555
simTime = maxSteps / 10.0 # simülasyon süresi saniye cinsinden
seed = 12
simArgs = {"--duration": simTime,}

dashes = "-"*18
input("[{}Başlamak için enter'a basınız{}]".format(dashes, dashes))

# Ortamı oluştur
env = ns3env.Ns3Env(port=port, startSim=startSim, simSeed=seed, simArgs=simArgs)

ob_space = env.observation_space
ac_space = env.action_space

# Ajanı al veya oluştur
# Bir socket UUID'sine göre ajanları kaydet ve yönlendir

def get_agent(state):
	socketUuid = state[0] # Socket UUID
	tcpEnvType = state[1] # TCP ortam tipi (event-based veya time-based)
	tcpAgent = get_agent.tcpAgents.get(socketUuid, None)
	if tcpAgent is None:
		# Seçilen ortam tipine göre yeni bir ajan oluştur
		if tcpEnvType == 0:
			# Event tabanlı ajan = 0
			tcpAgent = TcpEventBased()
		else:
			# Zaman tabanlı ajan = 1
			tcpAgent = TcpTimeBased()
		tcpAgent.set_spaces(get_agent.ob_space, get_agent.ac_space)
		get_agent.tcpAgents[socketUuid] = tcpAgent

	return tcpAgent

# Ajan değişkenlerini başlat
get_agent.tcpAgents = {}
get_agent.ob_space = ob_space
get_agent.ac_space = ac_space

# Tam bağlantılı bir sinir ağı tasarlayan bir fonksiyon
def modeler(input_size, output_size):
    """
    Tam bağlantılı bir sinir ağı tasarlar.
    """
    model = tf.keras.Sequential()

    # Giriş katmanı
    model.add(tf.keras.layers.Input(shape=(input_size,)))
    
    # Gizli katmanlar ekle
    model.add(tf.keras.layers.Dense((input_size + output_size) // 2, activation='relu'))

    # Çıkış katmanı
    model.add(tf.keras.layers.Dense(output_size, activation='softmax'))
    
    return model

# Durum ve aksiyon boyutlarını belirle
state_size = ob_space.shape[0] - 4 # Ortamın 4 özelliğini görmezden gel

action_size = 3
action_mapping = {} # Hızlı erişim için dict kullan
action_mapping[0] = 0
action_mapping[1] = 600
action_mapping[2] = -150

# Modeli oluştur
model = modeler(state_size, action_size)
model.compile(
	optimizer = tf.keras.optimizers.Adam(learning_rate=1e-2),
	loss='categorical_crossentropy',
	metrics=['accuracy']
)
model.summary()

# Epsilon-greedy algoritmasını başlat
# Keşif ve sömürü dengesi için ince ayar
epsilon = 1.0
epsilon_decay_param = iterationNum * 2
min_epsilon = 0.1
epsilon_decay = (((epsilon_decay_param*maxSteps) - 1.0) / (epsilon_decay_param*maxSteps))

# Q-learning indirim faktörünü başlat
discount_factor = 0.95

# Ödül ve tarihçe verilerini kaydetmek için değişkenler
total_reward = 0
reward_history = []
cWnd_history = []
pred_cWnd_history = []
rtt_history = []
tp_history = []

# Ortalama ile varyans analizi için yakın geçmişi belirle
recency = maxSteps // 15

# Eğitim döngüsü başlat
for iteration in range(iterationNum):
	# İlk durumu ayarla
	state = env.reset()
	# Ortam özelliklerini görmezden gel: socketID, ortam tipi, simülasyon zamanı, nodeID
	state = state[4:]

	cWnd = state[1]
	init_cWnd = cWnd

	state = np.reshape(state, [1, state_size])
	pretty_slash = ['\\', '|', '/', '-']



	try:
		for step in range(maxSteps):
			# Çıktıyı güzelleştirmek için
			pretty_index = step % 4
			print("\r[{}] Dosyaya yazılıyor {} {}".format(
				pretty_slash[pretty_index],
				w_file.name,
				'.'*(pretty_index+1)
			), end='')

			# Epsilon-greedy seçim
			if step == 0 or np.random.rand(1) < epsilon:
				# Yeni bir durum keşfet
				action_index = np.random.randint(0, action_size)
			else:
				# Bilgi birikimini kullan
				action_index = np.argmax(model.predict(state)[0])

			# Aksiyon hesapla
			calc_cWnd = cWnd + action_mapping[action_index]

			# Congestion window'u sınırla
			thresh = state[0][0] # ssThresh
			if step+1 > recency:
				tp_dev = math.sqrt(np.var(tp_history[(-recency):]))
				tp_1per = 0.01 * throughput
				if tp_dev < tp_1per:
					thresh = cWnd
			new_cWnd = max(init_cWnd, (min(thresh, calc_cWnd)))
			
			new_ssThresh = int(cWnd/2)
			actions = [new_ssThresh, new_cWnd]

			# Ortama aksiyon gönder ve geri bildirim al
			next_state, reward, done, _ = env.step(actions)

			total_reward += reward

			next_state = next_state[4:]
			cWnd = next_state[1]
			rtt = next_state[7]
			throughput = next_state[11]

			next_state = np.reshape(next_state, [1, state_size])
			
			# Sinir ağı eğitimi için hedef oluştur
			target = reward
			if not done:
				target = (reward + discount_factor * np.amax(model.predict(next_state)[0]))
			target_f = model.predict(state)
			target_f[0][action_index] = target
			model.fit(state, target_f, epochs=1, verbose=0)

			# Durumu güncelle
			state = next_state

			if done:
				break

			if epsilon > min_epsilon:
				epsilon *= epsilon_decay

			# Verileri kaydet
			reward_history.append(total_reward)
			rtt_history.append(rtt)
			cWnd_history.append(cWnd)
			tp_history.append(throughput)
	finally:
		if iteration+1 == iterationNum:
			break

# RTT ve TP geçmişini dosyaya yazdır
with open('rtt_tp_history.txt', 'w') as file:
    file.write("Adım\tRTT (μs)\tThroughput (bits)\n")
    for i in range(len(rtt_history)):
        file.write(f"{i+1}\t{rtt_history[i]}\t{tp_history[i]}\n")

# Yeni grafik oluşturma
mpl.rcdefaults()
mpl.rcParams.update({'font.size': 12})
fig, ax = plt.subplots(2, 2, figsize=(12, 8))
plt.tight_layout(pad=4)

# 1. Congestion Window grafiği
ax[0, 0].plot(range(len(cWnd_history)), cWnd_history, marker="", linestyle="-")
ax[0, 0].set_title('Congestion Window Değişimi')
ax[0, 0].set_xlabel('Adımlar')
ax[0, 0].set_ylabel('CWND (segment)')
ax[0, 0].grid(True)

# 2. Throughput grafiği
ax[0, 1].plot(range(len(tp_history)), tp_history, marker="", linestyle="-")
ax[0, 1].set_title('Throughput Değişimi')
ax[0, 1].set_xlabel('Adımlar')
ax[0, 1].set_ylabel('Throughput (bits)')
ax[0, 1].grid(True)

# 3. RTT grafiği
ax[1, 0].plot(range(len(rtt_history)), rtt_history, marker="", linestyle="-")
ax[1, 0].set_title('RTT Değişimi')
ax[1, 0].set_xlabel('Adımlar')
ax[1, 0].set_ylabel('RTT (μs)')
ax[1, 0].grid(True)

# 4. Ödül Toplamı grafiği
ax[1, 1].plot(range(len(reward_history)), reward_history, marker="", linestyle="-")
ax[1, 1].set_title('Toplam Ödül Değişimi')
ax[1, 1].set_xlabel('Adımlar')
ax[1, 1].set_ylabel('Toplam Ödül')
ax[1, 1].grid(True)

# Grafiği kaydet ve göster
plt.savefig('improved_plots.png')
plt.show()
