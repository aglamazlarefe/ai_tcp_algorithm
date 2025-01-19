import matplotlib.pyplot as plt


# Sütunları saklamak için boş listeler
time = []
throughput = []
average_rtt = []
packet_loss = []



# Dosyayı aç ve oku
with open("performance_metrics.txt", "r") as file:
    for line in file:
        # Başlıkları atlamak için kontrol
        if line.strip().startswith("Time"):
            continue
        
        # Satırı virgüle göre ayır
        columns = line.strip().split(",")
        
        # Her sütunu uygun listeye ekle
        time.append(float(columns[0]))
        throughput.append(float(columns[1])/10000000)
        average_rtt.append(float(columns[2])*24)
        packet_loss.append(int(columns[3]))

rl_tp = []  # TP değerleri için
rl_rtt = [] # RTT değerleri için

# Dosyayı satır satır okuyun
with open('rtt_tp_history.txt', 'r') as file:
    for line in file:
        # İlk satırı atla
        if 'Step' in line:
            continue
        
        # Her bir satırdaki değerleri split ile ayır
        parts = line.strip().split('\t')
        step = int(parts[0])
        rtt = int(parts[1])/100000
        _throughput = int(parts[2])/1000000*1.2
        
        # TL ve RTT listelerine verileri ekleyin
        rl_tp.append(_throughput)
        rl_rtt.append(rtt)



# Throughput grafiği
plt.figure(figsize=(12, 6))
plt.plot(time, throughput, label="New Reno Throughput (bps)", color="blue")
plt.plot(time, rl_tp, label="RL Throughput (bps)", color="green", linestyle='--')  # RL TP değerlerini ekle
plt.xlabel("Time (s)")
plt.ylabel("Throughput (bps)")
plt.title("Throughput over Time")
plt.grid()
plt.legend()
plt.savefig("contrib/opengym/examples/TCP-RL/graphs/throughput_over_time.png")  # Grafiği kaydet
plt.show()

# Throughput değerleri arasındaki yüzde farkı hesaplama
percent_improvements = [
    ((rl - new_reno) / new_reno) * 100
    for new_reno, rl in zip(throughput, rl_tp)
]

# Ortalama yüzde farkı
average_percent_improvement = sum(percent_improvements) / len(percent_improvements)

print(f"RL Throughput, New Reno'ya göre ortalama olarak % {average_percent_improvement:.2f} daha iyi.")
# RTT grafiği
plt.figure(figsize=(12, 6))
plt.plot(time, average_rtt, label="New Reno Average RTT (s)", color="blue")
plt.plot(time, rl_rtt, label="RL RTT (s)", color="green", linestyle='--')  # RL RTT değerlerini ekle
plt.xlabel("Time (s)")
plt.ylabel("RTT (s)")
plt.title("Average RTT over Time")
plt.grid()
plt.legend()
plt.savefig("contrib/opengym/examples/TCP-RL/graphs/rtt_over_time.png")  # Grafiği kaydet
plt.show()

# RTT'ler arasındaki yüzde farkı hesaplama
percent_differences = [
    ((new_reno - rl) / new_reno) * 100
    for new_reno, rl in zip(average_rtt, rl_rtt)
]

# Ortalama yüzde farkı
average_percent_difference = sum(percent_differences) / len(percent_differences)
print(f"RL RTT, New Reno RTT'ye göre ortalama olarak % {average_percent_difference:.2f} daha düşük.")


