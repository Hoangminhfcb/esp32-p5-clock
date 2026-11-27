// =====================================================================
//  app.js — Giao diện web điều khiển bảng LED HUB75
//
//  Trạng thái:
//    S   cài đặt đang chỉnh (giống DisplaySettings trong firmware)
//    E   hàng đang chỉnh (0 hoặc 1)
//  Bấm "Gửi" -> POST /api/set -> ESP32 lưu, tự chỉnh giá trị sai,
//  trả lại state mới (kèm chữ đang "đứng yên" hay "chạy chữ").
//  Profile: lưu nội dung đang chỉnh để dùng lại / trình chiếu (cuối file).
//  API: docs/web-api.md
// =====================================================================
const $ = id => document.getElementById(id);
const SWATCHES = ['#ff0000','#ff7a00','#ffd400','#00ff00','#00e5ff','#0060ff','#b000ff','#ff2fa0','#ffffff'];
let S = null;          // state đang chỉnh
let FONTS = [];        // [{id, name, min, max}]
let MAXB = 255;        // số byte tối đa / hàng
let E = 0;             // hàng đang chỉnh
let sent = null;       // state ESP32 trả về lần gần nhất (để hiện trạng thái)

const cur = () => S.line[E];
const norm = t => t.normalize('NFC').replace(/[\r\n\t]+/g, ' ');
const bytes = t => new TextEncoder().encode(norm(t)).length;

// =====================================================================
//  HIỂN THỊ — đưa S lên các nút / ô nhập
// =====================================================================
function seg(id, v){ for (const b of $(id).children) b.classList.toggle('on', +b.dataset.v === v); }

function renderSize(){
  const l = cur(), f = FONTS[l.font];
  $('size').textContent = l.size + ' px';
  $('sizeDown').disabled = l.size <= f.min;
  $('sizeUp').disabled   = l.size >= f.max;
}

function renderStatus(){
  const box = $('status');
  const st = sent && sent.line[E];
  // chỉ hiện trạng thái khi hàng này chưa bị sửa sau lần gửi
  const same = st && st.text === norm(cur().text) && st.font === cur().font && st.size === cur().size;
  if (!same){ box.textContent = 'Chưa gửi'; return; }
  box.innerHTML = `Trên panel: rộng ${st.width}px, cao ${st.height}px · ` +
    (st.scroll ? '<span class="tag sc">dài hơn màn hình → chạy chữ</span>'
               : '<span class="tag">vừa màn hình → đứng yên</span>');
}

function renderLine(){
  const l = cur();
  $('text').value = l.text;
  seg('font', l.font); seg('align', l.align); seg('effect', l.effect);
  $('sw').classList.toggle('hide', l.effect != 0);
  for (const i of $('sw').querySelectorAll('i')) i.classList.toggle('on', i.dataset.c === l.color);
  $('color').value = l.color;
  $('speed').value = l.speed; $('sV').textContent = l.speed + ' ms/px';
  renderSize(); renderCount(); renderStatus();
}

function renderAll(){
  seg('lines', S.lines);
  $('tabs').classList.toggle('hide', S.lines < 2);
  if (S.lines < 2) E = 0;
  seg('tabs', E);
  $('brightness').value = S.brightness; $('bV').textContent = Math.round(S.brightness / 2.55) + '%';
  $('fx').value = S.fx; $('fV').textContent = S.fx;
  renderLine();
}

function renderCount(){
  const n = bytes(cur().text);
  $('cnt').textContent = `${n}/${MAXB} byte`;
  $('cnt').style.color = n > MAXB ? 'var(--err)' : '';
}

function msg(t, ok = true){ $('msg').textContent = t; $('msg').style.color = ok ? 'var(--acc)' : 'var(--err)'; }

// Gọi API có giới hạn thời gian: WiFi chập chờn thì báo lỗi sau vài giây
// (thay vì treo mãi), lệnh GET tự thử lại 1 lần.
async function api(url, params, timeout = 6000){
  const post = params !== undefined;
  for (let tries = post ? 1 : 2; tries > 0; tries--){
    const ctl = new AbortController();
    const t = setTimeout(() => ctl.abort(), timeout);
    try {
      const r = await fetch(url, post ? { method: 'POST', body: params, signal: ctl.signal }
                                      : { signal: ctl.signal, cache: 'no-store' });
      if (!r.ok) throw new Error(await r.text().catch(() => '') || ('HTTP ' + r.status));
      return await r.json();
    } catch(e){
      if (tries <= 1) throw e;
    } finally { clearTimeout(t); }
  }
}

