#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "driver/gpio.h"
#include "esp_sleep.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SDA_PIN 8
#define SCL_PIN 9
#define BTN_PIN 3
#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
Preferences prefs;

const char* AP_SSID = "KeyChain-Eye";
const char* AP_PASS = "12345678";

WebServer server(80);

bool imageMode = false;

bool screenOff = false;
bool standbyPending = false;
unsigned long standbyAt = 0;
uint8_t imgBuf[1024];

uint8_t imgAnim = 4;
bool animActive = false;
uint8_t animType = 0;
uint8_t autoIdx = 0;
unsigned long animStart = 0;
unsigned long nextImgAnim = 0;
const unsigned long ANIM_GAP = 5000;

const int EYE_W = 36;
const int EYE_H = 40;
const int EYE_R = 10;
const int EYE_LX = 20;
const int EYE_RX = 72;
const int EYE_CY = 30;

float lookX = 0, lookY = 0;
float tgtX = 0, tgtY = 0;
unsigned long nextLook = 0;
unsigned long nextBlink = 0;
unsigned long blinkStart = 0;
bool blinking = false;
unsigned long lastFrame = 0;

enum Mood { MOOD_NORMAL = 0, MOOD_ANGRY, MOOD_FUNNY, MOOD_LOVE, MOOD_SLEEPY, MOOD_SAD };
Mood mood = MOOD_NORMAL;
Mood lastMood = MOOD_NORMAL;
unsigned long nextMoodAt = 0;
unsigned long moodEnd = 0;
int funnyStyle = 0;

uint8_t faceSel = 0;
const Mood SEL_MOOD[8] = { MOOD_NORMAL, MOOD_NORMAL, MOOD_ANGRY, MOOD_FUNNY,
                           MOOD_FUNNY, MOOD_LOVE, MOOD_SLEEPY, MOOD_SAD };

