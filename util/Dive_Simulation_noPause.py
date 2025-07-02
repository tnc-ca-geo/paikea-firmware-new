import numpy as np
import matplotlib.pyplot as plt

# Parameters
transmission_interval_min = 10
simulation_hours = 2
total_minutes = simulation_hours * 60

min_dive_duration_min = 4
max_dive_duration_min = 7
surface_duration_min = 3

transmission_success_rate = 0.9  # Probability of success if whale is at surface

# Simulate whale dive/surface timeline for simulation hours
cycle_times = []  # List of (start_time, end_time, state_string)
time_pointer = 0

while time_pointer < total_minutes:
    # Random dive duration
    dive_duration = np.random.uniform(min_dive_duration_min, max_dive_duration_min)
    surface_duration = surface_duration_min

    # Append dive period
    cycle_times.append((time_pointer, time_pointer + dive_duration, 'dive'))
    time_pointer += dive_duration

    # Append surface period
    cycle_times.append((time_pointer, time_pointer + surface_duration, 'surface'))
    time_pointer += surface_duration

# Simulate tag transmissions
tx_times = np.arange(0, total_minutes, transmission_interval_min)
tx_results = []  # List of (tx_time, result_string)

for tx_time in tx_times:
    # Find whale state at tx_time
    whale_state = None
    for (start, end, state) in cycle_times:
        if start <= tx_time < end:
            whale_state = state
            break

    # Determine result
    if whale_state == 'surface':
        # At surface → apply success probability
        success = np.random.rand() < transmission_success_rate
        result = 'success' if success else 'fail_surface'
    else:
        # Underwater → fail
        result = 'fail_underwater'

    tx_results.append((tx_time, result))

# Analyze effective transmission rate
num_successes = sum(1 for tx, result in tx_results if result == 'success')

if num_successes > 0:
    effective_tx_rate_min = total_minutes / num_successes
else:
    effective_tx_rate_min = np.inf  # No successes

# Print results

print(f"\nsimulation hours: {simulation_hours}")
print(f"tx interval min: {transmission_interval_min}")
print(f"surface duration min: {surface_duration_min}")
print(f"total tx tries: {len(tx_times)}")
print(f"Successful tx: {num_successes}")
print(f"Effective txrate: {effective_tx_rate_min:.1f} min per successful transmission\n")

# Plotting
plt.figure(figsize=(14, 4))

# Plot whale state bars
for start, end, state in cycle_times:
    color = 'white' if state == 'surface' else 'blue'
    plt.axvspan(start, end, color=color, alpha=1.0)

# Plot transmissions
for tx, result in tx_results:
    if result == 'success':
        plt.plot(tx, 1, 'go', label='Success' if 'Success' not in plt.gca().get_legend_handles_labels()[1] else "")
    elif result == 'fail_surface':
        plt.plot(tx, 1, 'yo', label='Fail (surface)' if 'Fail (surface)' not in plt.gca().get_legend_handles_labels()[1] else "")
    else:
        plt.plot(tx, 1, 'ro', label='Fail (underwater)' if 'Fail (underwater)' not in plt.gca().get_legend_handles_labels()[1] else "")

# Format plot
plt.title("One 12-hour simulation: Dive cycles, surface time, and tag transmissions")
plt.xlabel("Time (minutes since start)")
plt.yticks([])
plt.xlim(0, total_minutes)
plt.legend(loc='upper right')

# Add text with effective transmission rate
plt.text(total_minutes * 0.01, 0.2, 
         f"Effective transmission rate: {effective_tx_rate_min:.1f} min per successful transmission",
         fontsize=12, bbox=dict(facecolor='white', alpha=0.8))

plt.subplots_adjust(top=0.85)  # manually adjust top margin
plt.show()