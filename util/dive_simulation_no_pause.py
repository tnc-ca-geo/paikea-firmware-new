# pylint:disable=E0401
"""
Simulate whale dive sequences and transmission intervals of the Scout Buoy to
evaluate timing.
"""
# standard library
import argparse
import os
import random
from typing import Union
# third party
import numpy as np
import matplotlib.pyplot as plt


def get_cycles(
    sequence_length:int, dive_min:int, dive_max:int, surface_duration:int
) -> list[tuple[float, float, str]]:
    """
    Randomly generate a sequence of diving and surfacing.

    :params int sequence_length: Duration of the requested sequence
    :params int dive_min: Assumed minimum dive time
    :params ind dive_max: Assumed maximum dive time
    :params surface_duration: Assumed minimum surface duration
    :return type list[tuple[float, float, str]]:
    """
    dive = random.random() > .5
    first = True
    time_pointer = 0
    cycle_times = []
    while time_pointer < sequence_length:
        if dive:
            dive_start = 0 if first else dive_min
            duration = np.random.uniform(dive_start, dive_max)
            cycle_times.append((time_pointer, time_pointer + duration, 'dive'))
        else:
            surface_start = 0 if first else surface_duration
            duration = np.random.uniform(surface_start, surface_duration)
            cycle_times.append((
                time_pointer, time_pointer + duration, 'surface'))
        time_pointer += duration
        dive = not dive
        first = False
    # end sequence at the end of requested times
    cycle_times[-1] = (cycle_times[-1][0], sequence_length, cycle_times[-1][2])
    return cycle_times


def get_overlap(
    range1:tuple[float, float], range2:tuple[float, float]
) -> Union[tuple[float, float], None]:
    """
    Get the overlap of two ranges

    :param tuple[float, float] range1: The first range.
    :param tuple[float, float] range2: The second range.
    :return type Union[tuple[float, float], None]:
    """
    # Unpack the ranges
    start1, end1 = range1
    start2, end2 = range2
    # Calculate the overlap
    overlap_start = max(start1, start2)
    overlap_end = min(end1, end2)
    # Check if there is an actual overlap
    if overlap_start < overlap_end:
        return (overlap_start, overlap_end)
    else:
        return None  # No overlap


def get_tx_results(
    cycles:list[tuple[float, float, str]], interval:int,
    success_rate:float, gps_timeout:int, process_time:int
) -> list[tuple[float, float, str]]:
    """
    A list of simulated TX results

    :params list[tuple[float, float, str]] cycles: Simulated cycles from get_cycles
    :params int interval: Tx interval in minutes
    :params float success_rate: Success rate of above water send attempts
    :params int gps_timeout: Timout for GPS in minutes
    :params int process_time: Minimum process time for success in minutes
    :return type list[tuple[float, str]]
    """
    tx_times = np.arange(0, cycles[-1][1], interval)
    tx_results = []  # List of (tx_time, result_string)
    for tx_time in tx_times:
        result = 'fail_underwater'
        end_time = tx_time + gps_timeout + process_time
        # find start cycles that intersect
        for (start, end, state) in cycles:
            dive_range = (start, end)
            tx_range = (tx_time, tx_time + gps_timeout + process_time)
            overlap = get_overlap(dive_range, tx_range)
            if overlap:
                rx_start = tx_time if tx_time > start else start
                if state == 'surface' and rx_start + process_time < end:
                    result = 'success'
                    if random.random() > success_rate:
                        result = 'fail_surface'
                    else:
                        end_time = rx_start + process_time
                        break
        if result == 'success' and random.random() > success_rate:
            result = 'fail_surface'
        tx_results.append((tx_time, end_time, result))
    return tx_results


def analyze(tx_results:[list[tuple[float, str]]]) -> tuple[float, float]:
    """
    Output some stats
    :params list[tuple[float, str]] tx_results:
        Simulated tx_results from get_tx_results
    :return type tuple[float, float]: num_successes, effective_tx_rate_min
    """
    # Analyze effective transmission rate
    num_success = sum(1 for tx, _, result in tx_results if result == 'success')
    return num_success


def plot_results(
    cycles:list[tuple[float, float, str]], tx_results:list[tuple[float, str]]
) -> None:
    """
    Plot results

    :params list[tuple[float, float, str]] cycles: Simulated cycles from get_cycles
    :params list[tuple[float, str]] tx_results:
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
    for tx_start, tx_end, result in tx_results:
        labels = plt.gca().get_legend_handles_labels()[1]
        if result == 'success':
            label = '' if 'Success' in labels else 'Success'
            plt.plot((tx_start, tx_end), (1, 1), 'g-', label=label, linewidth=5)
        elif result == 'fail_surface':
            label = '' if 'Fail (surface)' in labels else 'Fail (surface)'
            plt.plot((tx_start, tx_end), (1, 1), 'y-', label=label, linewidth=5)
        else:
            label = '' if 'Fail (underwater)' in labels else 'Fail (underwater)'
            plt.plot((tx_start, tx_end), (1, 1), 'r-', label=label, linewidth=5)

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
    print(__doc__)
    cycles = get_cycles(
        args.duration, args.minimum_dive, args.maximum_dive,
        args.minimum_surface)
    tx_results = get_tx_results(
        cycles, args.interval, args.surface_success_odds, args.gps_timeout,
        args.process_time)
    num_successes = analyze(tx_results)
    effective_tx_rate = args.duration/num_successes if num_successes else None
    print(
        'Assumptions:\n'
        '------------\n'
        f'simulation duration: {args.duration/60:.1f} hours\n'
        f'scheduled tx interval: {args.interval} minutes\n'
        f'surface duration: {args.minimum_surface} minutes\n'
        f'minimum dive duration: {args.maximum_dive} minutes\n'
        f'maximum dive duration: {args.maximum_dive} minutes\n'
        '\nResults:\n'
        '-------\n'
        f'total tx tries: {len(tx_results)}\n'
        f'successful tx: {num_successes}')
    if effective_tx_rate:
        print(
            f'Effective tx rate: {effective_tx_rate:.1f} '
            'min per successful transmission\n')
    else:
        print('No successful transmission')
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
        help='Odds of successful transmission while on surface (default=0.8)')
    parser.add_argument(
        '-t', '--gps_timeout', type=int, default=6,
        help='Timeout for GPS in minutes (default=6)')
    parser.add_argument(
        '-p', '--process_time', type=int, default=2,
        help='Minimum process time for successful send in minutes (default=3)')
    main(parser.parse_args())
