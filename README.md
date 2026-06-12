# esp32-watch

ESP32-S3 smartwatch firmware — smart home voice frontend for [yuanfang-brain](https://github.com/YOUR_GITHUB_USER/yuanfang-brain)

**Target hardware:** Waveshare ESP32-S3-Touch-AMOLED-2.06
(ESP32-S3R8 + 2.06" 410×502 AMOLED + FT3168 touch + ES8311/ES7210 audio + QMI8658 IMU + AXP2101 PMU)

## 功能

- [x] WiFi STA + SmartConfig 配网 fallback
- [x] WebSocket 客户端 → Mac `192.168.1.10:7103` /ws/audio
- [x] 音频采集 (ES7210 I2S, 16kHz/16bit PCM → base64)
- [x] LVGL UI (麦克风按钮 + 状态 + 电池)
- [x] 触屏 FT3168
- [x] 电源管理 AXP2101
- [x] IMU QMI8658 + RTC PCF85063

## yuanfang-brain 协议

| 方向 | 格式 |
|------|------|
| ESP→服务器 | `{"type":"audio","data":"<base64 PCM 16k/16bit>","sr":16000}` |
| ESP→服务器 | `{"type":"text","text":"..."}` |
| 服务器→ESP | `{"type":"reply","text":"...","audio":"<base64>","intent":{...}}` |

## 目录结构

```
esp32-watch/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── README.md
└── main/
    ├── main.c
    ├── wifi.c / .h
    ├── ws_client.c / .h
    ├── audio.c / .h
    ├── display.c / .h
    ├── touch.c / .h
    ├── power.c / .h
    ├── sensors.c / .h
    └── ui.c / .h
```

## 编译 (macOS / Linux)

### 方式 A: 本地 ESP-IDF

```bash
# 1. 安装 ESP-IDF 5.4.2
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf && ./install.sh
. ./export.sh

# 2. 编译
cd ~/repos/esp32-watch
idf.py set-target esp32s3
idf.py build
```

### 方式 B: Docker (推荐无 ESP-IDF 环境)

```bash
cd ~/repos/esp32-watch
docker run --rm \
  -v "$(pwd):/project" \
  -w /project \
  espressif/idf:5.4.2 \
  idf.py set-target esp32s3 && idf.py build
```

## 烧录 5 步

```bash
# 1. 确认串口
ls /dev/cu.usbmodem*

# 2. 进入烧录模式
#    按住 BOOT(GPIO0) + 按 RESET(EN) + 先松 RESET + 再松 BOOT
#    或: 按住 PWR ≥ 6秒 (硬重启)

# 3. 擦除 (可选, 首次或异常时)
esptool.py --chip esp32s3 -p /dev/cu.usbmodemXXXX erase-flash

# 4. 烧录
esptool.py --chip esp32s3 -p /dev/cu.usbmodemXXXX write_flash \
  0x0     ~/esp/esp-idf/components/bootloader/bootloader.bin \
  0x8000  partitions.csv \
  0x10000 build/esp32-watch.bin \
  0x3F0000 ~/esp/esp-idf/components/esp_system/esp32s3/ota_data_initial.bin

# 5. 串口监控
idf.py -p /dev/cu.usbmodemXXXX monitor
```

> 分区表 `partitions.csv` 中 app0 从 `0x10000` 开始, 大小 0x3B0000 (分区表占 0x8000~0xE000)

## Mac 端 yuanfang-brain 启动

```bash
cd ~/repos/yuanfang-brain/voice
source ~/.venv/hermes/bin/activate   # 或 your venv
python ws_server.py
# 监听 ws://0.0.0.0:7103/ws/audio
```

**重启:** `pkill -f ws_server.py && python ws_server.py`

## 真机测试 Checklist

- [ ] 手表上电 → 屏幕亮
- [ ] BOOT+RESET → Mac 出现 `/dev/cu.usbmodem*`
- [ ] `idf.py monitor` 看到 "WiFi STA connected"
- [ ] 手表屏幕显示 "在线"
- [ ] 按麦克风按钮 → Mac ws_server.py 收到 audio 消息
- [ ] 服务器回复 reply → 手表屏幕更新状态
- [ ] 电池图标显示正确电量

## 已知限制

- `audio_pcm_to_b64()` 是 placeholder — 需用 `esp_crypto/base64.h` 实现真实 base64 编码
- LVGL `flush_cb` 需要根据 Waveshare QSPI 驱动填充真实 DMA flush 实现
- WiFi SSID/PASS 保存到 NVS 需配合 `wifi_connect_with_smartconfig()` 完成后写入

## Token / 密码保护

所有敏感信息 (WiFi 密码、API key) 通过环境变量或 NVS 传入，**永远不写死在源码中**。
