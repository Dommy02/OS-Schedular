# OS Process Scheduler Simulator

Welcome to the **OS-Schedular** repository! This project implements a comprehensive Operating System CPU Scheduler Simulation developed in C and C++. It demonstrates core operating system concepts including process management, inter-process communication (IPC), and various CPU scheduling algorithms.

## 🚀 Features

- **Process Generator:** Reads process details from an input file and coordinates the initialization of the clock and scheduler.
- **Clock Module:** Simulates the system clock using IPC (Shared Memory) to keep all components synchronized.
- **CPU Scheduler:** Simulates context switching and manages the ready queue based on standard scheduling algorithms.
- **Process Simulation:** Simulated processes act as CPU-bound tasks, decrementing their remaining times and responding to signals from the scheduler.
- **IPC Mechanisms:** Utilizes Message Queues, Semaphores, and Shared Memory to facilitate robust communication between the generator, scheduler, and processes.

## 📁 Repository Structure

The project is divided into distinct phases:

- `Phase1/`: Contains the foundational components including the clock (`clk.c`), process generator (`process_generator.c`), and scheduler modules (with specific implementations like Round Robin).
- `Phase2/`: Contains extended features and memory management modules.
  
## ⚙️ Scheduling Algorithms Supported

This scheduler includes modular support for several algorithms, such as:
1. **Round Robin (RR)** (Noticeable via dedicated `schedulerRR.c` implementation)
2. **2-CPU architecture, each CPU will have its own First Come First Serve (FCFS) scheduler with a stealing mechanism**
3. **Highest Priority First (HPF)** 

## 💻 Getting Started

### Prerequisites

- A Linux/Unix-based environment (or WSL on Windows).
- GCC/G++ Compiler.
- `make` installed.