bool btnLast = HIGH;
unsigned long btnTime = 0;

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KeyChain OLED</title>
<style>
  body{font-family:system-ui,sans-serif;background:#10141a;color:#e8eef5;margin:0;padding:16px;max-width:480px;margin:auto}
  h2{margin:0 0 4px}
  h3{margin:18px 0 6px;font-size:16px}
  p.sub{margin:0 0 10px;color:#8fa1b3;font-size:13px}
  .btns{display:flex;gap:8px;margin-bottom:12px}
  label.btn,button{flex:1;text-align:center;background:#2563eb;color:#fff;border:0;border-radius:10px;padding:12px 8px;font-size:15px;cursor:pointer}
  label.btn.alt{background:#7c3aed}
  button.go{background:#16a34a;width:100%;margin-top:6px}
  button.eyes{background:#475569;width:100%;margin-top:14px}
  input[type=file]{display:none}
  canvas{width:100%;image-rendering:pixelated;background:#000;border:2px solid #334155;border-radius:6px;display:block;aspect-ratio:2/1}
  .row{display:flex;align-items:center;gap:10px;margin:10px 0;font-size:14px}
  .row span{width:90px;color:#8fa1b3}
  .row input[type=range]{flex:1}
  select{background:#1e293b;color:#fff;border:1px solid #334155;border-radius:6px;padding:6px}
  hr{border:0;border-top:1px solid #334155;margin:18px 0 4px}
  .chips{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}
  .chips button{background:#1e293b;border:1px solid #334155;padding:10px 6px;font-size:14px}
  .chips button.on{background:#2563eb;border-color:#93c5fd}
  .chips button.wide{grid-column:span 2}
  .chips button.warn{background:#7f1d1d;border-color:#ef4444}
  #st{margin-top:10px;font-size:14px;min-height:20px;color:#fbbf24}
</style>
</head>
<body>
<h2>KeyChain OLED</h2>
<p class="sub">Pick a photo &rarr; convert to 128x64 &rarr; send to OLED</p>

<div class="btns">
  <label class="btn">📷 Camera
    <input type="file" id="cam" accept="image/*" capture="environment">
  </label>
  <label class="btn alt">🖼 Gallery
    <input type="file" id="gal" accept="image/*">
  </label>
</div>

<canvas id="cv" width="128" height="64"></canvas>

<div class="row"><span>Contrast</span><input type="range" id="con" min="0.5" max="3" step="0.05" value="1.2"></div>
<div class="row"><span>Brightness</span><input type="range" id="bri" min="-100" max="100" step="1" value="0"></div>
<div class="row"><span>Fit</span>
  <select id="fit">
    <option value="cover">Cover (crop)</option>
    <option value="contain">Contain (full image)</option>
    <option value="stretch">Stretch</option>
  </select>
</div>
<div class="row"><label><input type="checkbox" id="dith" checked> Dithering (shading)</label>
  <label><input type="checkbox" id="inv"> Invert</label></div>

<button class="go" id="send">⬆ Send to OLED</button>

<hr>
<h3>Photo animation</h3>
<p class="sub">Plays every 5 seconds while a photo is on the OLED.</p>
<div class="chips" id="animBox"></div>

<h3>Robot face animation</h3>
<p class="sub">Used while the robot eyes are showing. Pick one, or Auto for random moods.</p>
<div class="chips" id="faceBox"></div>

<h3>Power</h3>
<p class="sub">Screen OFF keeps WiFi on, so you can turn it back on here. Standby is deep sleep for maximum battery saving: WiFi turns off and you wake it with the button on the keychain.</p>
<div class="chips">
  <button id="scrOff">🌙 Screen OFF</button>
  <button id="scrOn">☀ Screen ON</button>
  <button id="stby" class="wide warn">🔋 Standby (deep sleep)</button>
</div>

<button class="eyes" id="eyes">👀 Robot Eyes</button>
<div id="st"></div>

<script>
const W=128,H=64;
const cv=document.getElementById('cv');
const ctx=cv.getContext('2d',{willReadFrequently:true});
ctx.imageSmoothingEnabled=true;
ctx.imageSmoothingQuality='high';
const $=id=>document.getElementById(id);
const st=t=>$('st').textContent=t;
let img=null;
const bytes=new Uint8Array(1024);

function loadFile(f){
  if(!f) return;
  const i=new Image();
  i.onload=()=>{img=i;render();st('Image ready. Adjust settings, then send.');};
  i.src=URL.createObjectURL(f);
}
$('cam').onchange=e=>loadFile(e.target.files[0]);
$('gal').onchange=e=>loadFile(e.target.files[0]);
['con','bri','fit','dith','inv'].forEach(id=>$(id).oninput=render);

function render(){
  if(!img) return;
  ctx.fillStyle='#000';ctx.fillRect(0,0,W,H);
  const fit=$('fit').value;
  if(fit==='stretch'){
    ctx.drawImage(img,0,0,W,H);
  }else{
    const s=fit==='cover'?Math.max(W/img.width,H/img.height):Math.min(W/img.width,H/img.height);
    const dw=img.width*s, dh=img.height*s;
    ctx.drawImage(img,(W-dw)/2,(H-dh)/2,dw,dh);
  }
  const d=ctx.getImageData(0,0,W,H);
  const px=d.data;
  const c=+$('con').value, b=+$('bri').value;
  const g=new Float32Array(W*H);
  for(let i=0;i<W*H;i++){
    let v=0.299*px[i*4]+0.587*px[i*4+1]+0.114*px[i*4+2];
    v=(v-128)*c+128+b;
    g[i]=v<0?0:(v>255?255:v);
  }
  const bit=new Uint8Array(W*H);
  if($('dith').checked){
    for(let y=0;y<H;y++)for(let x=0;x<W;x++){
      const i=y*W+x, old=g[i], nw=old<128?0:255;
      bit[i]=nw?1:0;
      const err=old-nw;
      if(x+1<W) g[i+1]+=err*7/16;
      if(y+1<H){
        if(x>0) g[i+W-1]+=err*3/16;
        g[i+W]+=err*5/16;
        if(x+1<W) g[i+W+1]+=err/16;
      }
    }
  }else{
    for(let i=0;i<W*H;i++) bit[i]=g[i]>=128?1:0;
  }
  if($('inv').checked) for(let i=0;i<W*H;i++) bit[i]^=1;

  bytes.fill(0);
  for(let y=0;y<H;y++)for(let x=0;x<W;x++){
    const i=y*W+x, v=bit[i];
    if(v) bytes[y*16+(x>>3)]|=(0x80>>(x&7));
    const o=i*4, c8=v?255:0;
    px[o]=px[o+1]=px[o+2]=c8; px[o+3]=255;
  }
  ctx.putImageData(d,0,0);
}

$('send').onclick=async()=>{
  if(!img){st('Pick a photo first.');return;}
  const hex=Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('');
  st('Sending...');
  try{
    const r=await fetch('/upload',{method:'POST',headers:{'Content-Type':'text/plain'},body:hex});
    st(r.ok?'✅ Done! Photo is on the OLED and saved (stays after power off).':'❌ Error: '+await r.text());
  }catch(e){st('❌ Connection error. Is your phone connected to KeyChain-Eye WiFi?');}
};
$('eyes').onclick=async()=>{
  try{await fetch('/eyes');st('👀 Robot eyes are back.');}
  catch(e){st('❌ Connection error.');}
};

// ----- power buttons -----
async function pw(m,msg){
  try{await fetch('/power?m='+m);st(msg);}
  catch(e){st(m==='standby'?msg:'❌ Connection error.');}
}
$('scrOff').onclick=()=>pw('off','🌙 Screen is OFF. Tap Screen ON to turn it back on.');
$('scrOn').onclick=()=>pw('on','☀ Screen is ON.');
$('stby').onclick=()=>{
  if(confirm('Go to standby? WiFi will turn off. Press the button on the keychain to wake it up.'))
    pw('standby','🔋 Standby. Press the button on the keychain to wake it up.');
};

// ----- option chips (animation selectors) -----
function mark(box,v){[...box.children].forEach(b=>b.classList.toggle('on',+b.dataset.v===v));}
function chips(boxId,url,labels,msg){
  const box=$(boxId);
  labels.forEach((t,i)=>{
    const b=document.createElement('button');
    b.textContent=t; b.dataset.v=i;
    b.onclick=async()=>{
      try{await fetch(url+'?m='+i);mark(box,i);st(msg+': '+t.replace(/^\S+\s/,''));}
      catch(e){st('❌ Connection error.');}
    };
    box.appendChild(b);
  });
  fetch(url).then(r=>r.text()).then(v=>mark(box,+v)).catch(()=>{});
}
chips('animBox','/anim',['⏹ Static','😉 Blink','↔ Slide','🔍 Zoom','🔀 Auto (cycle)'],'Photo animation');
chips('faceBox','/face',['🔀 Auto (random)','😐 Normal','😠 Angry','😄 Funny - Happy','🤪 Funny - Wacky','😍 Love','😴 Sleepy','😢 Sad'],'Robot face');
</script>
</body>
</html>
)rawliteral";

uint8_t hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}

void showSavedImage() {
  display.clearDisplay();
  display.drawBitmap(0, 0, imgBuf, SCREEN_W, SCREEN_H, SSD1306_WHITE);
  display.display();
}

void goToEyes() {
  imageMode = false;
  animActive = false;
  prefs.putBool("img", false);
  unsigned long now = millis();
  mood = MOOD_NORMAL;
  nextBlink = now + 1500;
  nextLook = now + 500;
  nextMoodAt = now + random(5000, 9000);
}

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleUpload() {
  String hex = server.arg("plain");
  if (hex.length() != 2048) {
    server.send(400, "text/plain", "Wrong size (need 2048 hex chars)");
    return;
  }
  for (int i = 0; i < 1024; i++) {
    imgBuf[i] = (hexVal(hex[2 * i]) << 4) | hexVal(hex[2 * i + 1]);
  }
  imageMode = true;
  animActive = false;
  nextImgAnim = millis() + ANIM_GAP;
  showSavedImage();
  prefs.putBytes("bmp", imgBuf, 1024);
  prefs.putBool("img", true);
  server.send(200, "text/plain", "OK");
}

void handleEyes() {
  goToEyes();
  server.send(200, "text/plain", "OK");
}

void handleAnim() {
  if (server.hasArg("m")) {
    int m = server.arg("m").toInt();
    if (m < 0 || m > 4) m = 0;
    imgAnim = m;
    prefs.putUChar("anim", imgAnim);
    animActive = false;
    nextImgAnim = millis() + 600;
    if (imageMode) showSavedImage();
  }
  server.send(200, "text/plain", String(imgAnim));
}

void handleFace() {
  if (server.hasArg("m")) {
    int m = server.arg("m").toInt();
    if (m < 0 || m > 7) m = 0;
    faceSel = m;
    prefs.putUChar("face", faceSel);
    if (faceSel == 0) {
      mood = MOOD_NORMAL;
      nextMoodAt = millis() + 2000;
    }
  }
  server.send(200, "text/plain", String(faceSel));
}

void turnScreenOff() {
  screenOff = true;
  animActive = false;
  display.ssd1306_command(SSD1306_DISPLAYOFF);
}

void turnScreenOn() {
  display.ssd1306_command(SSD1306_DISPLAYON);
  screenOff = false;
  animActive = false;
  nextImgAnim = millis() + ANIM_GAP;
  if (imageMode) showSavedImage();
}

void goStandby() {

  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  prefs.end();

  while (digitalRead(BTN_PIN) == LOW) delay(10);
  gpio_pullup_en((gpio_num_t)BTN_PIN);
  gpio_pulldown_dis((gpio_num_t)BTN_PIN);
  gpio_hold_en((gpio_num_t)BTN_PIN);
  gpio_deep_sleep_hold_en();
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BTN_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

void handlePower() {
  String m = server.arg("m");
  if (m == "off") {
    turnScreenOff();
  } else if (m == "on") {
    turnScreenOn();
  } else if (m == "standby") {
    standbyPending = true;
    standbyAt = millis() + 800;
  }
  server.send(200, "text/plain", screenOff ? "off" : "on");
}

void drawScaledImage(float s) {
  int dw = (int)(SCREEN_W * s);
  int dh = (int)(SCREEN_H * s);
  if (dw < 1) dw = 1;
  if (dh < 1) dh = 1;
  int x0 = (SCREEN_W - dw) / 2;
  int y0 = (SCREEN_H - dh) / 2;
  for (int y = 0; y < dh; y++) {
    int sy = (y * SCREEN_H) / dh;
    for (int x = 0; x < dw; x++) {
      int sx = (x * SCREEN_W) / dw;
      if (imgBuf[sy * 16 + (sx >> 3)] & (0x80 >> (sx & 7))) {
        display.drawPixel(x0 + x, y0 + y, SSD1306_WHITE);
      }
    }
  }
}

void startImgAnim(unsigned long now) {
  if (imgAnim == 4) {
    animType = (autoIdx % 3) + 1;
    autoIdx++;
  } else {
    animType = imgAnim;
  }
  animActive = true;
  animStart = now;
}

void runImgAnim(unsigned long now) {
  unsigned long t = now - animStart;
  bool done = false;

  display.clearDisplay();

  if (animType == 1) {

    const unsigned long D = 360;
    if (t >= D) {
      done = true;
    } else {
      float c;
      if (t < 150)      c = t / 150.0f;
      else if (t < 210) c = 1.0f;
      else              c = 1.0f - (t - 210) / 150.0f;
      display.drawBitmap(0, 0, imgBuf, SCREEN_W, SCREEN_H, SSD1306_WHITE);
      int cover = (int)(c * 32);
      if (cover > 0) {
        display.fillRect(0, 0, SCREEN_W, cover, SSD1306_BLACK);
        display.fillRect(0, SCREEN_H - cover, SCREEN_W, cover, SSD1306_BLACK);
      }
    }
  } else if (animType == 2) {

    const unsigned long D = 900;
    if (t >= D) {
      done = true;
    } else {
      int x;
      if (t < 450) {
        float p = t / 450.0f;
        x = (int)(SCREEN_W * p * p);
      } else {
        float p = (t - 450) / 450.0f;
        x = -SCREEN_W + (int)(SCREEN_W * (1.0f - (1.0f - p) * (1.0f - p)));
      }
      display.drawBitmap(x, 0, imgBuf, SCREEN_W, SCREEN_H, SSD1306_WHITE);
    }
  } else {

    const unsigned long D = 700;
    if (t >= D) {
      done = true;
    } else {
      float p = t / (float)D;
      float s = 0.05f + 0.95f * (1.0f - (1.0f - p) * (1.0f - p));
      drawScaledImage(s);
    }
  }

  if (done) {
    showSavedImage();
    animActive = false;
    nextImgAnim = now + ANIM_GAP;
    return;
  }
  display.display();
}

void drawEye(int x, int top, int w, int h) {
  int r = EYE_R;
  if (r > h / 2) r = h / 2;
  if (r > w / 2) r = w / 2;
  display.fillRoundRect(x, top, w, h, r, SSD1306_WHITE);
}

void drawHeart(int cx, int cy, int s) {
  int r = s / 2;
  display.fillCircle(cx - r, cy - r / 2, r, SSD1306_WHITE);
  display.fillCircle(cx + r, cy - r / 2, r, SSD1306_WHITE);
  display.fillTriangle(cx - s, cy - r / 3, cx + s, cy - r / 3, cx, cy + s, SSD1306_WHITE);
}

void updateMood(unsigned long now) {
  if (faceSel != 0) {
    mood = SEL_MOOD[faceSel];
    funnyStyle = (faceSel == 4) ? 1 : 0;
    return;
  }

  if (mood == MOOD_NORMAL) {
    if (now >= nextMoodAt) {
      Mood m;
      do { m = (Mood)random(1, 6); } while (m == lastMood);
      mood = m;
      lastMood = m;
      funnyStyle = random(0, 2);
      moodEnd = now + random(2800, 5000);
    }
  } else if (now >= moodEnd) {
    mood = MOOD_NORMAL;
    nextMoodAt = now + random(6000, 14000);
  }
}

void drawEyes() {
  unsigned long now = millis();
  updateMood(now);

  if (now >= nextLook) {
    tgtX = random(-10, 11);
    tgtY = random(-6, 7);
    if (random(0, 4) == 0) { tgtX = 0; tgtY = 0; }
    nextLook = now + random(1200, 3500);
  }
  lookX += (tgtX - lookX) * 0.25f;
  lookY += (tgtY - lookY) * 0.25f;

  int h = EYE_H;
  if (!blinking && now >= nextBlink) {
    blinking = true;
    blinkStart = now;
  }
  if (blinking) {
    unsigned long t = now - blinkStart;
    if (t < 80)       h = EYE_H - (36 * t) / 80;
    else if (t < 160) h = 4 + (36 * (t - 80)) / 80;
    else {
      blinking = false;
      nextBlink = now + random(2000, 5500);
      if (random(0, 5) == 0) nextBlink = now + 250;
    }
  }
  if (h < 4) h = 4;

  display.clearDisplay();

  if (mood == MOOD_NORMAL) {
    int cy = EYE_CY + (int)lookY;
    int top = cy - h / 2;
    drawEye(EYE_LX + (int)lookX, top, EYE_W, h);
    drawEye(EYE_RX + (int)lookX, top, EYE_W, h);

  } else if (mood == MOOD_ANGRY) {
    int shake = random(-1, 2);
    int cy = EYE_CY + 3;
    int top = cy - h / 2;
    int lx = EYE_LX + (int)(lookX / 3) + shake;
    int rx = EYE_RX + (int)(lookX / 3) + shake;
    int slant = (22 * h) / EYE_H;
    drawEye(lx, top, EYE_W, h);
    drawEye(rx, top, EYE_W, h);

    display.fillTriangle(lx - 1, top - 1, lx + EYE_W + 1, top - 1, lx + EYE_W + 1, top + slant, SSD1306_BLACK);
    display.fillTriangle(rx - 1, top - 1, rx + EYE_W + 1, top - 1, rx - 1, top + slant, SSD1306_BLACK);

  } else if (mood == MOOD_FUNNY) {
    if (funnyStyle == 0) {

      int cy = EYE_CY + (int)(3 * sin(now / 120.0));
      int lcx = EYE_LX + EYE_W / 2;
      int rcx = EYE_RX + EYE_W / 2;
      drawEye(EYE_LX, cy - 20, EYE_W, 40);
      drawEye(EYE_RX, cy - 20, EYE_W, 40);
      display.fillCircle(lcx, cy + 14, 15, SSD1306_BLACK);
      display.fillCircle(rcx, cy + 14, 15, SSD1306_BLACK);
    } else {

      bool swap = ((now / 450) % 2) == 0;
      int wig = (int)(2 * sin(now / 60.0));
      int lw = swap ? 36 : 26, lh = swap ? 40 : 22;
      int rw = swap ? 26 : 36, rh = swap ? 22 : 40;
      drawEye(38 - lw / 2, EYE_CY - lh / 2 + wig, lw, lh);
      drawEye(90 - rw / 2, EYE_CY - rh / 2 - wig, rw, rh);
    }

  } else if (mood == MOOD_LOVE) {

    int s = (int)(15 + 3 * sin(now / 120.0));
    drawHeart(EYE_LX + EYE_W / 2, EYE_CY - 3, s);
    drawHeart(EYE_RX + EYE_W / 2, EYE_CY - 3, s);

  } else if (mood == MOOD_SLEEPY) {

    int eh = 30;
    int cy = EYE_CY + 4;
    int top = cy - eh / 2;
    int droop = 10 + (int)(10 * (0.5 + 0.5 * sin(now / 500.0)));
    drawEye(EYE_LX, top, EYE_W, eh);
    drawEye(EYE_RX, top, EYE_W, eh);
    display.fillRect(EYE_LX - 1, top - 1, EYE_W + 2, droop + 1, SSD1306_BLACK);
    display.fillRect(EYE_RX - 1, top - 1, EYE_W + 2, droop + 1, SSD1306_BLACK);
    int zp = (now / 60) % 40;
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(100, 14 - zp / 4);
    display.print("z");
    display.setTextSize(2);
    display.setCursor(110, 8 - zp / 5);
    display.print("Z");

  } else {

    int cy = EYE_CY - 2;
    int top = cy - h / 2;
    int lx = EYE_LX + (int)(lookX / 4);
    int rx = EYE_RX + (int)(lookX / 4);
    int slant = (18 * h) / EYE_H;
    drawEye(lx, top, EYE_W, h);
    drawEye(rx, top, EYE_W, h);
    display.fillTriangle(lx - 1, top - 1, lx + EYE_W + 1, top - 1, lx - 1, top + slant, SSD1306_BLACK);
    display.fillTriangle(rx - 1, top - 1, rx + EYE_W + 1, top - 1, rx + EYE_W + 1, top + slant, SSD1306_BLACK);
    int ty = (cy - EYE_H / 2) + EYE_H - 4 + (int)((now / 60) % 40);
    if (ty < 62) {
      int tx = lx + 8;
      display.fillCircle(tx, ty, 2, SSD1306_WHITE);
      display.fillTriangle(tx - 2, ty, tx + 2, ty, tx, ty - 5, SSD1306_WHITE);
    }
  }

  display.display();
}

void checkButton() {
  bool b = digitalRead(BTN_PIN);
  if (b != btnLast && millis() - btnTime > 40) {
    btnTime = millis();
    btnLast = b;
    if (b == LOW) {
      if (screenOff) turnScreenOn();
      else goToEyes();
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);
  btnLast = digitalRead(BTN_PIN);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found! Check SDA=8, SCL=9 and address 0x3C.");
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.display();

  randomSeed(micros());

  prefs.begin("keychain", false);
  imgAnim = prefs.getUChar("anim", 4);
  if (imgAnim > 4) imgAnim = 4;
  faceSel = prefs.getUChar("face", 0);
  if (faceSel > 7) faceSel = 0;

  unsigned long now = millis();
  nextBlink = now + 2000;
  nextLook = now + 1000;
  nextMoodAt = now + random(5000, 9000);
  nextImgAnim = now + ANIM_GAP;

  if (prefs.getBool("img", false) && prefs.getBytesLength("bmp") == 1024) {
    prefs.getBytes("bmp", imgBuf, 1024);
    imageMode = true;
    showSavedImage();
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_POST, handleUpload);
  server.on("/eyes", HTTP_GET, handleEyes);
  server.on("/anim", HTTP_GET, handleAnim);
  server.on("/face", HTTP_GET, handleFace);
  server.on("/power", HTTP_GET, handlePower);
  server.begin();
}

void loop() {
  server.handleClient();
  checkButton();

  unsigned long now = millis();

  if (standbyPending && now >= standbyAt) goStandby();

  if (screenOff) {
    delay(5);
    return;
  }

  if (imageMode) {
    if (animActive) {
      if (now - lastFrame >= 25) {
        lastFrame = now;
        runImgAnim(now);
      }
    } else if (imgAnim != 0 && now >= nextImgAnim) {
      startImgAnim(now);
    }
  } else if (now - lastFrame >= 30) {
    lastFrame = now;
    drawEyes();
  }
}
