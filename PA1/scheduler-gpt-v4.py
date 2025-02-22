# Students:
# Kazi Amin - FCFS
# Adrian Cosentino - SJF
# Camilo Alvarez-Velez - RR
# Edward Rosales - SJF2 & Optimizations

import sys
import heapq
from collections import deque

class Process:
    def __init__(self, name, arrival, burst):
        self.name = name
        self.arrival = arrival
        self.burst = burst
        self.remaining_time = burst
        self.start_time = -1
        self.finish_time = -1

    def __lt__(self, other):
        return self.remaining_time < other.remaining_time

def process_sort_key(p):
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
            if not parts or parts[0].startswith("#"):
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
                name = parts[2]
                arrival = int(parts[4])
                burst = int(parts[6])
                processes.append(Process(name, arrival, burst))
            elif parts[0] == "end":
                break

    processes.sort(key=lambda p: (p.arrival, process_sort_key(p)))  # Sort once globally
    return process_count, total_time, processes, algorithm, quantum

def calculate_process_metrics(processes, output):
    """ Compute and append waiting, turnaround, and response times. """
    output.append("")  # Empty line for formatting
    unfinished = []
    
    for process in sorted(processes, key=process_sort_key):
        if process.finish_time == -1:
            unfinished.append(process.name)
        else:
            turnaround_time = process.finish_time - process.arrival
            waiting_time = turnaround_time - process.burst
            response_time = process.start_time - process.arrival
            output.append(f"{process.name} wait {waiting_time:>3} turnaround {turnaround_time:>3} response {response_time:>3}")

    if unfinished:
        output.append("Processes that did not finish: " + " ".join(unfinished))

def sjf_scheduler(process_count, total_time, processes, output_filename):
    output = [f"{process_count:>3} processes", "Using preemptive Shortest Job First"]
    time = 0
    ready_queue = []
    process_index = 0
    running_process = None

    while time < total_time:
        # Process arrivals
        while process_index < len(processes) and processes[process_index].arrival == time:
            output.append(f"Time {time:>3} : {processes[process_index].name} arrived")
            heapq.heappush(ready_queue, processes[process_index])
            process_index += 1

        # Handle preemption if a new shorter job arrives
        if running_process and ready_queue and ready_queue[0].remaining_time < running_process.remaining_time:
            output.append(f"Time {time:>3} : {running_process.name} preempted")
            heapq.heappush(ready_queue, running_process)
            running_process = None

        # Select a new process if necessary
        if not running_process and ready_queue:
            running_process = heapq.heappop(ready_queue)
            if running_process.start_time == -1:
                running_process.start_time = time
            output.append(f"Time {time:>3} : {running_process.name} selected (burst {running_process.remaining_time:>3})")

        # Execute the running process
        if running_process:
            running_process.remaining_time -= 1
            if running_process.remaining_time == 0:
                running_process.finish_time = time + 1
                output.append(f"Time {(time + 1):>3} : {running_process.name} finished")
                running_process = None
        else:
            output.append(f"Time {time:>3} : Idle")

        time += 1

    output.append(f"Finished at time {total_time:>3}")
    calculate_process_metrics(processes, output)

    with open(output_filename, 'w') as f:
        f.write("\n".join(output) + "\n")


def fcfs_scheduler(process_count, total_time, processes, output_filename):
    output = [f"{process_count:>3} processes", "Using First-Come First-Served"]
    time = 0
    process_index = 0
    running_process = None

    while time < total_time:
        while process_index < len(processes) and processes[process_index].arrival == time:
            output.append(f"Time {time:>3} : {processes[process_index].name} arrived")
            process_index += 1

        if not running_process and process_index > 0:
            running_process = next((p for p in processes if p.finish_time == -1 and p.arrival <= time), None)
            if running_process:
                running_process.start_time = time
                output.append(f"Time {time:>3} : {running_process.name} selected (burst {running_process.burst:>3})")

        if running_process:
            running_process.remaining_time -= 1
            if running_process.remaining_time == 0:
                running_process.finish_time = time + 1
                output.append(f"Time {(time + 1):>3} : {running_process.name} finished")
                running_process = None
        else:
            output.append(f"Time {time:>3} : Idle")

        time += 1

    output.append(f"Finished at time {total_time:>3}")
    calculate_process_metrics(processes, output)

    with open(output_filename, "w") as f:
        f.write("\n".join(output) + "\n")

def rr_scheduler(process_count, total_time, processes, quantum, output_filename):
    output = [f"{process_count:>3} processes", "Using Round-Robin", f"Quantum {quantum:>3}"]
    time = 0
    ready_queue = deque()
    process_index = 0
    running_process = None
    quantum_counter = 0

    while time < total_time:
        while process_index < len(processes) and processes[process_index].arrival == time:
            output.append(f"Time {time:>3} : {processes[process_index].name} arrived")
            ready_queue.append(processes[process_index])
            process_index += 1

        if running_process and quantum_counter == quantum:
            ready_queue.append(running_process)
            running_process = None

        if not running_process and ready_queue:
            running_process = ready_queue.popleft()
            if running_process.start_time == -1:
                running_process.start_time = time
            output.append(f"Time {time:>3} : {running_process.name} selected (burst {running_process.remaining_time:>3})")
            quantum_counter = 0

        if running_process:
            running_process.remaining_time -= 1
            quantum_counter += 1
            if running_process.remaining_time == 0:
                running_process.finish_time = time + 1
                output.append(f"Time {(time + 1):>3} : {running_process.name} finished")
                running_process = None

        else:
            output.append(f"Time {time:>3} : Idle")

        time += 1

    output.append(f"Finished at time {total_time:>3}")
    calculate_process_metrics(processes, output)

    with open(output_filename, "w") as f:
        f.write("\n".join(output) + "\n")

if __name__ == "__main__":
    input_filename = sys.argv[1]
    output_filename = input_filename.replace(".in", "-student.out")
    process_count, total_time, processes, algorithm, quantum = parse_input(input_filename)

    if algorithm == "sjf":
        sjf_scheduler(process_count, total_time, processes, output_filename)
    elif algorithm == "fcfs":
        fcfs_scheduler(process_count, total_time, processes, output_filename)
    elif algorithm == "rr":
        if quantum is None:
            print("Error: Missing quantum parameter when using 'rr'")
            sys.exit(1)
        rr_scheduler(process_count, total_time, processes, quantum, output_filename)
    else:
        print("Error: Unknown scheduler type")