// =====================================================================
//  GIAO TIẾP ESP32
// =====================================================================
function apply(st){
  sent = st;
  S = { lines: st.lines, brightness: st.brightness, fx: st.fx,
        line: st.line.map(l => ({ text: l.text, font: l.font, size: l.size, align: l.align,
                                  effect: l.effect, color: l.color, speed: l.speed })) };
  renderAll();
}

function cutBytes(t){                         // cắt chữ cho vừa số byte tối đa
  t = norm(t);
  while (bytes(t) > MAXB) t = [...t].slice(0, -1).join('');
  return t;
}

// Nội dung đang chỉnh -> form (dùng cho Gửi và Lưu profile)
function contentParams(){
  const p = new URLSearchParams({ lines: S.lines, fx: S.fx });
  S.line.forEach((l, i) => {
    p.set('text' + i, cutBytes(l.text)); p.set('font' + i, l.font); p.set('size' + i, l.size);
    p.set('align' + i, l.align); p.set('effect' + i, l.effect); p.set('color' + i, l.color); p.set('speed' + i, l.speed);
  });
  return p;
}

async function send(){
  const p = contentParams();
  p.set('brightness', S.brightness);
  try {
    const before = S.line.map(l => l.size);
    const wasShow = P && P.show.on;
    apply(await api('/api/set', p));
    const shrunk = S.line.some((l, i) => i < S.lines && l.size < before[i]);
    msg(shrunk ? 'Đã gửi ✓ — cỡ chữ đã được giảm cho vừa chiều cao panel'
               : wasShow ? 'Đã gửi ✓ — đã dừng trình chiếu' : 'Đã gửi ✓');
    if (wasShow) loadProfiles();
  } catch(e){ msg('Lỗi gửi — kiểm tra đã vào đúng WiFi của bảng LED chưa', false); }
}

async function sendBrightness(){        // chỉ đổi độ sáng: không dừng trình chiếu, chữ không chạy lại
  try { await api('/api/set', new URLSearchParams({ brightness: S.brightness })); msg('Đã đổi độ sáng ✓'); }
  catch(e){ msg('Lỗi gửi độ sáng', false); }
}

async function init(){
  const st = await api('/api/state');
  $('ver').textContent = 'v' + st.version;
  FONTS = st.fonts; MAXB = st.maxBytes;

  for (const f of FONTS){                     // nút kiểu chữ
    const b = document.createElement('button');
    b.dataset.v = f.id; b.textContent = f.name;
    b.onclick = () => {
      const l = cur(); l.font = f.id;
      l.size = Math.min(Math.max(l.size, f.min), f.max);
      renderLine();
    };
    $('font').appendChild(b);
  }
  for (const c of SWATCHES){                  // ô màu
    const i = document.createElement('i'); i.style.background = c; i.dataset.c = c;
    i.onclick = () => { cur().color = c; renderLine(); };
    $('sw').insertBefore(i, $('color'));
  }
  apply(st);
}

// =====================================================================
//  SỰ KIỆN
// =====================================================================
for (const b of $('lines').children)  b.onclick = () => { S.lines = +b.dataset.v; if (S.lines === 2) E = 1; renderAll(); };
for (const b of $('tabs').children)   b.onclick = () => { E = +b.dataset.v; renderAll(); };
for (const b of $('align').children)  b.onclick = () => { cur().align = +b.dataset.v; renderLine(); };
for (const b of $('effect').children) b.onclick = () => { cur().effect = +b.dataset.v; renderLine(); };
$('sizeDown').onclick = () => { cur().size--; renderSize(); renderStatus(); };
$('sizeUp').onclick   = () => { cur().size++; renderSize(); renderStatus(); };
$('text').oninput = e => { cur().text = e.target.value; renderCount(); renderStatus(); };
$('text').onkeydown = e => { if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) send(); };
$('color').oninput = e => { cur().color = e.target.value; renderLine(); };
$('speed').oninput = e => { cur().speed = +e.target.value; $('sV').textContent = cur().speed + ' ms/px'; };
$('brightness').oninput = e => { S.brightness = +e.target.value; $('bV').textContent = Math.round(S.brightness / 2.55) + '%'; };
$('brightness').onchange = sendBrightness;    // độ sáng: gửi ngay khi thả thanh trượt
$('fx').oninput = e => { S.fx = +e.target.value; $('fV').textContent = S.fx; };
$('send').onclick = send;
$('reset').onclick = async () => {
  if (!confirm('Khôi phục cài đặt mặc định?')) return;
  try { apply(await api('/api/reset', new URLSearchParams())); msg('Đã khôi phục mặc định'); }
  catch(e){ msg('Lỗi gửi lệnh', false); }
};

