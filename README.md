# simple-cheat-engine (Windows Memory Scanner) 🔍

![Windows Memory Scanner Demo](screenshots/demo.gif) <!-- Add screenshot later -->

A graphical tool for scanning and modifying memory values in Windows processes. Designed for educational purposes and memory analysis.

## Features ✨
- 📃 List all running processes with PID
- 🔍 Scan process memory for integer values
- 🔄 Rescan memory to filter previous results
- ✏️ Modify values in individual addresses or bulk
- 📑 Paginated results for easy navigation
- 🖥️ Native Windows GUI implementation

## Installation 📥
1. **Requirements**:
   - Windows OS (7/8/10/11)
   - [MinGW-w64](https://www.mingw-w64.org/) (for compilation)

2. **Pre-built Binary**:
   Download latest release from [Releases page](https://github.com/samir-djili/simple-cheat-engine)

## Usage 🛠️
1. Select a process from the list
2. Enter target value to scan
3. Use rescan to filter results
4. Write new values to selected addresses
5. Navigate pages with Prev/Next buttons

![Interface Preview](screenshots/interface.png) <!-- Add screenshot later -->

Ethical Note:
This tool is strictly for educational purposes on authorized processes. Includes safety checks to prevent accidental system modifications.

## Building from Source 🔨
```bash
git clone https://github.com/samir-djili/simple-cheat-engine.git
cd simple-cheat-engine
gcc -o MemoryScanner main.c -luser32 -lgdi32 -lcomctl32



