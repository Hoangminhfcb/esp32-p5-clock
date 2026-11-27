# Ghep chu tieng Viet tu chu goc + dau (cho font khong co san chu tieng Viet).
#   chu goc (a, e, o, u ...) -> them dau phu (mu ^, trang ˘, moc ơ/ư) -> them dau thanh
#   Dau duoc ve bang cac mau pixel nho ben duoi, chon theo do lon cua font.
import unicodedata

ACUTE, GRAVE, HOOK, TILDE, DOT = chr(0x301), chr(0x300), chr(0x309), chr(0x303), chr(0x323)
BREVE, CIRC, HORN = chr(0x306), chr(0x302), chr(0x31B)
TONES = {ACUTE: "acute", GRAVE: "grave", HOOK: "hook", TILDE: "tilde", DOT: "dot"}

# 134 chu cai tieng Viet co dau (ke ca đ Đ)
VN_CHARS = sorted(c for c in {unicodedata.normalize("NFC", b + t)
                              for b in "aăâeêioôơuưyAĂÂEÊIOÔƠUƯY"
                              for t in ["", ACUTE, GRAVE, HOOK, TILDE, DOT]} | set("đĐ")
                  if ord(c) > 0x7E)

# Mau dau theo co chu: 0 = nho (chu hoa <= 6px), 1 = vua (7..9px), 2 = lon (>= 10px)
MARKS = [
    {   # nho
        "acute": [".#", "#."],
        "grave": ["#.", ".#"],
        "hook":  ["##", ".#", "#."],
        "tilde": [".#.#", "#.#."],
        "circ":  [".#.", "#.#"],
        "breve": ["#.#", ".#."],
        "dot":   ["#"],
    },
    {   # vua
        "acute": ["..#", ".#.", "#.."],
        "grave": ["#..", ".#.", "..#"],
        "hook":  ["##.", "..#", ".#."],
        "tilde": [".#..#", "#.##.", ],
        "circ":  ["..#..", ".#.#.", "#...#"],
        "breve": ["#...#", ".###."],
        "dot":   ["#"],
    },
    {   # lon
        "acute": ["..##", ".##.", "##.."],
        "grave": ["##..", ".##.", "..##"],
        "hook":  ["###.", "...#", "..#.", ".#.."],
        "tilde": [".##..#", "#..##."],
        "circ":  ["..##..", ".#..#.", "#....#"],
        "breve": ["#....#", ".####."],
        "dot":   ["##", "##"],
    },
]


def _bbox(rows, adv):
    ys = [y for y, r in enumerate(rows) if any(r[:adv])]
    xs = [x for x in range(adv) if any(r[x] for r in rows)]
    if not ys:
        return None
    return ys[0], ys[-1], xs[0], xs[-1]


def _copy(g):
    rows, adv = g
    return [r[:] for r in rows], adv


def _widen(g, new_adv, left=0):
    rows, adv = g
    rows = [[0] * left + r + [0] * (new_adv - adv - left) for r in rows]
    return rows, new_adv


def _stamp(g, shape, top, cx):
    """Ve mau 'shape' voi hang tren cung = top, tam ngang = cx. Tu noi rong neu tran."""
    rows, adv = g
    w = len(shape[0])
    x0 = int(round(cx - (w - 1) / 2))
    x0 = max(0, x0)
    if x0 + w > adv - 1:
        rows, adv = _widen((rows, adv), x0 + w + 1)
    for dy, line in enumerate(shape):
        for dx, c in enumerate(line):
            if c == "#" and 0 <= top + dy < len(rows):
                rows[top + dy][x0 + dx] = 1
    return rows, adv


