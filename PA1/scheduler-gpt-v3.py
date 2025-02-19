# Students:
# <Name> - FCFS
# <Name> - SJF
# Camilo Alvarez-Velez - RR
# <Name> - RR2

import sys
import heapq


class Process:
    def __init__(self, name, arrival, burst):
        self.name = name
        self.arrival = arrival
        self.burst = burst
        self.remaining_time = burst
        self.start_time = -1
        self.finish_time = -1
        self.waiting_time = 0

    def __lt__(self, other):
        return self.remaining_time < other.remaining_time


def process_sort_key(p):
    """
    Helper function to extract a numeric key from a process name.
    If the name is in the form 'P<number>', it returns that number.
    Otherwise, it returns the name string.
    """
    if p.name.startswith("P"):
        num_part = p.name[1:]
        if num_part.isdigit():
            return int(num_part)
    return p.name


def parse_input(filename):
    processes = []
    total_time = 0
    process_count = 0
    quantum = None
    algorithm = None

    with open(filename, 'r') as file:
        for line in file:
            parts = line.strip().split()
            if not parts:
                continue
            # Ignore comments starting with '#'
            if parts[0].startswith("#"):
                continue
            if parts[0] == "processcount":
                process_count = int(parts[1])
            elif parts[0] == "runfor":
                total_time = int(parts[1])
            elif parts[0] == "use":
                algorithm = parts[1]
            elif parts[0] == "quantum":
                quantum = int(parts[1])
            elif parts[0] == "process" and parts[1] == "name":
                # Format: process name <Name> arrival <Arrival> burst <Burst>
                name = parts[2]
                arrival = int(parts[4])
                burst = int(parts[6])
                processes.append(Process(name, arrival, burst))
            elif parts[0] == "end":
                break

    return process_count, total_time, processes, algorithm, quantum


# Implementation of Pre-emptive Shortest Job First Scheduling Algorithm
def sjf_scheduler(process_count, total_time, processes, output_filename):
    output = []
    output.append(f"{process_count} processes")
    output.append("Using preemptive Shortest Job First")

    time = 0
    ready_queue = []
    processes.sort(key=lambda p: p.arrival)
    process_index = 0
    running_process = None
    finished_processes = []
    unfinished = []

    while time < total_time:
        # Check for new arrivals
        while process_index < len(processes) and processes[process_index].arrival <= time:
            output.append(f"Time {time} : {processes[process_index].name} arrived")
            heapq.heappush(ready_queue, processes[process_index])
            process_index += 1

        # Check if we need to preempt the running process
        if running_process and ready_queue and ready_queue[0].remaining_time < running_process.remaining_time:
            heapq.heappush(ready_queue, running_process)
            running_process = None

        # Select a new process if necessary
        if not running_process and ready_queue:
            running_process = heapq.heappop(ready_queue)
            if running_process.start_time == -1:
                running_process.start_time = time
            output.append(f"Time {time} : {running_process.name} selected (burst {running_process.remaining_time})")

        # Run the selected process
        if running_process:
            running_process.remaining_time -= 1
            if running_process.remaining_time == 0:
                running_process.finish_time = time + 1
                finished_processes.append(running_process)
                output.append(f"Time {time + 1} : {running_process.name} finished")
                running_process = None
        else:
            output.append(f"Time {time} : Idle")

        time += 1

    output.append(f"Finished at time {total_time}")

    for process in processes:
        if process.finish_time == -1:
            unfinished.append(process.name)
        else:
            turnaround_time = process.finish_time - process.arrival
            waiting_time = turnaround_time - process.burst
            response_time = process.start_time - process.arrival
            output.append(f"{process.name} wait {waiting_time} turnaround {turnaround_time} response {response_time}")

    if unfinished:
        output.append("Processes that did not finish: " + " ".join(unfinished))

    with open(output_filename, 'w') as f:
        f.write("\n".join(output) + "\n")

#Implementation of First Come First Serve Scheduling Algorithm
def fcfs_scheduler(process_count, total_time, processes, output_filename):
    output = []
    output.append(f"{process_count} processes")
    output.append("Using First-Come First-Served")

    time = 0
    processes.sort(key=lambda p: p.arrival)  # Sort by arrival time
    process_index = 0
    running_process = None
    finished_processes = []

    while time < total_time:
        # Check for new arrivals
        while process_index < len(processes) and processes[process_index].arrival == time:
            output.append(f"Time {time} : {processes[process_index].name} arrived")
            process_index += 1

        # Select a new process if necessary
        if not running_process and len(finished_processes) != process_count:
            for process in processes:
                if process.finish_time == -1 and process.arrival <= time:
                    running_process = process
                    running_process.start_time = time
                    output.append(f"Time {time} : {running_process.name} selected (burst {running_process.burst})")
                    break

        # Run the selected process
        if running_process:
            running_process.remaining_time -= 1
            if running_process.remaining_time == 0:
                running_process.finish_time = time + 1
                finished_processes.append(running_process)
                output.append(f"Time {time + 1} : {running_process.name} finished")
                running_process = None
        else:
            output.append(f"Time {time} : Idle")

        time += 1

    # Print final results
    output.append(f"Finished at time {total_time}\n")

    # Sort processes by name before printing final statistics
    processes.sort(key=lambda p: p.name)

    for process in processes:
        turnaround = process.finish_time - process.arrival
        wait_time = turnaround - process.burst
        response_time = process.start_time - process.arrival
        output.append(f"{process.name} wait {wait_time} turnaround {turnaround} response {response_time}")

    # Write to output file
    with open(output_filename, "w") as file:
        file.write("\n".join(output))