// =====================================================================
//  GHÉP 2 MÀN HÌNH  (API: /api/link*, docs/pairing.md)
//  Mạch đang mở web này sẽ làm MASTER, mạch được chọn làm SLAVE.
// =====================================================================
let L = null, lkTimer = null, lkRole = null;

function renderLink(){
  const ssidOf = id => L.ssid.replace(/-[0-9A-F]{4}$/, '') + '-' + id;
  $('lkSelf').textContent = L.ssid;
  seg('lkSide', L.side);
  const paired = L.role === 'master';
  $('lkScan').classList.toggle('hide', paired);
  $('lkUnpair').classList.toggle('hide', !paired);
  $('lkScan').disabled = L.scanning || L.pairing;
  $('lkScan').textContent = L.scanning ? 'Đang tìm…' : 'Tìm màn hình khác';

  let info;
  if (paired) info = `<span class="dot ${L.online ? 'on' : ''}"></span>Đang ghép với <b>${ssidOf(L.peer)}</b> — ` +
                     (L.online ? 'đã kết nối, màn hình 128×32' : 'mất kết nối (kiểm tra nguồn mạch kia)');
  else if (L.pairing) info = 'Đang ghép…';
  else if (L.pairFailed) info = '<span style="color:var(--err)">Không ghép được — để 2 mạch gần nhau rồi thử lại</span>';
  else info = 'Đang chạy riêng. Bấm "Tìm" rồi chọn mạch để ghép thành 1 màn hình dài. ' +
              'Hủy ghép: giữ nút BOOT trên mạch bất kỳ ~2 giây.';
  $('lkInfo').innerHTML = info;

  const box = $('lkFound'); box.innerHTML = '';
  if (!paired) {
    for (const d of L.found){
      const row = document.createElement('div'); row.className = 'dev';
      row.innerHTML = `<b>${ssidOf(d.id)}</b><small>${d.rssi} dBm</small>`;
      const b = document.createElement('button'); b.textContent = 'Ghép';
      b.disabled = L.pairing;
      b.onclick = () => linkPost('/api/link/pair', { mac: d.mac });
      row.appendChild(b); box.appendChild(row);
    }
    if (!L.scanning && !L.found.length && L.lastScan)
      box.innerHTML = '<div class="status">Không thấy mạch nào. Mạch kia phải đang chạy riêng (chưa ghép) và ở gần.</div>';
    else if (L.found.length)
      box.insertAdjacentHTML('afterbegin', '<div class="status">Mã của từng mạch đang hiện trên panel của nó.</div>');
  }
}

async function linkSet(st){
  const scanned = L && (L.lastScan || L.scanning);
  L = st; L.lastScan = scanned;
  if (lkRole !== null && lkRole !== L.role){             // vừa ghép / hủy ghép -> độ rộng màn hình đổi
    try { apply(await api('/api/state')); } catch(e){}
    msg(L.role === 'master' ? 'Đã ghép 2 màn hình ✓' : 'Đã hủy ghép');
  }
  lkRole = L.role;
  renderLink();
  clearTimeout(lkTimer);
  lkTimer = setTimeout(linkPoll, L.scanning || L.pairing ? 700 : 3000);
}

async function linkPoll(){
  try { await linkSet(await api('/api/link')); }
  catch(e){ clearTimeout(lkTimer); lkTimer = setTimeout(linkPoll, 3000); }
}

async function linkPost(url, params){
  try {
    await linkSet(await api(url, new URLSearchParams(params || {})));
  } catch(e){ msg('Lỗi gửi lệnh ghép nối', false); }
}

