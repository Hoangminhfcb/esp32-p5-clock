// Kiểm tra luồng chọn Wi-Fi với HTTP/DOM giả lập, không cần thư viện ngoài.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const html=fs.readFileSync('main/net/www/clock.html','utf8');
const source=html.match(/<script>([\s\S]*?)<\/script>/)[1];
class Element {
 constructor(){this.value='';this.disabled=false;this.options=[];this.events={};this.textContent=''}
 addEventListener(name,fn){this.events[name]=fn}
 add(option){this.options.push(option)}
 replaceChildren(...options){this.options=options}
 focus(){}
}
const ids={};
const get=id=>ids[id]??=new Element();
let responses=[
 {scanning:true,networks:[]},
 {scanning:false,networks:[{ssid:'Nhà <Wi-Fi>',rssi:-45,secured:true},{ssid:'Khach',rssi:-80,secured:false}]}
];
const calls=[];
const context=vm.createContext({
 document:{getElementById:get},
 Option:class {constructor(text,value){this.text=text;this.value=value}},
 setTimeout:fn=>queueMicrotask(fn),
 fetch:async(url,options)=>{
  calls.push({url,options});
  const body=url==='/palette'&&options?.method!=='POST'
   ?{time:['#112233','#445566','#778899','#aabbcc'],date:['#f01122','#f03344','#f05566','#f07788']}
   :responses.shift();assert.ok(body,'Unexpected fetch');
  return {ok:!body.error,json:async()=>body,text:async()=>body.message||body.error};
 }
});
const settle=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
 vm.runInContext(source,context);
 await settle();
 assert.equal(calls[0].url,'/scan?refresh=1');assert.equal(calls.filter(call=>call.url.startsWith('/scan'))[1].url,'/scan');
 assert.equal(get('networks').options.length,3);
 assert.match(get('networks').options[1].text,/Nhà <Wi-Fi>/);
 assert.match(get('networks').options[2].text,/Không mật khẩu/);
 assert.equal(get('scan').disabled,false);
 get('networks').value='Nhà <Wi-Fi>';get('networks').events.change();
 get('password').value='test-only-password';
 responses=[{message:'Đang kết nối'}];
 await get('form').events.submit({preventDefault(){}});
 const post=calls.at(-1);
 assert.equal(post.url,'/wifi');assert.equal(post.options.method,'POST');
 assert.deepEqual(JSON.parse(post.options.body),{ssid:'Nhà <Wi-Fi>',password:'test-only-password'});
 get('ssid').value='Mang an';get('ssid').events.input();
 responses=[{message:'Đang kết nối mạng ẩn'}];
 await get('form').events.submit({preventDefault(){}});
 assert.equal(JSON.parse(calls.at(-1).options.body).ssid,'Mang an');
 responses=[{error:'Đang kết nối, thử lại sau'}];
 await context.scanWifi();
 assert.match(get('status').textContent,/thử lại sau/);assert.equal(get('scan').disabled,false);
 responses=[{scanning:false,networks:[]}];
 await context.scanWifi();
 assert.match(get('status').textContent,/Không thấy mạng/);assert.equal(get('networks').disabled,true);
 assert.equal(get('h0').value,'#112233');assert.equal(get('d3').value,'#f07788');
 get('h0').value='#123456';responses=[{message:'Đã lưu bảng màu'}];
 await get('palette-form').events.submit({preventDefault(){}});
 assert.equal(calls.at(-1).url,'/palette');
 assert.equal(JSON.parse(calls.at(-1).options.body).time[0],'#123456');
 assert.match(get('palette-status').textContent,/Đã lưu/);
 assert.equal(get('save-palette').disabled,false);
 console.log('Portal tests passed: automatic scan, selection, safe SSID labels, submission, hidden network, busy/error, empty results and custom palette load/save');
})().catch(error=>{console.error(error);process.exitCode=1});
