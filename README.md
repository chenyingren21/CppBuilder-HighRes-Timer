# 🚀 ThxTimerConsole (High-Precision Timer for C++Builder)

![Platform](https://img.shields.io/badge/Platform-Windows%2064--bit-blue)
![IDE](https://img.shields.io/badge/RAD%20Studio-12.2%20Athens%20(bcc64x)-red)
![C++](https://img.shields.io/badge/C%2B%2B-Modern%20Clang%2015-00599C)
![License](https://img.shields.io/badge/License-MIT-green)

**ThxTimerConsole** 是一個專為 Windows 平台設計的高性能計時器框架，基於 **C++Builder (Windows 64-bit Modern)** 開發。本專案核心在於突破 Windows 預設 15.6ms 的系統時鐘限制，結合底層系統呼叫與排程優化，達到工業級的穩定性與亞毫秒級（Sub-millisecond）的精確度。

---

## 🛠️ 核心模組介紹 (`Reuse\uFunction` Library)

本專案的核心能力封裝於 `Reuse\uFunction` 目錄下的獨立工具庫，方便直接移植至其他 C++Builder 專案：

| 模組名稱 | 技術特點 | 應用場景 |
| :--- | :--- | :--- |
| **`uThreadTimer`** | 支援 CPU 親和性 (Affinity) 綁定與 MMCSS 執行緒優先權提升 | 256Hz+ 高頻穩定迴圈、即時數據採集、飛行模擬器同步 |
| **`uStopWatch`** | 基於 `QueryPerformanceCounter` (QPC) 高精度計時，支援絕對時間戳校準 | 演算法效能分析、微秒級時間差測量 |
| **`uUseNtDll`** | 動態封裝底層 `NtSetTimerResolution` API | 將 Windows 系統計時器解析度從 15.6ms 提升至 0.5ms |
| **`uDosCmd`** | 異步匿名管道 (Anonymous Pipe) 通訊機制 | 非阻塞式執行外部 DOS/CLI 指令並即時串流讀取輸出 |
| **`uUtilize`** | 綜合系統與開發工具集 | 檔案操作、INI 設定檔讀寫、雜湊與加密校驗 |

---

## 📊 1 小時長時間壓力測試 (Performance Benchmark)

在高性能硬體環境下進行超過 1 小時的連續高頻觸發測試，驗證無累積時間漂移（Zero Drift）：

* **測試設備：** MSI Crosshair A16 (AMD Ryzen 9 7945HX)
* **測試時長：** `01:01:12` (超過 1 小時)
* **目標頻率：** `256 Hz` (理論週期 `3.906250 ms`)
* **總循環次數：** `940,057` 次
* **實測平均週期：** `3.906100 ms` (平均誤差僅 `0.00015 ms`)
* **整體準確度：** **`99.998%`**
* **抖動控制 (Jitter)：** 絕大多數 Tick 誤差控制在 `0.05 ms` 以內；全程 94 萬次觸發中，僅發生 **18 次** 輕微超出閾值之抖動。

> 🖼️ **測試截圖與日誌：** 詳見專案目錄內之「1 小時穩定性統計截圖」與「完整逐秒測試 Log 日誌」。

---

## ⚙️ 開發環境與編譯器架構

本專案完全基於 **Embarcadero RAD Studio 12.2 Athens** 開發，充分利用其新一代現代化工具鏈：

* **Compiler:** C++Builder Win64 Modern (`bcc64x`) — 基於 Clang 15 的高效能 64 位元編譯器。
* **Framework:** VCL / Console Application 混合架構。
* **SDK:** Windows 11 SDK (`10.0.22621.0`)。
* **Key System APIs:** `ntdll.dll` (核心計時器解析度控制)、`avrt.dll` (MMCSS 多媒體類別排程服務)。

**為什麼選擇 Win64 Modern (`bcc64x`)？**  
透過 Clang 15 架構引入的現代化指令集與迴圈優化，`uThreadTimer` 能以極低的執行緒上下文切換與指令開銷（Overhead）運行，這對於維持 256Hz 高頻且不產生周期性跳動至關重要。

---

## 🚀 快速開始 (Quick Start)

1. **Clone 專案至本地：**
   ```bash
   git clone [https://github.com/chenyingren21/CppBuilder-HighRes-Timer.git](https://github.com/chenyingren21/CppBuilder-HighRes-Timer.git)
開啟專案： 使用 RAD Studio / C++Builder 開啟 proj\console\ThxTimerConsole.cbproj。

確認路徑設定： 確保專案設定（Project Options）中的 Include Path 已包含 ./Reuse/uFunction。

選擇目標平台： 在 Project Manager 中，請務必切換至 Windows 64-bit (Modern) 平台進行編譯以獲得最佳執行效能。

📝 授權 (License)
本專案採用 MIT License 授權。歡迎自由使用、修改並提交 Issue 或 Pull Request 交流。