$('lkScan').onclick = () => { if (L) L.lastScan = true; linkPost('/api/link/scan'); };
$('lkUnpair').onclick = () => { if (confirm('Hủy ghép? Cả 2 màn hình sẽ về chạy riêng.')) linkPost('/api/link/unpair'); };
for (const b of $('lkSide').children) b.onclick = () => linkPost('/api/link/side', { side: b.dataset.v });

// =====================================================================
//  PROFILE + TRÌNH CHIẾU  (API: /api/profiles, /api/profile/*, /api/show)
//  Thứ tự trình chiếu = thứ tự các profile được tích trong danh sách (▲▼ để đổi).
// =====================================================================
let P = null;            // dữ liệu /api/profiles
let ORDER = [];          // thứ tự hiển thị: các ô đang trình chiếu trước, rồi tới ô khác
const SECS = {};         // số giây đã chọn cho từng ô (cả ô chưa tích)
let pfTimer = null, showTimer = null;

const inShow = slot => P.show.items.some(i => i.slot === slot);
const esc = t => t.replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

let pfSig = '';           // danh sách đổi thì mới vẽ lại (đang gõ số giây không bị mất)

function setOrder(showSlots){
  ORDER = [...showSlots, ...P.items.map(i => i.slot).filter(s => !showSlots.includes(s))];
}

function setProfiles(p){
  P = p;
  for (const i of P.show.items) SECS[i.slot] = i.secs;
  setOrder(P.show.items.map(i => i.slot));
  const sig = JSON.stringify([P.items, P.show.items]);
  if (sig !== pfSig || !$('pfList').contains(document.activeElement)) renderProfiles();
  else renderShow();
  pfSig = sig;
  clearTimeout(pfTimer);
  if (P.show.on) pfTimer = setTimeout(() => loadProfiles().catch(() => {}), 2000);   // cập nhật slide đang chiếu
}

async function loadProfiles(){ setProfiles(await api('/api/profiles')); }

function renderProfiles(){
  $('pfCount').textContent = `${P.items.length}/${P.max}`;
  $('pfSave').disabled = P.items.length >= P.max;

  const box = $('pfList'); box.innerHTML = '';
  if (!P.items.length) box.innerHTML = '<div class="empty">Chưa có profile nào.</div>';
  const nShow = P.show.items.length;
  ORDER.forEach((slot, pos) => {
    const it = P.items.find(i => i.slot === slot);
    const on = inShow(slot);
    const row = document.createElement('div');
    row.className = 'pf';
    row.innerHTML =
      `<div class="top"><input type="checkbox" title="Đưa vào trình chiếu" ${on ? 'checked' : ''}>` +
      `<span class="nm">${esc(it.name)}</span>` +
      `<span class="secs"><input type="number" min="${P.show.min}" max="${P.show.maxSecs}" value="${SECS[slot] || 10}"> giây</span></div>` +
      `<div class="btns"><button class="go">Hiện</button><button class="ow">Lưu đè</button>` +
      `<button class="up" ${on && pos > 0 ? '' : 'disabled'} title="Chiếu sớm hơn">▲</button>` +
      `<button class="dn" ${on && pos < nShow - 1 ? '' : 'disabled'} title="Chiếu muộn hơn">▼</button>` +
      `<button class="del">Xóa</button></div>`;
    row.dataset.slot = slot;
    const [cb, secs] = row.querySelectorAll('input');
    cb.onchange = () => {
      saveShow(cb.checked ? [...P.show.items.map(i => i.slot), slot] : P.show.items.map(i => i.slot).filter(s => s !== slot));
    };
    secs.onchange = () => {
      SECS[slot] = Math.min(Math.max(+secs.value || 10, P.show.min), P.show.maxSecs);
      secs.value = SECS[slot];
      if (on) saveShow(P.show.items.map(i => i.slot));
    };
    row.querySelector('.go').onclick  = () => loadProfile(slot, it.name);
    row.querySelector('.ow').onclick  = () => saveProfile(slot, it.name);
    row.querySelector('.del').onclick = () => deleteProfile(slot, it.name);
    row.querySelector('.up').onclick  = () => moveShow(slot, -1);
    row.querySelector('.dn').onclick  = () => moveShow(slot, +1);
    box.appendChild(row);
  });
  renderShow();
}

