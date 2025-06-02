# JOS: My MIT Operating System Project

This repository contains my personal implementation of **JOS**, an educational operating system developed as part of MIT's [6.828: Operating System Engineering](https://pdos.csail.mit.edu/6.828/2022/) course.

JOS is a Unix-like teaching operating system written in C and x86 assembly. It runs on the x86 architecture and is designed to help understand fundamental OS concepts like virtual memory, processes, file systems, and kernels.

## 📁 Structure

The project is organized into a series of labs, each building on the previous one:

- `lab1`: Boot loader and low-level hardware initialization
- `lab2`: Memory management and virtual memory setup
- `lab3`: User environments and process management
- `lab4`: Preemptive multitasking and kernel scheduling
- `lab5`: File system support (optional lab)

Each lab is implemented in its own directory with self-contained code and documentation.

## 🚀 Features Implemented

- A bootable kernel that runs in protected mode
- Physical and virtual memory management using paging
- User-level process loading and context switching
- System call interface and user/kernel isolation
- Round-robin and priority-based multitasking
- Inter-process communication using message passing
- Optional: Basic file system interface and networking stack

## 🛠 How to Build and Run

You'll need:

- `gcc`, `make`, and `binutils`
- `qemu` (recommended emulator)
- A UNIX-like environment (Linux)

To build and run the kernel using QEMU:

```bash
make
make qemu
