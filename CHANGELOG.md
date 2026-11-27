# 2.3.0 — Rounded, rainbow và bảng màu tùy chọn

- Thiết kế lại 10 chữ số bằng đường cong, nét tròn 3 px, giữ ô 14×19.
- Thanh giây rainbow; màu giờ theo giờ, màu lịch theo ngày tại UTC+7.
- Hai bảng 4 màu gốc tùy chọn trên web, phối màu và lưu NVS độc lập Wi-Fi.
- Khi chưa có giờ: chỉ hiện SETTING hoặc CONNECTING, không hiện IP.

# 2.2.3 — Vạch giây giữa giờ và lịch

- Thêm vạch 60 LED cao 1 pixel tại y=21: thêm một LED mỗi giây, bắt đầu lại khi sang phút.
- Giữ nguyên font, vị trí giờ/lịch và màu hiện tại.

# 2.2.2 — Giờ Bold và dấu / khác màu

- Dùng font DejaVu Sans Condensed Bold thật, cao 19 px.
- Dấu / trong lịch luôn màu trắng; chữ lịch và giờ vẫn đổi màu mỗi giờ.

# 2.2.1 — Nét giờ đậm hơn và lịch dùng dấu /

- Tăng nét số giờ thêm 1 px ngang, giữ bố cục.
- Lịch dương `T6 27/11/26`, lịch âm `19/10 AL`.

# 2.2.0 — Font bitmap tự nhiên và chọn Wi-Fi

- Font số DejaVu ExtraLight 19 px, lịch Spleen 8 px: không kéo giãn, không dùng bảy đoạn.
- Trang cấu hình tự quét Wi-Fi, cho chọn SSID, hiển thị mức sóng/mạng mở và quét lại.
- Quét trong tác vụ mạng, không quét đồng thời với kết nối; giữ ô nhập tên mạng ẩn.

# 2.1.0 — Đồng hồ hai hàng, chữ lớn

- Tăng giờ lên 20 px và lịch lên 10 px; chỉ hiện hai hàng.
- Dấu hai chấm nháy theo giây; màu thay đổi theo giờ.
- Luân phiên `T6 27.11.2026` / `19.10 AL` mỗi 6 giây, trượt lịch vào 400 ms.
- Kiểm tra dấu hai chấm, bố cục và cắt hiệu ứng trong vùng lịch.

# 2.0.0 — Đồng hồ P5

- Giờ Việt Nam, thứ/ngày/tháng/năm và âm lịch có tháng nhuận.
- Cấu hình Wi-Fi bằng web, lưu NVS sau khi lấy giờ thành công.
- Đồng bộ NTP mỗi 24 giờ và tắt radio giữa các lần; BOOT mở cấu hình.
- Bitmap 3x5 vừa panel 64x32; kiểm tra lịch và giới hạn pixel.

# Nhật ký thay đổi

Định dạng theo [Keep a Changelog](https://keepachangelog.com/vi/1.1.0/), phiên bản theo [SemVer](https://semver.org/lang/vi/).
Phiên bản firmware khai báo ở `CMakeLists.txt` (`PROJECT_VER`).

## [1.2.0] - 2026-10-18

### Thêm
- **Profile**: lưu nội dung đang chỉnh (chữ, kiểu, cỡ, màu, hiệu ứng, tốc độ) thành tối đa 10 profile có tên, nhớ khi mất điện. Hiện / lưu đè / xóa trên web.
- **Trình chiếu**: tích các profile, chọn số giây cho từng cái, sắp thứ tự bằng ▲▼ → bảng LED tự chuyển lần lượt. Mất điện bật lại vẫn chiếu tiếp. Đổi slide không ghi flash. Khi ghép 2 màn hình, slave hiện theo.
- API `/api/profiles`, `/api/profile/save|load|delete`, `/api/show`.

### Sửa
- **WiFi load lâu / như mất kết nối**: thêm DNS + captive portal (điện thoại không phải chờ DNS / kiểm tra Internet hết giờ, tự mở trang điều khiển); web server không còn treo khi 1 điện thoại rớt sóng giữa lúc gửi; tăng số kết nối đồng thời (LWIP 16 socket, httpd 10) + TCP keep-alive dọn kết nối chết; AP chạy kênh 20 MHz; WiFi nhà (nếu cấu hình) thử lại giãn dần thay vì quét liên tục làm AP chập chờn.
- Kéo độ sáng chỉ gửi độ sáng (không làm chữ chạy lại từ đầu, không dừng trình chiếu).
- Web: mọi lệnh có giới hạn thời gian 6 giây và báo lỗi rõ ràng, lệnh đọc tự thử lại 1 lần.

## [1.1.0] - 2026-10-17

Ghi chú commit + danh sách file + checklist thử: [docs/changes/v1.1.0-ghep-2-man-hinh.md](docs/changes/v1.1.0-ghep-2-man-hinh.md)

### Thêm
- **Ghép 2 màn hình** qua ESP-NOW thành 1 màn hình 128×32: master phát WiFi + web, slave tắt WiFi và hiện nửa còn lại. Chạy chữ / cầu vồng đồng bộ theo đồng hồ chung.
- Thẻ *Ghép 2 màn hình* trên web: tìm mạch, ghép, chọn master bên trái/phải, hủy ghép. API `/api/link*`.
- Giữ nút BOOT ~2 giây: hủy ghép (cả 2 về chạy riêng); đang chạy riêng thì hiện tên WiFi.
- Ghép nối được nhớ khi mất điện (NVS `link`).

### Thay đổi
- Tên WiFi = `WIFI_AP_SSID` + mã 4 ký tự từ MAC (vd `HUB75-LED-A1B2`) → nạp 1 firmware cho nhiều mạch. Panel hiện tên WiFi sau lời chào.
- WiFi luôn chạy chế độ AP+STA (ESP-NOW dùng giao diện STA).
- Chạy chữ / đổi màu tính thẳng từ thời gian (không cộng dồn) để 2 mạch khớp từng pixel.

## [1.0.0] - 2026-10-08

### Thêm
- Web điều khiển qua WiFi (AP `HUB75-LED`, http://192.168.4.1): chữ, kiểu chữ, cỡ chữ, căn lề, màu, độ sáng, tốc độ.
- 1 hoặc 2 hàng chữ, mỗi hàng cài đặt riêng; tự chạy chữ khi dài hơn panel, đứng yên khi vừa.
- Hiệu ứng màu: một màu, cầu vồng chạy, đổi màu liên tục.
- 2 kiểu chữ dễ đọc trên LED (Đậm, Mỏng), cỡ chữ 6–17 px, chỉnh từng 1 px.
- Chữ tiếng Việt có dấu (134 chữ) ở mọi kiểu và cỡ.
- Công cụ sinh font `tools/gen_fonts`.
- Lưu cài đặt vào NVS; các bài test panel bật bằng `RUN_STARTUP_TESTS`.

### Cấu trúc
- Chia code theo lớp `config / hal / gfx / app / net / tests`.
- Thông tin WiFi tách ra `main/config/secrets.h` (không đưa lên git).
