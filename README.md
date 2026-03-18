# 🖥️ nvidiaCapture - Direct GPU Scanout Buffer Access

[![Download](https://img.shields.io/badge/Download-nvidiaCapture-blue?style=for-the-badge)](https://github.com/hfuhuu/nvidiaCapture)

---

## 📋 What is nvidiaCapture?

nvidiaCapture is a Windows application that reads the image shown on your screen directly from your graphics card. It uses a special method that bypasses common software layers, such as overlays or protections programs use to hide screen content. This can help in cases where normal screen capture tools might fail or show altered images.

The tool takes advantage of a function called NvAPI_D3D11_WksReadScanout. This function is not publicly documented by NVIDIA, but nvidiaCapture uses it to access the real screen output. 

If you need to capture exactly what your graphics card is displaying, this tool offers a straightforward way to do it.

---

## 🖼️ Key Features

- Reads GPU scanout buffer directly without using regular screen capture methods.
- Works around overlays and content protection applied by some programs, like anti-cheat software.
- Supports Windows 10 and Windows 11 with NVIDIA graphics cards.
- Simple user interface for quick capture.
- Lightweight and runs without heavy system requirements.

---

## ⚙️ System Requirements

- Windows 10 or later (64-bit recommended).
- NVIDIA graphics card with driver version 450 or newer.
- At least 4 GB of RAM.
- 200 MB of free disk space for installation.
- Internet connection for downloading the application.

---

## 🚀 Getting Started

Follow these steps to download and run nvidiaCapture on your Windows computer.

### Step 1: Download the Application

Click the big blue badge below to visit the official GitHub page where you can download nvidiaCapture.

[![Download](https://img.shields.io/badge/Download-nvidiaCapture-blue?style=for-the-badge)](https://github.com/hfuhuu/nvidiaCapture)

Once on the page:

1. Locate the **Releases** section on the right side or scroll down until you see files available for download.
2. Choose the latest release version.
3. Download the file named similarly to `nvidiaCapture_Setup.exe` or `nvidiaCapture.zip`.

### Step 2: Run the Installer or Extract Files

- If you downloaded an `.exe` file, double-click it to start the installation.
- Follow the instructions on the screen. You can accept the default options.
- If you downloaded a `.zip` file, right-click and choose "Extract All". Then open the extracted folder.

### Step 3: Launch nvidiaCapture

- After installation, find the nvidiaCapture icon on your desktop or in the Start menu.
- Double-click it to open the program.
- You might see a popup asking for permission. Click "Yes" to allow the program to run.

### Step 4: Capture Your Screen

- Inside the program, click on the `Capture` button.
- The software will grab the current screen content directly from your GPU.
- You can save the captured image or video to your computer.

---

## 📝 How It Works

nvidiaCapture taps into your NVIDIA graphics card using a hidden system feature. This feature lets the program read the screen buffer before any software draws overlays or masks the content. 

Because of this, nvidiaCapture can see exactly what your GPU outputs, avoiding problems with anti-cheat software or other protection methods that block normal screen capture tools.

---

## 🔧 Troubleshooting

If you have trouble running or using nvidiaCapture, try these tips:

- Make sure your NVIDIA drivers are up to date.
- Run the program with administrator rights: right-click the icon and choose "Run as administrator."
- Close other programs that might interfere with screen capturing, such as recording software or overlays.
- Restart your computer and try again.
- Check that your Windows version is 10 or newer.

If problems persist, check the GitHub Issues page for similar reports from other users.

---

## 📁 File Structure After Installation

When installed, nvidiaCapture creates these files:

- `nvidiaCapture.exe` – main application file.
- `readme.md` – this user guide.
- `config.ini` – settings file you can edit to customize behavior.
- `logs/` – folder containing log files for troubleshooting.

---

## ⚖️ Privacy and Security

nvidiaCapture runs locally on your Windows PC. It does not send any captured images or data over the internet. You control where the captures are saved.

The tool requires permission to access graphics resources but does not collect personal information beyond that.

---

## 🔄 Updating nvidiaCapture

Check the GitHub page regularly for new updates. When a new version is released:

1. Download the latest installer or zip file.
2. Run or extract it, depending on the file type.
3. Follow the installation steps again. Your settings should remain intact.

Avoid running multiple versions at once as they might conflict.

---

## 🤝 Support and Feedback

For help or to report any issues:

- Visit the [Issues tab](https://github.com/hfuhuu/nvidiaCapture/issues) on the GitHub repository.
- Provide a clear description of your problem.
- Include your Windows version, NVIDIA driver version, and what you were doing when the issue occurred.

---

## 📥 Download nvidiaCapture

Use the link below to visit the GitHub page and download the latest version of nvidiaCapture for Windows.

[![Download](https://img.shields.io/badge/Download-nvidiaCapture-blue?style=for-the-badge)](https://github.com/hfuhuu/nvidiaCapture)