class Composer:
    def __init__(self, glyphs, gap_mark=1, gap_stack=1, max_cls=2):
        """max_cls: giới hạn cỡ mẫu dấu (font mỏng dùng 1 để dấu không bị đậm)."""
        self.g = glyphs
        cap = _bbox(*glyphs["H"])
        self.cap_h = cap[1] - cap[0] + 1
        self.cls = min(max_cls, 0 if self.cap_h <= 6 else (1 if self.cap_h <= 9 else 2))
        self.m = MARKS[self.cls]
        self.gap_mark, self.gap_stack = gap_mark, gap_stack
        bb = _bbox(*glyphs["x"])
        self.baseline_row = bb[1]            # hang cuoi cua chu 'x'

    # --- dau phu ---------------------------------------------------------
    def _above(self, g, shape, cx, gap):
        rows, adv = g
        top = _bbox(rows, adv)[0]
        return _stamp(g, shape, top - gap - len(shape), cx)

    def _horn(self, g):
        rows, adv = g
        t, b, l, r = _bbox(rows, adv)
        n = 1 if self.cls == 0 else 2                # so pixel moc nho len tren
        rows, adv = _widen(g, r + 3) if r + 2 > adv - 1 else _copy(g)
        for y in range(t - n, t + 1):
            rows[y][r + 1] = 1
        return rows, adv

    def _dot_below(self, g, cx):
        shape = self.m["dot"]
        return _stamp(g, shape, self.baseline_row + 1 + self.gap_mark, cx)

    def _dotless_i(self, g):
        rows, adv = _copy(g)
        t, b, l, r = _bbox(rows, adv)
        # xoa phan cham: cac hang co muc nam tren hang trong dau tien
        y = t
        while y <= b and any(rows[y]):
            y += 1
        if y < b:
            for yy in range(t, y):
                rows[yy] = [0] * adv
        return rows, adv

    def _dbar(self, g, upper):
        rows, adv = _copy(g)
        t, b, l, r = _bbox(rows, adv)
        th = 2 if self.cls == 2 else 1
        if upper:                                    # Đ: gach ngang than trai, giua chu
            rows, adv = _widen((rows, adv), adv + 1, left=1)
            l += 1
            y = (t + b) // 2
            for yy in range(y, y + (th if self.cls == 2 else 1)):
                for x in range(l - 1, l + th + 1):
                    rows[yy][x] = 1
        else:                                        # đ: gach ngang gan dinh than phai
            y = t + (2 if self.cls == 2 else 1)
            if r + 2 > adv - 1:
                rows, adv = _widen((rows, adv), r + 3)
            for x in range(r - th, r + 2):
                rows[y][x] = 1
        return rows, adv

    # --- ghep 1 ky tu ----------------------------------------------------
    def compose(self, ch):
        if ch in self.g:
            return self.g[ch]
        if ch in "đĐ":
            return self._dbar(self.g["d" if ch == "đ" else "D"], ch == "Đ")
        nfd = unicodedata.normalize("NFD", ch)
        base, marks = nfd[0], nfd[1:]
        mod = next((m for m in marks if m in (BREVE, CIRC, HORN)), None)
        tone = next((m for m in marks if m in TONES), None)

        # uu tien chu co san cua font cho phan "chu + dau phu" (ă â ê ô ơ ư)
        start = unicodedata.normalize("NFC", base + mod) if mod else base
        if start in self.g:
            g = _copy(self.g[start])
            mod = None
        else:
            g = _copy(self.g[base])
        if base == "i" and tone and tone != DOT:
            g = self._dotless_i(g)

        bb = _bbox(*g)
        cx = (bb[2] + bb[3]) / 2

        if mod == HORN:
            g = self._horn(g)
        elif mod == CIRC:
            g = self._above(g, self.m["circ"], cx, self.gap_mark)
        elif mod == BREVE:
            g = self._above(g, self.m["breve"], cx, self.gap_mark)

        if tone == DOT:
            g = self._dot_below(g, cx)
        elif tone:
            stacked = start in self.g and start != base or mod in (CIRC, BREVE)
            gap = self.gap_stack if stacked else self.gap_mark
            g = self._above(g, self.m[TONES[tone]], cx, gap)
        return g