# Implementation of Round Robin Scheduling Algorithm with event ordering.
def rr_scheduler(process_count, total_time, processes, quantum, output_filename):
    # Header lines (always printed first).
    header_lines = [f"{process_count:>3} processes", "Using Round-Robin", f"Quantum {quantum:>3}"]

    # We'll collect simulation events as tuples: (timestamp, priority, insertion_order, message)
    events = []
    event_counter = 0

    def add_event(t, priority, msg):
        nonlocal event_counter
        events.append((t, priority, event_counter, msg))
        event_counter += 1

    time = 0
    ready_queue = []
    processes.sort(key=lambda p: p.arrival)
    process_index = 0
    current_process = None
    current_quantum_counter = 0
    preempted = None  # Process whose quantum just expired

    while time < total_time:
        # Process any arrivals at this time.
        while process_index < len(processes) and processes[process_index].arrival <= time:
            add_event(time, 0, f"Time {time:>3} : {processes[process_index].name} arrived")
            ready_queue.append(processes[process_index])
            process_index += 1

        # Then, if a process was preempted in the previous tick, add it now.
        if preempted is not None:
            ready_queue.append(preempted)
            preempted = None

        # If no process is running, select one from the ready queue.
        if current_process is None:
            if ready_queue:
                current_process = ready_queue.pop(0)
                if current_process.start_time == -1:
                    current_process.start_time = time
                add_event(time, 2, f"Time {time:>3} : {current_process.name} selected (burst {current_process.remaining_time:>3})")
                current_quantum_counter = 0
            else:
                add_event(time, 3, f"Time {time:>3} : Idle")
                time += 1
                continue

        # Execute one tick.
        current_process.remaining_time -= 1
        current_quantum_counter += 1

        # Check if the current process has finished.
        if current_process.remaining_time == 0:
            finish_time = time + 1
            current_process.finish_time = finish_time
            add_event(finish_time, 1, f"Time {finish_time:>3} : {current_process.name} finished")
            current_process = None
            current_quantum_counter = 0
        # Otherwise, if the quantum expires, preempt the process.
        elif current_quantum_counter == quantum:
            preempted = current_process
            current_process = None
            current_quantum_counter = 0

        time += 1

    add_event(total_time, 4, f"Finished at time {total_time:>3}")

    # Sort simulation events by (timestamp, priority, insertion_order)
    events.sort(key=lambda e: (e[0], e[1], e[2]))
    simulation_timeline = [msg for (_, _, _, msg) in events]

    # Build the final summary (final chart), sorting processes by their numeric id.
    sorted_processes = sorted(processes, key=process_sort_key)
    summary_lines = []
    unfinished = []
    for process in sorted_processes:
        if process.finish_time == -1:
            unfinished.append(process.name)
        else:
            turnaround_time = process.finish_time - process.arrival
            waiting_time = turnaround_time - process.burst
            response_time = process.start_time - process.arrival
            summary_lines.append(
                f"{process.name} wait {waiting_time:>3} turnaround {turnaround_time:>3} response {response_time:>3}")
    if unfinished:
        summary_lines.append("Processes that did not finish: " + " ".join(unfinished))

    # Combine header lines, a blank line after header, simulation timeline, a blank line, and final summary lines.
    output_lines = header_lines + [""] + simulation_timeline + [""] + summary_lines

    with open(output_filename, 'w') as f:
        f.write("\n".join(output_lines) + "\n")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: scheduler-gpt.py <input file>")
        sys.exit(1)

    input_filename = sys.argv[1]
    # Ensure the input file has the proper .in extension.
    if not input_filename.endswith(".in"):
        print("Usage: scheduler-gpt.py <input file>")
        sys.exit(1)

    output_filename = input_filename.rsplit('.', 1)[0] + "-student.out"
    process_count, total_time, processes, algorithm, quantum = parse_input(input_filename)

    # Determine the scheduler type from the input filename (e.g., scheduler-gpt-v2-rr.in)
    schduler_split = input_filename.rsplit('-', 1)[1]
    scheduler_name = schduler_split.rsplit(".")[0]

    if scheduler_name == "sjf":
        sjf_scheduler(process_count, total_time, processes, output_filename)
    elif scheduler_name == "fcfs":
        fcfs_scheduler(process_count, total_time, processes, output_filename)
    elif scheduler_name == "rr":
        if quantum is None:
            print("Error: Missing quantum parameter when use is 'rr'")
            sys.exit(1)
        rr_scheduler(process_count, total_time, processes, quantum, output_filename)
    else:
        print("Scheduler Name Not Found from Input File")
