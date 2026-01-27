# NTMemory - Usermode NT Explorer
![Demo](res.gif)

## Overview

**NTMemory** is a fully usermode Windows NT Explorer.<br>
Query kernel addresses, translate virtual to physical addresses, inspect the PFN database, and more.

---

## Features

### Overview
- Physical memory usage tracking
- Page file statistics
- Commit charge monitoring
- File cache statistics
- Memory history graphs
- 8-priority standby list breakdown (P0-P7)
- Memory list tracking (zeroed, free, modified, modified no-write)
- Kernel pool statistics (paged and non-paged)
- Context switches and system calls counters
- Memory compression statistics (Windows 10+)

### Kernel Objects
- **Kernel address enumeration** - EPROCESS and KTHREAD pointers for all processes/threads

### Kernel Drivers
- Loaded kernel driver enumeration
- Driver load order, base addresses, sizes, and paths

### Ntoskrnl Exports
- **Kernel export resolution** - Full ntoskrnl.exe export table with calculated kernel addresses

### Physical Memory
- **Virtual to physical address translation** - Translate any kernel VA to PA from usermode via Superfetch
- **PFN database queries** - Query Page Frame Number database for page state and usage
- Physical memory range mapping

### Processes
- Per-process memory statistics (working set, private bytes, page faults)

### Pool Tags
- Kernel pool allocation tracking by tag
- Paged and non-paged pool usage breakdown
- Allocation counts per tag

### Handles
- System-wide handle count
- Handle type breakdown (File, Key, Event, Mutant, Section, Thread, Process, Token, Other)
- Per-process handle details
- Handle access rights and values

### Performance
- I/O operation metrics (read, write, other)
- Per-processor statistics (kernel time, user time, DPC time, interrupts)

### Prefetch
- Application launch history from prefetch files
- File sizes and last access timestamps

---

## Requirements

- Windows 10/11 (64-bit)
- Administrator privileges
- SysMain service (for Superfetch features)

---

## Building

### Requirements
- Visual Studio 2019+
- Windows SDK 10.0.19041.0+
- DirectX 11 SDK

### Dependencies
- Dear ImGui
- DirectX 11
- ntdll.lib

---

## License

This project is licensed under the terms of the MIT license.

---

## Acknowledgments

- [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut
- [Windows Internals](https://learn.microsoft.com/en-us/sysinternals/resources/windows-internals) by Yosifovich, Ionescu, Russinovich, Solomon
- [ReactOS](https://reactos.org/) project
- [Geoff Chappell](https://www.geoffchappell.com/studies/windows/km/index.htm) for undocumented Windows research
- [superfetch](https://github.com/jonomango/superfetch) by jonomango - inspiration for this project

---
