
# Hashx86 Operating System

<p align="center">
  <img width="500" height="500" alt="Image" src="https://github.com/user-attachments/assets/e7a30385-11d8-454c-9654-6f251f28f610" />
</p>


**Status**: 🚧 This project is currently under development.

Hashx86 is a minimalistic operating system built for the **x86 architecture**. Designed primarily for educational and experimental purposes, it provides basic functionality and serves as a foundation for exploring OS concepts and low-level system programming.

Simple yet effective, Hashx86 focuses on core features without unnecessary complexity.

---

## 🔧 About

This project focuses on low-level system development, implementing core operating system functionalities such as:

- Interrupt Service Routines (ISRs)
- Physical Memory Management (PMM)
- Dynamic Kernel Heap (KHeap)
- Direct video memory manipulation (supports both VGA Text and VGA Graphics modes)
- High-resolution graphics rendering via VESA
- Hardware interaction through custom drivers
- Paging
- ELF binary loading and execution
- System call interface for custom binaries
- Multitasking with process and thread management
- Basic event handling
- Widget-based GUI framework
- GUI performance optimization using draw caching
- Support for dynamic resolution switching via dynamic drivers
- HDD driver implementation
- FAT32 filesystem support (via FatFs R0.16)
- DMA / PCI driver framework
- Audio driver support
- Task State Segment (TSS) integration
---

## 🖼 Demonstrations

- 📺 **Taskbar, Dynamic BGA Graphics (High Resolution) and Sound Support**

  Seamless transition from boot to a high-resolution desktop environment with active sound support and taskbar integration.

  https://github.com/user-attachments/assets/e57feed9-6dd5-41ff-b930-78bbaea33a81

- 📺 **Concurrency & Kernel Error Handling**

  https://github.com/user-attachments/assets/63dbf463-cb2d-4e7b-83dc-c5fb6a2dcb69

- 🧪 **Early GUI (VGA Mode 320×200)**

  Initial implementation of the graphical interface using legacy VGA mode.

  https://github.com/user-attachments/assets/e4f244ec-4996-43c0-a84d-7cb74f135137

- 🧩 **Interrupt Service Routines**

  Custom ISRs tested for hardware event response and system stability.

  https://github.com/user-attachments/assets/859b1a6b-4d5c-47d7-a7fc-679594a35b53

---

## 🚀 Ready to Build Your Own OS?

Dive deep into the fundamentals of operating system development with our comprehensive tutorial series.

**👉 [Start the Tutorial Series](https://malakagunawardana.pages.dev/workshops/build-your-own-operating-system-from-scratch/)**

Learn to build your own digital world from the ground up!

---

## 🧪 Development Roadmap

Hashx86 is currently under active development. Upcoming improvements include:

- Desktop taskbar implementation with smooth animations
- Expanded system call library
- Thread-safe process management
- Networking support
- USB driver support

**Stay tuned for future updates!**

---

## 🛠 Build Instructions

> Prerequisites:
> - GCC cross-compiler for i686
> - GRUB and `xorriso` (for ISO generation)
> - `qemu-system-i386` and `qemu-img`
> - `make`

### 1. Clone the Repository
```bash
git clone https://github.com/Hashx86-OS/Hashx86-OS
cd Hashx86-OS
```

> Legacy repository (discontinued):
> The original Hashx86 repository is archived and no longer maintained.
> https://github.com/sdmdg/Hashx86

### 2. Install the OS

Choose one of the methods below:

**Option A: Quick Start (Recommended)**

- Download the installer ISO from the **Releases** page, then boot it to install the OS onto a disk.

To install onto an ATA disk in VirtualBox:

1. Create a new VM (e.g. *My Operating System*), OS type *Other/Unknown*, at least **1 GB RAM**.
2. Create a virtual **IDE (ATA)** hard disk (VDI, dynamically sized, at least **1 GB**). The installer only targets ATA disks, so use an IDE controller rather than SATA.
3. Mount the downloaded **installer ISO** as the VM's **Optical Drive**.
4. Start the VM. The installer will format the ATA disk and install the OS.
5. After installation, eject the ISO from the Optical Drive and reboot the VM to boot the installed system.

**Option B: Manual Setup**
If you prefer to create a fresh disk:
1. Clone the repository

    ```bash
    git clone https://github.com/Hashx86-OS/Hashx86-OS
    cd Hashx86-OS
    ```

2. Create a 1GB VirtualBox Disk Image (VDI):

    ```bash
    mkdir build
    make hddinit
    ```

3. Build the OS

    ```bash
    make build
    ```

4. Boot the OS in QEMU

    ```bash
    make runq
    ```

---

## 📄 License

This project is licensed under the MIT License. See `LICENSE` for more details.

---

## 👤 Author

Hashx86 is developed and maintained by **[@sdmdg](https://github.com/sdmdg)**.  
Built with ❤️ for learning and having fun with bare-metal programming.

---

## ❤️ Special Thanks

This project wouldn’t have been possible without the help, guidance and inspiration from:

- **[Viktor Engelmann](https://www.youtube.com/watch?v=1rnA6wpF0o4&list=PLHh55M_Kq4OApWScZyPl5HhgsTJS9MZ6M)** – *Write your own Operating System* YouTube series  
- **[OSDev.org](https://wiki.osdev.org/Main_Page)** – Incredible community and resources for OS development  
- **[lowlevel.eu](https://lowlevel.eu)** – Excellent tutorials on low-level programming  


## 🎨 Credits

* **TLSF 3.1:** Two-Level Segregated Fit memory allocator by **mattconte** (BSD 3-Clause) — [github.com/mattconte/tlsf](https://github.com/mattconte/tlsf)

* **FatFs R0.16:** Generic FAT Filesystem Module by **ChaN** on [elm-chan.org](https://elm-chan.org/fsw/ff/)

* **stb_truetype.h:** Single-header TTF rasterizer by **Sean Barrett** (public domain) — [github.com/nothings/stb](https://github.com/nothings/stb)

* **Sun fdlibm:** Freely Distributable LIBM by **Sun Microsystems** (public domain) — [netlib.org/fdlibm](https://www.netlib.org/fdlibm/)

* **PDCLib:** Public Domain C Library by **the PDCLib contributors** (CC0) — [github.com/DevSolar/pdclib](https://github.com/DevSolar/pdclib)

* **BootUp Sound:** "New Notification 09" by **Universfield** on [Pixabay](https://pixabay.com/sound-effects/film-special-effects-new-notification-09-352705/)

---
