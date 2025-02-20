import heapq
import sys
import os
from collections import deque

class Process:
    def __init__(self, name, arrival_time, burst_time):
        self.name = name
        self.arrival_time = arrival_time
        self.burst_time = burst_time
        self.remaining_time = burst_time
        self.start_time = -1
        self.completion_time = -1
        self.waiting_time = 0
        self.turnaround_time = 0
        self.response_time = -1

def fcfs_scheduling(processes, total_time):
    processes.sort(key=lambda p: p.arrival_time)
    time = 0
    timeline = []
    for process in processes:
        if time < process.arrival_time:
            while time < process.arrival_time:
                timeline.append(f"Time {time} : Idle")
                time += 1
        
        timeline.append(f"Time {process.arrival_time} : {process.name} arrived")
        process.start_time = time
        process.response_time = time - process.arrival_time
        timeline.append(f"Time {time} : {process.name} selected (burst {process.burst_time})")
        
        time += process.burst_time
        process.completion_time = time
        process.turnaround_time = process.completion_time - process.arrival_time
        process.waiting_time = process.turnaround_time - process.burst_time
        timeline.append(f"Time {time} : {process.name} finished")
    
    while time < total_time:
        timeline.append(f"Time {time} : Idle")
        time += 1
    
    timeline.append(f"Finished at time {total_time}")
    return processes, timeline

def sjf_preemptive_scheduling(processes, total_time):
    processes.sort(key=lambda p: (p.arrival_time, p.burst_time))
    pq = []  # Min-heap based on remaining burst time
    time = 0
    index = 0
    completed = 0
    n = len(processes)
    timeline = []
    
    while completed < n and time < total_time:
        while index < n and processes[index].arrival_time <= time:
            heapq.heappush(pq, (processes[index].remaining_time, index, processes[index]))
            timeline.append(f"Time {processes[index].arrival_time} : {processes[index].name} arrived")
            index += 1
        
        if pq:
            remaining_time, idx, process = heapq.heappop(pq)
            if process.response_time == -1:
                process.response_time = time - process.arrival_time
                timeline.append(f"Time {time} : {process.name} selected (burst {process.remaining_time})")
            
            process.remaining_time -= 1
            time += 1
            
            if process.remaining_time == 0:
                process.completion_time = time
                process.turnaround_time = process.completion_time - process.arrival_time
                process.waiting_time = process.turnaround_time - process.burst_time
                timeline.append(f"Time {time} : {process.name} finished")
                completed += 1
            else:
                heapq.heappush(pq, (process.remaining_time, idx, process))
        else:
            timeline.append(f"Time {time} : Idle")
            time += 1
    
    while time < total_time:
        timeline.append(f"Time {time} : Idle")
        time += 1
    
    timeline.append(f"Finished at time {total_time}")
    return processes, timeline

def parse_input_file(filename):
    processes = []
    total_time = 0
    scheduling_type = ""
    
    with open(filename, 'r') as file:
        lines = file.readlines()
        for line in lines:
            parts = line.split()
            if parts[0] == "processcount":
                process_count = int(parts[1])
            elif parts[0] == "runfor":
                total_time = int(parts[1])
            elif parts[0] == "use":
                scheduling_type = parts[1]
            elif parts[0] == "process":
                name = parts[2]
                arrival_time = int(parts[4])
                burst_time = int(parts[6])
                processes.append(Process(name, arrival_time, burst_time))
    return processes, total_time, scheduling_type

def write_output(filename, algorithm, timeline, processes):
    output_filename = os.path.splitext(filename)[0] + ".out"
    with open(output_filename, "w") as f:
        f.write(f"{len(processes)} processes\n")
        f.write(f"Using {algorithm}\n")
        f.write("\n".join(timeline) + "\n")
        f.write("\n")
        for p in processes:
            f.write(f"{p.name} wait {p.waiting_time} turnaround {p.turnaround_time} response {p.response_time}\n")

if __name__ == "__main__":
    if len(sys.argv) != 2 or not sys.argv[1].endswith(".in"):
        print("Usage: scheduler-gpt.py inputFile.in")
        sys.exit(1)
    
    filename = sys.argv[1]
    process_list, total_time, scheduling_type = parse_input_file(filename)
    
    if scheduling_type == "sjf":
        result, timeline = sjf_preemptive_scheduling(process_list, total_time)
    elif scheduling_type == "fcfs":
        result, timeline = fcfs_scheduling(process_list, total_time)
    else:
        print("Error: Unsupported scheduling type.")
        sys.exit(1)
    
    write_output(filename, scheduling_type.upper(), timeline, result)
