# pylint:disable=E0401
"""
Simulate whale dive sequences and transmission intervals of the Scout Buoy to
evaluate timing.
"""
# standard library
import argparse
import os
# third party
import numpy as np
import matplotlib.pyplot as plt


def get_cycles(
    sequence_length:int, dive_min:int, dive_max:int, surface_duration:int
) -> list[tuple[int, int, str]]:
    """
    Randomly generate a sequence of diving and surfacing.

    :params int sequence_length: Duration of the requested sequence
    :params int dive_min: Assumed minimum dive time
    :params ind dive_max: Assumed maximum dive time
    :params surface_duration: Assumed minimum surface duration
    """
    time_pointer = 0
    cycle_times = []
    while time_pointer < sequence_length:
        dive_duration = np.random.uniform(dive_min, dive_max)
        # Append dive period
        cycle_times.append((time_pointer, time_pointer + dive_duration, 'dive'))
        time_pointer += dive_duration
        # Append surface period
        cycle_times.append((
            time_pointer, time_pointer + surface_duration, 'surface'))
        time_pointer += surface_duration
    # end sequence at the end of requested times
    cycle_times[-1] = (cycle_times[-1][0], sequence_length, cycle_times[-1][2])
    return cycle_times


def get_tx_results(
    cycles:list[tuple[int, int, str]], interval:int,
    success_rate: int
) -> list[tuple[int, str]]:
    """
    A list of simulated TX results

    :params list[tuple[int, int, str]] cycles: Simulated cycles from get_cycles
    :params int interval: Tx interval
    :params int success_rate: Success rate of above water transmission attempts
    :return type list[tuple[int, str]]
    """
    tx_times = np.arange(0, cycles[-1][1], interval)
    tx_results = []  # List of (tx_time, result_string)
    for tx_time in tx_times:
        # Find whale state at tx_time
        whale_state = None
        for (start, end, state) in cycles:
            if start <= tx_time < end:
                whale_state = state
                break
        # Determine result
        if whale_state == 'surface':
            # At surface → apply success probability
            success = np.random.rand() < success_rate
            result = 'success' if success else 'fail_surface'
        else:
            # Underwater → fail
            result = 'fail_underwater'
        tx_results.append((tx_time, result))
    return tx_results


def analyze(tx_results:[list[tuple[int, str]]]) -> tuple[int, int]:
    """
    Output some stats
    :params list[tuple[int, str]] tx_results:
        Simulated tx_results from get_tx_results
    :return type tuple[int, int]: num_successes, effective_tx_rate_min
    """
    # Analyze effective transmission rate
    return sum(1 for tx, result in tx_results if result == 'success')


def plot_results(
    cycles:list[tuple[int, int, str]], tx_results:list[tuple[int, str]]
) -> None:
    """
    Plot results

    :params list[tuple[int, int, str]] cycles: Simulated cycles from get_cycles
    :params list[tuple[int, str]] tx_results:
        Simulated tx_results from get_tx_results
    :return type None:
    """
    # Plotting
    plt.figure(figsize=(14, 4))
    # Plot whale state bars
    for start, end, state in cycles:
        color = 'white' if state == 'surface' else 'blue'
        plt.axvspan(start, end, color=color, alpha=1.0)
    # Plot transmissions
    for tx, result in tx_results:
        labels = plt.gca().get_legend_handles_labels()[1]
        if result == 'success':
            label = '' if 'Success' in labels else 'Success'
            plt.plot(tx, 1, 'go', label=label)
        elif result == 'fail_surface':
            label = '' if 'Fail (surface)' in labels else 'Fail (surface)'
            plt.plot(tx, 1, 'yo', label=label)
        else:
            label = '' if 'Fail (underwater)' in labels else 'Fail (underwater)'
            plt.plot(tx, 1, 'ro', label=label)

    # Format plot
    plt.title(
        'Dive cycles, surface time, and tag transmissions')
    plt.xlabel('Time (minutes since start)')
    plt.yticks([])
    plt.xlim(0, cycles[-1][1])
    plt.legend(loc='upper right')
    plt.subplots_adjust(top=0.85)  # manually adjust top margin
    plt.show()


def main(args:argparse.Namespace) -> None:
    """
    The main function

    :params argparse.Namespace args: CLI arguments and defaults
    :return type None:
    """
    cycles = get_cycles(
        args.duration, args.minimum_dive, args.maximum_dive,
        args.minimum_surface)

    tx_results = get_tx_results(cycles, args.interval, surface_success_odds)

    num_successes = analyze(tx_results)

    effective_tx_rate = args.duration/num_successes if num_successes else None

    print(
        f'\nsimulation hours: {args.duration/60:.1f}\n'
        f'tx interval min: {args.interval}\n'
        f'surface duration: {args.minimum_surface}\n'
        f'total tx tries: {len(tx_results)}\n'
        f'Successful tx: {num_successes}\n'
        f'Effective txrate: {effective_tx_rate:.1f}\n'
        'min per successful transmission\n')

    plot_results(cycles, tx_results)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        prog = f'\n\npython {os.path.basename(__file__)}',
        description=__doc__, epilog='Thank you for saving the whales!')
    parser.add_argument(
        '-d', '--duration', type=int, default=120,
        help='Sequence duration in minutes (default=120)')
    parser.add_argument(
        '-i', '--interval', type=int, default=10,
        help='Tx interval in minutes (default=10)')
    parser.add_argument(
        '-m', '--minimum_dive', type=int, default=4,
        help='Assumed minimum dive duration in minutes (default=4)')
    parser.add_argument(
        '-x', '--maximum_dive', type=int, default=7,
        help='Assumed maximum dive duration in minutes (default=7)')
    parser.add_argument(
        '-s', '--minimum_surface', type=int, default=3,
        help='Assumed minimum surface time in minutes (default=3)')
    parser.add_argument(
        '-o', '--surface_success_odds', type=float, default=.9,
        help='Odds of successful transmission while on surface (default=0.9)')
    parser.add_argument(
        '-t', '--gps_timeout', type=int, default=6,
        help='Timeout for GPS in minutes (default=6)')
    parser.add_argument(
        '-p', '--process_time', type=int, default=2,
        help='Minimum process time for successful send in minutes (default-2)')
    main(parser.parse_args())
