# Web API

Server chạy trên ESP32, cổng 80. Khi kết nối vào AP của ESP32, địa chỉ là `http://192.168.4.1`.

## Trang tĩnh

| URL | File nguồn |
|---|---|
| `GET /` | `main/net/www/index.html` |
| `GET /style.css` | `main/net/www/style.css` |
| `GET /app.js` | `main/net/www/app.js` |

Các file được nhúng vào firmware lúc build (`EMBED_TXTFILES` trong `main/CMakeLists.txt`).
Thêm file mới thì phải khai báo ở **cả hai** chỗ: `EMBED_TXTFILES` và bảng `FILES[]` trong `net/web_server.cpp`.

## `GET /api/state`

Trả về cài đặt hiện tại và thông tin để vẽ giao diện.

```json
{
  "version": "1.0.0",
  "panel": { "w": 64, "h": 32 },
  "maxLines": 2, "maxBytes": 255,
  "lines": 2, "brightness": 128, "fx": 5,
  "line": [
    { "text": "Xin chào", "font": 0, "size": 12, "align": 1, "effect": 0, "color": "#00ff00", "speed": 30,
      "width": 52, "height": 17, "scroll": false },
    { "text": "ESP32 LED", "font": 1, "size": 8, "align": 1, "effect": 1, "color": "#00c8ff", "speed": 30,
      "width": 48, "height": 11, "scroll": false }
  ],
  "fonts": [ { "id": 0, "name": "Đậm", "min": 6, "max": 17 }, { "id": 1, "name": "Mỏng", "min": 6, "max": 17 } ]
}
```

`panel.w` là chiều rộng màn hình ảo: 64 khi chạy riêng, **128 khi đang ghép 2 màn hình**. `scroll` tính theo chiều rộng này.

`width` / `height` là kích thước thật của hàng chữ trên panel (px). `scroll` = `true` khi chữ dài hơn panel và đang chạy chữ.

## `POST /api/set`

Body dạng `application/x-www-form-urlencoded`. **Chỉ cần gửi các key muốn đổi.** Trả về JSON giống `/api/state`.

| Key | Giá trị |
|---|---|
| `lines` | 1..2 |
| `brightness` | 0..255 |
| `fx` | 1..10 (tốc độ đổi màu) |
| `textN` | chữ UTF-8 của hàng N (N = 0, 1), ≤ 255 byte |
| `fontN` | id kiểu chữ (xem `fonts` trong `/api/state`) |
| `sizeN` | cỡ chữ = chiều cao chữ in hoa (px), trong khoảng `min..max` của kiểu chữ |
| `alignN` | 0 trái · 1 giữa · 2 phải |
| `effectN` | 0 một màu · 1 cầu vồng · 2 đổi màu |
| `colorN` | `#rrggbb` |
| `speedN` | ms/pixel khi chạy chữ (10..500) |

Ví dụ:

```bash
curl -X POST http://192.168.4.1/api/set \
     --data-urlencode "text0=Chúc mừng năm mới" \
     -d "effect0=1" -d "lines=1"
```

ESP32 tự kiểm tra và sửa các giá trị sai. Ví dụ: nếu tổng chiều cao các hàng vượt 32 px, cỡ chữ của hàng cao nhất bị giảm dần từng 1 px cho đến khi vừa.

Chỉ gửi `brightness` → chỉ đổi độ sáng (không dừng trình chiếu). Gửi bất kỳ key nội dung nào → **dừng trình chiếu** và lưu nội dung mới.

`GET /api/state` luôn trả nội dung **đã lưu** (nội dung trên web), kể cả khi panel đang trình chiếu profile khác.

## `POST /api/reset`

Đưa cài đặt về mặc định (`config/app_config.h`). Trả về JSON giống `/api/state`.

## Ghép 2 màn hình (`/api/link*`)

Chi tiết cách hoạt động: [pairing.md](pairing.md). Các lệnh POST chạy bất đồng bộ; tất cả đều trả về JSON giống `GET /api/link`, web hỏi lại `GET /api/link` để biết kết quả.

### `GET /api/link`

```json
{
  "self": "A1B2", "ssid": "HUB75-LED-A1B2",
  "role": "standalone", "side": 0, "peer": "", "online": false,
  "scanning": false, "pairing": false, "pairFailed": false,
  "found": [ { "id": "C3D4", "mac": "aa:bb:cc:dd:c3:d4", "rssi": -41 } ]
}
```

| Trường | Ý nghĩa |
|---|---|
| `role` | `standalone` (chạy riêng) · `master` (đang ghép) |
| `side` | 0 = master bên trái · 1 = bên phải |
| `peer`, `online` | mã mạch đang ghép, còn nghe thấy nó không |
| `found` | mạch tìm thấy sau `/api/link/scan` |

| Lệnh | Body | Việc |
|---|---|---|
| `POST /api/link/scan` | — | tìm mạch đang chạy riêng ở gần (~3 giây) |
| `POST /api/link/pair` | `mac=aa:bb:cc:dd:ee:ff` | ghép: mạch này làm master, mạch kia làm slave |
| `POST /api/link/unpair` | — | hủy ghép, cả 2 về chạy riêng |
| `POST /api/link/side` | `side=0` hoặc `1` | master đặt bên trái / phải |

## Profile + trình chiếu

### `GET /api/profiles`

```json
{ "max": 10, "nameMax": 47,
  "items": [ { "slot": 0, "name": "Khai trương" }, { "slot": 3, "name": "Giảm giá" } ],
  "show": { "on": true, "cur": 3, "left": 4200, "min": 2, "maxSecs": 3600,
            "items": [ { "slot": 3, "secs": 10 }, { "slot": 0, "secs": 5 } ] } }
```

`show.items` là danh sách chiếu theo thứ tự; `cur` = ô đang chiếu, `left` = ms còn lại của slide.

| Lệnh | Body | Việc |
|---|---|---|
| `POST /api/profile/save` | `name`, `slot` (bỏ = ô trống mới) + các key như `/api/set` | lưu profile (key không gửi lấy theo nội dung đã lưu). Lưu đè không gửi `name` thì giữ tên cũ. Trả JSON `/api/profiles`; đầy → 400 |
| `POST /api/profile/load` | `slot` | hiện profile + lưu làm nội dung chính, dừng trình chiếu. Trả JSON `/api/state` |
| `POST /api/profile/delete` | `slot` | xóa (tự bỏ khỏi danh sách chiếu) |
| `POST /api/show` | `items=3:10,0:5` và/hoặc `on=0\|1` | đặt danh sách chiếu (ô:giây) / bật tắt |

## Captive portal

Mọi đường dẫn không có ở trên → `302` về `http://192.168.4.1/`, và DNS của ESP32 trả mọi tên miền về 192.168.4.1. Tắt bằng `WIFI_CAPTIVE_PORTAL 0` trong `config/app_config.h`.
