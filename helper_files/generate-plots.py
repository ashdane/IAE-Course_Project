import matplotlib.pyplot as plt
import numpy as np

# --- 1. DATA SETUP ---
n_values = [100000, 1000000, 5000000, 10000000, 50000000]

# Times in ms for "Random Distribution"
ca_random = [8.15, 96.57, 564.28, 1204.28, 7565.43]
co_random = [5.83, 65.63, 363.99, 781.75, 5961.96]
std_random = [5.13, 57.85, 330.55, 1046.30, 9044.58]

# Times in ms for "Nearly-Sorted Distribution"
ca_nearly_sorted = [8.75, 443.14, 534.60, 1064.27, 6306.96]
co_nearly_sorted = [7.62, 94.05, 629.67, 1465.74, 8195.06]
std_nearly_sorted = [1.65, 21.89, 164.73, 371.17, 2271.45]

# --- 2. LINE CHART (N vs Time - Random Distribution) ---
fig1, ax1 = plt.subplots(figsize=(10, 6))
ax1.plot(n_values, ca_random, marker='o', label='Cache-Aware')
ax1.plot(n_values, co_random, marker='s', label='Cache-Oblivious')
ax1.plot(n_values, std_random, marker='^', label='std::sort')

ax1.set_xscale('log')
ax1.set_xlabel('N (Input Size - Log Scale)')
ax1.set_ylabel('Time (ms)')
ax1.set_title('Performance Comparison (Random Distribution)')
ax1.legend()
ax1.grid(True, which="both", ls="-", alpha=0.5)
plt.savefig('line_chart.png', dpi=300)

# --- 3. LINE CHART (N vs Time - Nearly-Sorted Distribution) ---
fig2, ax2 = plt.subplots(figsize=(10, 6))
ax2.plot(n_values, ca_nearly_sorted, marker='o', label='Cache-Aware')
ax2.plot(n_values, co_nearly_sorted, marker='s', label='Cache-Oblivious')
ax2.plot(n_values, std_nearly_sorted, marker='^', label='std::sort')

ax2.set_xscale('log')
ax2.set_xlabel('N (Input Size - Log Scale)')
ax2.set_ylabel('Time (ms)')
ax2.set_title('Performance Comparison (Nearly-Sorted Distribution)')
ax2.legend()
ax2.grid(True, which="both", ls="-", alpha=0.5)
plt.savefig('line_chart_nearly_sorted.png', dpi=300)

# --- 4. BAR CHART (Comparison at N=10M) ---
distributions = ['Random', 'Nearly-Sorted']
ca_vals = [1204.28, 1064.27]
co_vals = [781.75, 1465.74]
std_vals = [1046.30, 371.17]

x = np.arange(len(distributions))
width = 0.25 

fig3, ax3 = plt.subplots(figsize=(10, 6))
ax3.bar(x - width, ca_vals, width, label='Cache-Aware')
ax3.bar(x, co_vals, width, label='Cache-Oblivious')
ax3.bar(x + width, std_vals, width, label='std::sort')

ax3.set_ylabel('Time (ms)')
ax3.set_title('Performance Comparison at N=10M')
ax3.set_xticks(x)
ax3.set_xticklabels(distributions)
ax3.legend()

plt.savefig('bar_chart.png', dpi=300)