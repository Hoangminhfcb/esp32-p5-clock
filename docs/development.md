# Quy ước code & Git

## Quy ước code

### Đặt tên

| Loại | Kiểu | Ví dụ |
|---|---|---|
| Hàm public | `module_hanh_dong` | `panel_present()`, `text_measure()`, `settings_get()` |
| Struct / enum | `PascalCase` | `DisplaySettings`, `TextMetrics` |
| Hằng số, macro | `UPPER_CASE` | `PANEL_W`, `FX_RAINBOW` |
| Biến toàn cục trong file | `s_` + `static` | `static uint8_t s_hue;` |
| File | `snake_case` | `display_task.cpp` |

Tiền tố module trùng với tên file: `panel_*` → `hal/panel.h`, `text_*` → `gfx/text.h`…

### Tổ chức file

- Mỗi module có cặp `.h` / `.cpp`. Đầu file `.h` ghi **module làm gì** và **cách dùng**.
- `#include` luôn ghi kèm thư mục: `#include "gfx/text.h"`, không viết `#include "text.h"`.
- Biến và hàm chỉ dùng trong 1 file thì để `static`.
- Không gọi ngược lớp (xem [architecture.md](architecture.md)). Ví dụ: `gfx/` không được include `app/`.
- Hằng số cấu hình đặt trong `config/app_config.h`. Thông tin bí mật đặt trong `config/secrets.h`.
- Thêm file `.cpp` thì phải khai báo trong `main/CMakeLists.txt`.

### Chú thích & log

- Chú thích viết tiếng Việt có dấu.
- Log `ESP_LOGx` viết **không dấu**, vì monitor trên Windows hay lỗi font.
- File trong `main/gfx/fonts/` được sinh tự động, **không sửa tay**.

## Git

### Lần đầu (đã có sẵn code trên máy)

Chạy `tools\git_setup.bat`. Script sẽ:

1. Xóa file trùng lặp và không còn dùng, bằng cách gọi `tools\clean_old_files.bat`. Script này cũng chạy riêng được bất cứ lúc nào.
2. `git init`, rồi đưa thư viện HUB75 vào dạng **submodule**.
3. Tạo commit đầu tiên.

Sau đó tạo repo trống trên GitHub/GitLab và đẩy lên:

```bash
git remote add origin https://github.com/<ten-ban>/esp32-p5-hub75.git
git push -u origin main
```

### Những gì KHÔNG đưa lên git (`.gitignore`)

| File | Lý do |
|---|---|
| `build/` | file sinh ra khi build |
| `sdkconfig` | tự sinh từ `sdkconfig.defaults` |
| `main/config/secrets.h` | mật khẩu WiFi |
| `.vscode/` | đường dẫn riêng của máy (COM port, thư mục ESP-IDF) |

Đổi cấu hình trong `menuconfig` thì chạy `idf.py save-defconfig` để cập nhật `sdkconfig.defaults`, rồi commit file đó.

### Nhánh & commit

- `main`: luôn build được.
- Tính năng mới: tạo nhánh riêng `feature/<ten>`, xong thì merge vào `main`.

```bash
git switch -c feature/hieu-ung-nhay
# ... sửa code, build thử ...
git add -A
git commit -m "feat: thêm hiệu ứng nháy chữ"
git switch main
git merge feature/hieu-ung-nhay
```

Đầu message commit dùng một trong các tiền tố:

| Tiền tố | Dùng khi |
|---|---|
| `feat:` | thêm tính năng |
| `fix:` | sửa lỗi |
| `refactor:` | sắp xếp lại code, không đổi tính năng |
| `docs:` | sửa tài liệu |
| `font:` | thêm / sinh lại font |
| `chore:` | cấu hình, build, linh tinh |

### Phát hành phiên bản

1. Tăng `PROJECT_VER` trong `CMakeLists.txt` (theo SemVer, ví dụ `1.1.0`). Phiên bản này hiện trên trang web.
2. Ghi thay đổi vào `CHANGELOG.md`.
3. Commit, rồi gắn tag:

```bash
git commit -am "chore: phát hành v1.1.0"
git tag v1.1.0
git push && git push --tags
```

### Thư viện HUB75 (submodule)

```bash
git submodule update --init                 # sau khi clone
cd components/ESP32-HUB75-MatrixPanel-DMA   # cập nhật thư viện lên bản mới
git pull origin master
cd ../..
git add components/ESP32-HUB75-MatrixPanel-DMA
git commit -m "chore: cập nhật thư viện HUB75"
```