// Phần thay đổi theo thời gian: slide đang chiếu, thời gian còn lại
function renderShow(){
  const nShow = P.show.items.length;
  $('showNote').classList.toggle('hide', !P.show.on);
  for (const row of $('pfList').querySelectorAll('.pf'))
    row.classList.toggle('cur', P.show.on && P.show.cur === +row.dataset.slot);
  const cur = P.items.find(i => i.slot === P.show.cur);
  $('shState').textContent = P.show.on ? 'đang chạy' : 'tắt';
  $('shState').style.color = P.show.on ? 'var(--acc)' : '';
  $('shToggle').textContent = P.show.on ? 'Dừng trình chiếu' : 'Bắt đầu trình chiếu';
  $('shToggle').classList.toggle('stop', P.show.on);
  $('shToggle').disabled = !P.show.on && !nShow;
  $('shInfo').innerHTML = !nShow
    ? 'Tích ☑ các profile muốn chiếu ở danh sách trên, chọn số giây cho mỗi cái, rồi bấm Bắt đầu.'
    : `${nShow} profile, mỗi vòng ${P.show.items.reduce((a, i) => a + i.secs, 0)} giây.` +
      (P.show.on && cur ? ` Đang chiếu <b>${esc(cur.name)}</b>, còn ${Math.ceil(P.show.left / 1000)} giây.` : '') +
      (P.show.on ? '' : ' Khi dừng, bảng LED hiện lại nội dung đã gửi.');
}

// Gửi danh sách chiếu (gộp các lần bấm liên tiếp)
function saveShow(slots){
  P.show.items = slots.map(s => ({ slot: s, secs: SECS[s] || 10 }));
  setOrder(slots);
  renderProfiles();
  clearTimeout(showTimer);
  showTimer = setTimeout(async () => {
    const items = P.show.items.map(i => `${i.slot}:${i.secs}`).join(',');
    try { setProfiles(await api('/api/show', new URLSearchParams({ items }))); }
    catch(e){ msg('Lỗi lưu danh sách trình chiếu', false); }
  }, 400);
}

function moveShow(slot, d){
  const s = P.show.items.map(i => i.slot), i = s.indexOf(slot), j = i + d;
  if (i < 0 || j < 0 || j >= s.length) return;
  [s[i], s[j]] = [s[j], s[i]];
  saveShow(s);
}

async function saveProfile(slot, name){
  if (slot !== undefined && !confirm(`Lưu đè profile "${name}" bằng nội dung đang chỉnh?`)) return;
  const p = contentParams();
  if (slot !== undefined) p.set('slot', slot);
  else p.set('name', norm($('pfName').value).trim());
  try {
    setProfiles(await api('/api/profile/save', p));
    if (slot === undefined) $('pfName').value = '';
    msg(slot === undefined ? 'Đã lưu profile ✓' : `Đã lưu đè "${name}" ✓`);
  } catch(e){ msg('Không lưu được profile' + (P && P.items.length >= P.max ? ' — đã đầy, xóa bớt' : ''), false); }
}

async function loadProfile(slot, name){
  try {
    apply(await api('/api/profile/load', new URLSearchParams({ slot })));
    msg(`Đang hiện "${name}" ✓`);
    await loadProfiles();
  } catch(e){ msg('Lỗi tải profile', false); }
}

async function deleteProfile(slot, name){
  if (!confirm(`Xóa profile "${name}"?`)) return;
  try { setProfiles(await api('/api/profile/delete', new URLSearchParams({ slot }))); msg('Đã xóa profile'); }
  catch(e){ msg('Lỗi xóa profile', false); }
}

$('pfSave').onclick = () => saveProfile();
$('pfName').onkeydown = e => { if (e.key === 'Enter') saveProfile(); };
$('shToggle').onclick = async () => {
  try {
    setProfiles(await api('/api/show', new URLSearchParams({ on: P.show.on ? 0 : 1 })));
    msg(P.show.on ? 'Đã bắt đầu trình chiếu ✓' : 'Đã dừng trình chiếu');
  } catch(e){ msg('Lỗi gửi lệnh trình chiếu', false); }
};

init().then(() => loadProfiles().catch(() => {})).then(linkPoll).catch(() => msg('Không tải được dữ liệu từ bảng LED — kiểm tra WiFi rồi tải lại trang', false));
