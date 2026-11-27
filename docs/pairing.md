# Ghép 2 màn hình

Hai mạch nạp **cùng một firmware**. Ghép lại thì 2 panel 64×32 thành **1 màn hình dài 128×32**: chữ chạy liền từ panel này sang panel kia, màu cầu vồng nối liền.

## Tên WiFi khác nhau

Mỗi mạch phát WiFi `WIFI_AP_SSID` + `-` + **mã 4 ký tự** lấy từ MAC, ví dụ `HUB75-LED-A1B2` và `HUB75-LED-C3D4`.
Lúc khởi động, sau lời chào, panel hiện tên WiFi của nó vài giây (`SHOW_SSID_MS`).
Đang chạy riêng mà giữ nút BOOT ~2 giây thì panel hiện lại tên WiFi.

## Vai trò

| Vai trò | Phát WiFi | Web | Hiện gì |
|---|---|---|---|
| **Riêng** (mặc định) | có | có | nội dung của chính nó (64×32) |
| **Master** | có | có | nửa màn hình 128×32 theo cài đặt của nó |
| **Slave** | **không** | không | nửa còn lại, theo cài đặt của master |

Ghép nối được lưu NVS (namespace `link`): mất điện, có điện lại thì 2 mạch tự ghép tiếp.
Cài đặt riêng của slave **không bị xóa**: hủy ghép thì nó hiện lại nội dung cũ.

## Cách ghép

1. Bật cả 2 mạch, để gần nhau. Cả 2 đang chạy riêng.
2. Điện thoại vào WiFi của mạch muốn làm **master**, mở http://192.168.4.1.
3. Thẻ **Ghép 2 màn hình** → chọn master đặt **bên trái / bên phải** → bấm **Tìm màn hình khác**.
4. Mạch kia hiện `Mã: C3D4` trên panel. Trên web bấm **Ghép** ở dòng `HUB75-LED-C3D4`.
5. Mạch kia hiện `Ghép với A1B2`, tự khởi động lại, tắt WiFi, rồi hiện nửa còn lại.

Đổi bên trái/phải lúc đang ghép: bấm nút trên web, đổi ngay.

## Cách hủy ghép

- Trên web của master: **Hủy ghép**, hoặc
- **Giữ nút BOOT ~2 giây** trên mạch **bất kỳ** (`LINK_BUTTON_HOLD_MS`).

Cả 2 về chạy riêng. Slave khởi động lại để bật lại WiFi.
Nếu lúc hủy mạch kia đang tắt, khi nó bật lên sẽ tự nhận ra và cũng về chạy riêng.

> Chỉ *nhấn* BOOT lúc đang chạy, **đừng giữ BOOT lúc cắm điện / nhấn RESET** — giữ như vậy ESP32-S3 sẽ vào chế độ nạp code.

## Cách hoạt động

Truyền bằng **ESP-NOW** (không cần router, slave không cần kết nối WiFi). Mọi gói chạy trên giao diện STA, MAC STA là cố định nên dùng làm địa chỉ của mạch.

| Gói | Hướng | Khi nào |
|---|---|---|
| `DISCOVER` | master → tất cả (broadcast) | bấm Tìm, 1 lần/giây trong 3 giây |
| `HELLO` | mạch riêng → master | trả lời DISCOVER |
| `PAIR` / `PAIR_OK` | master ↔ slave | bấm Ghép |
| `DATA` | master → slave | cài đặt đổi (chia 3 mảnh ≤ 200 byte) |
| `SYNC` | master → slave | mỗi `LINK_SYNC_MS` (500 ms): đồng hồ master |
| `ALIVE` | slave → master | mỗi giây: báo bản nội dung đang có (thiếu thì master gửi lại) |
| `UNPAIR` | 2 chiều | hủy ghép |

**Khớp hình giữa 2 panel**

- Slave giữ độ lệch đồng hồ so với master (lấy mẫu từ mỗi gói SYNC/DATA, chọn mẫu trễ ít nhất). `link_now_ms()` = đồng hồ master.
- Mỗi lần cài đặt đổi, master ghi mốc `settings_epoch()` và gửi kèm.
- `app/screen.cpp` tính vị trí chạy chữ và pha màu **thẳng từ `now - epoch`** (không cộng dồn), nên 2 mạch luôn ra cùng một khung hình.
- Mỗi mạch chỉ vẽ phần `[view_x, view_x + 64)` của màn hình ảo (`link_view_x()`).

**Kênh WiFi**

ESP-NOW cần 2 mạch cùng kênh. Master dùng kênh của AP (`WIFI_AP_CHANNEL`, hoặc kênh router nếu có cấu hình WiFi nhà).
Slave nhớ kênh lần cuối thấy master. Mất master quá `LINK_LOST_MS` thì slave dò lần lượt kênh 1..13.

**Lưu ý**

- Khi **tìm**, chỉ thấy các mạch đang chạy riêng và **cùng kênh** với master. Nếu master có cấu hình WiFi nhà (`WIFI_STA_SSID`), nên ghép trước, hoặc để trống WiFi nhà.
- Nếu cấu hình WiFi nhà mà không vào được, master liên tục dò mạng → ESP-NOW có thể chập chờn.
- Gói không mã hóa. Ai có firmware này ở gần cũng có thể gửi lệnh ghép tới mạch đang chạy riêng.

## Code

| File | Việc |
|---|---|
| `app/link_state.*` | vai trò, bên trái/phải, đồng hồ chung, nội dung slave nhận được, lưu NVS |
| `net/link.*` | giao thức ESP-NOW, task `link` (core 0), nút BOOT |
| `hal/button.*` | đọc nút BOOT |
| `net/wifi.cpp` | tên WiFi theo MAC; slave không bật AP |
| `app/screen.cpp` | vẽ theo màn hình ảo + thời gian chung |
| `net/web_api.cpp` | `/api/link*` |
