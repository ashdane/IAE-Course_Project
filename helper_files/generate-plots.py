import matplotlib.pyplot as plt
import numpy as np

# --- 1. DATA SETUP (From your benchmark output) ---
n_values = [100000, 1000000, 5000000, 10000000, 50000000]

# Times in ms for "Random Distribution"
ca_random = [7.19, 82.00, 422.25, 873.91, 4836.27]
co_random = [6.29, 69.34, 410.05, 801.01, 4710.79]
std_random = [4.80, 60.29, 336.40, 710.28, 4237.74]

# Times in ms for "Nearly-Sorted Distribution"
ca_nearly_sorted = [4.77, 62.69, 360.18, 1036.94, 4032.16]
co_nearly_sorted = [3.25, 43.74, 366.45, 626.21, 3396.61]
std_nearly_sorted = [1.02, 11.66, 106.16, 165.16, 902.95]

# --- 2. LINE CHART (N vs Time - Random Distribution) ---
plt.figure(figsize=(10, 6))
plt.plot(n_values, ca_random, marker='o', label='Cache-Aware')
plt.plot(n_values, co_random, marker='s', label='Cache-Oblivious')
plt.plot(n_values, std_random, marker='^', label='std::sort')

plt.xscale('log')
plt.xlabel('N (Input Size - Log Scale)')
plt.ylabel('Time (ms)')
plt.title('Performance Comparison (Random Distribution)')
plt.legend()
plt.grid(True, which="both", ls="-", alpha=0.5)
plt.savefig('line_chart.png', dpi=300)
print("Saved: line_chart.png")
plt.show()

# --- 3. LINE CHART (N vs Time - Nearly-Sorted Distribution) ---
plt.figure(figsize=(10, 6))
plt.plot(n_values, ca_nearly_sorted, marker='o', label='Cache-Aware')
plt.plot(n_values, co_nearly_sorted, marker='s', label='Cache-Oblivious')
plt.plot(n_values, std_nearly_sorted, marker='^', label='std::sort')

plt.xscale('log')
plt.xlabel('N (Input Size - Log Scale)')
plt.ylabel('Time (ms)')
plt.title('Performance Comparison (Nearly-Sorted Distribution)')
plt.legend()
plt.grid(True, which="both", ls="-", alpha=0.5)
plt.savefig('line_chart_nearly_sorted.png', dpi=300)
print("Saved: line_chart_nearly_sorted.png")
plt.show()

# --- 4. BAR CHART (Comparison at N=10M) ---
# Data for N=10M
# Random: [CA: 873.91, CO: 801.01, std: 710.28]
# Nearly-Sorted: [CA: 1036.94, CO: 626.21, std: 165.16]
distributions = ['Random', 'Nearly-Sorted']
ca_vals = [873.91, 1036.94]
co_vals = [801.01, 626.21]
std_vals = [710.28, 165.16]

x = np.arange(len(distributions))
width = 0.25 

fig, ax = plt.subplots(figsize=(10, 6))
ax.bar(x - width, ca_vals, width, label='Cache-Aware')
ax.bar(x, co_vals, width, label='Cache-Oblivious')
ax.bar(x + width, std_vals, width, label='std::sort')

ax.set_ylabel('Time (ms)')
ax.set_title('Performance Comparison at N=10M')
ax.set_xticks(x)
ax.set_xticklabels(distributions)
ax.legend()

plt.savefig('bar_chart.png', dpi=300)
print("Saved: bar_chart.png")
plt.show()