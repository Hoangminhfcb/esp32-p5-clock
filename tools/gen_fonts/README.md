# Sinh font cho panel

`gen_fonts.py` render các kiểu chữ trong `font_src/` ở **mọi cỡ** rồi sinh mã C++ vào `main/gfx/fonts/`:

- `font_<kiểu>.h`: bitmap từng cỡ
- `fonts.cpp`: danh sách kiểu chữ

**Không sửa tay** các file đã sinh.

```bash
pip install pillow fonttools
python tools/gen_fonts/gen_fonts.py            # sinh lại
python tools/gen_fonts/gen_fonts.py --preview  # thêm ảnh preview.png để xem trước
```

## Cỡ chữ

- **Cỡ = chiều cao chữ IN HOA (px).** Script tìm cỡ render sao cho chữ `H` cao đúng N px, với mọi N từ `SIZE_MIN` đến `SIZE_MAX`.
- Vì vậy trên web, mỗi lần bấm +/− chữ cao thêm/bớt đúng 1 px.
- Script tự dừng ở cỡ lớn nhất mà chữ (kể cả dấu tiếng Việt) còn vừa 32 px.
- Font render **không khử răng cưa, có hinting**, nên nét chữ khớp lưới LED.

## File

| File | Việc |
|---|---|
| `gen_fonts.py` | danh sách kiểu chữ (`FAMILIES`), khoảng cỡ, cắt hàng trống, ghi file C++ |
| `fontlib.py` | render TTF thành bitmap, tìm cỡ render cho chiều cao chữ hoa |
| `vietnamese.py` | ghép dấu tiếng Việt bằng mẫu pixel (dùng cho cỡ < `NATIVE_VN_FROM`) |
| `font_src/` | file TTF gốc + giấy phép |

## Thêm kiểu chữ

1. Chép file `.ttf` vào `font_src/`.
   - Chọn font sans-serif đậm, nét đều, **có hinting** và **đủ chữ tiếng Việt**: DejaVu, Liberation Sans, FreeSans…
   - Font có chân (serif) hoặc nét mảnh khó đọc trên LED.
2. Thêm 1 dòng vào **cuối** `FAMILIES` trong `gen_fonts.py`:
   ```python
   ("ten_id", "Tên hiển thị", [(6, "File.ttf")], False, "License"),
   ```
   - `[(6, "A.ttf"), (13, "B.ttf")]` nghĩa là cỡ 6–12 dùng A, từ 13 dùng B. Kiểu "Mỏng" dùng cách này để nét vẫn mảnh ở cỡ lớn.
   - `True` ở tham số thứ 4 nếu là kiểu nét mảnh: dấu tiếng Việt ghép sẽ dùng mẫu mảnh.
3. Chạy `python tools/gen_fonts/gen_fonts.py --preview` và xem `preview.png`.
4. Build lại firmware. Kiểu chữ mới tự hiện trên web.

> Thứ tự trong `FAMILIES` chính là **id kiểu chữ**. Chỉ thêm vào cuối, để cài đặt đã lưu không trỏ nhầm kiểu.

Mỗi kiểu chữ chiếm khoảng 120 KB flash. Tổng dung lượng được in ra khi chạy script.

## Dấu tiếng Việt ở cỡ nhỏ

Ở cỡ nhỏ, dấu có sẵn của font dễ bị dính hoặc mất nét. Vì vậy với cỡ nhỏ hơn `NATIVE_VN_FROM` (mặc định 12), script tự ghép dấu bằng mẫu pixel trong `MARKS` (`vietnamese.py`).

- Mẫu có 3 cỡ, chọn theo chiều cao chữ hoa: nhỏ ≤ 6 px, vừa 7–9 px, lớn ≥ 10 px.
- Mỗi chuỗi trong mẫu là 1 hàng pixel, `#` là điểm sáng.
