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

def parse_input(filename):
    processes = []
    total_time = 0
    process_count = 0
    
    with open(filename, 'r') as file:
        for line in file:
            parts = line.strip().split()
            if not parts:
                continue
            if parts[0] == "processcount":
                process_count = int(parts[1])
            elif parts[0] == "runfor":
                total_time = int(parts[1])
            elif parts[0] == "process" and parts[1] == "name":
                name = parts[2]
                arrival = int(parts[4])
                burst = int(parts[6])
                processes.append(Process(name, arrival, burst))
            elif parts[0] == "end":
                break
    
    return process_count, total_time, processes

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

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python scheduler.py inputfile.in")
        sys.exit(1)
    
    input_filename = sys.argv[1]
    output_filename = input_filename.rsplit('.', 1)[0] + ".out"
    process_count, total_time, processes = parse_input(input_filename)
    sjf_scheduler(process_count, total_time, processes, output_filename)