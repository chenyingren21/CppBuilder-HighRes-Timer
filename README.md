# 🚀 ThxTimerConsole (High-Precision Timer)

**ThxTimerConsole** 是一個專為 Windows 平台設計的高性能計時器框架，基於 **C++Builder (Windows 64-bit Modern)** 開發。本專案核心在於突破 Windows 預設的計時限制，達到工業級的穩定性與毫秒級的精確度。

---

## 📊 效能表現 (Performance Benchmark)

在高性能設備上經過長達一小時的壓力測試，結果如下：

*   **目標頻率**：256 Hz (週期約 3.90625 ms)
*   **平均準確度**：**99.998%**
*   **抖動控制 (Jitter)**：絕大多數 Tick 誤差控制在 **0.05ms** 以內。
*   **穩定性**：具備自動漂移補償機制，長時間運行不累積時間差。

---

## 🛠️ 核心模組介紹 (uFunction Library)

本專案的強大源於 `Reuse\uFunction` 目錄下的獨立工具庫：

| 模組名稱 | 技術特點 | 應用場景 |
| :--- | :--- | :--- |
| **uThreadTimer** | 支援 CPU 親和性 (Affinity) 與 MMCSS 優先權。 | 高頻穩定迴圈、即時數據採集。 |
| **uStopWatch** | 基於 QPC 的高精度計時，支援絕對時間戳校準。 | 效能分析、精確時間差測量。 |
| **uUseNtDll** | 封裝 `NtSetTimerResolution`。 | 將系統解析度從 15.6ms 提升至 **0.5ms**。 |
| **uDosCmd** | 異步管道 (Pipe) 通訊。 | 非阻塞式執行 DOS 指令並即時讀取輸出。 |
| **uUtilize** | 綜合工具集。 | 檔案、INI、加密校驗等日常開發工具。 |

---

## 💻 測試環境 (Hardware)

*   **CPU**: AMD Ryzen 9 7945HX (16C/32T)
*   **RAM**: 64GB DDR5 5200MT/s
*   **OS**: Windows 11 Pro 64-bit
*   **IDE**: RAD Studio 12.x (C++ 64-bit Modern Compiler)

---

## 🚀 快速開始

1. **Clone 專案**：
   ```bash
   git clone [https://github.com/你的帳號/ThxTimerConsole.git](https://github.com/你的帳號/ThxTimerConsole.git)
開啟專案：
使用 C++Builder 開啟 proj\console\ThxTimerConsole.cbproj。

設定路徑：
確保專案設定中的 Include Path 包含 ./Reuse/uFunction。

編譯平台：
請務必選擇 Windows 64-bit (Modern) 以獲得最佳效能。

📝 授權 (License)
本專案採用 MIT License。歡迎自由使用、修改並提交 Pull Request。
