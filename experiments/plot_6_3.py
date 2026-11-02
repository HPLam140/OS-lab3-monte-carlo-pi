import pandas as pd
import matplotlib.pyplot as plt

# Đọc file kết quả
df = pd.read_csv('results_6_3.csv')

# Vẽ biểu đồ thời gian thực thi (Execution Time)
plt.figure(figsize=(8, 5))
plt.plot(df['nThreads'], df['Approach2_Local'], marker='o', label='Approach 2 (Local Accumulation)', color='green', linewidth=2)
plt.plot(df['nThreads'], df['Approach3_Shared'], marker='s', label='Approach 3 (Shared Mutex)', color='red', linewidth=2)

plt.title('Trade-Off 6.3: Local vs Shared Computation', fontsize=13, fontweight='bold')
plt.xlabel('Number of Threads', fontsize=11)
plt.ylabel('Execution Time (seconds)', fontsize=11)
plt.xticks(df['nThreads'])
plt.legend()
plt.grid(True)
plt.tight_layout()

# Lưu ảnh biểu đồ
plt.savefig('tradeoff_6_3_chart.png', dpi=300)
plt.show()
