# JOS: Educational Operating System – Technion CS Project

This repository contains my personal implementation of **JOS**, an educational operating system developed as part of a computer science course at the **Technion – Israel Institute of Technology**.

JOS is originally based on MIT's [6.828: Operating System Engineering](https://pdos.csail.mit.edu/6.828/) labs. The goal of the project is to gain hands-on experience building a Unix-like operating system from the ground up using C and x86 assembly.

## 📁 Project Structure

The project is divided into a series of labs, each focused on core operating system concepts:

- **Lab 1** – Boot loader and kernel entry
- **Lab 2** – Physical and virtual memory management
- **Lab 3** – User environments and context switching
- **Lab 4** – Multitasking and kernel preemption
- **Lab 5** – File system (optional)
  
Each stage incrementally adds key functionality to the OS.

## ✅ Features Implemented

- Bootable x86 kernel with protected mode support
- Page-based virtual memory and physical memory allocation
- Environment abstraction for user programs
- Trap handling and system calls
- Round-robin and yield-based scheduling
- Inter-process communication using syscalls
- (Optional) File system and networking (if implemented)

## 🛠 Building and Running

**Requirements**:

- `gcc`, `make`, `binutils`
- `qemu` (or another x86 emulator)
- Linux or WSL environmen
