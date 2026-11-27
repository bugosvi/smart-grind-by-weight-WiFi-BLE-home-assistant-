#pragma once

#include <Arduino.h>

inline constexpr char SMART_GRIND_DEVICE_PAGE[] PROGMEM = R"WEB(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0e110e">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="description" content="Smart Grind Studio — Live espresso scale telemetry, scheduled AMOLED Morning Notes, adaptive grinding & cup safety controls, and grind history.">
<link rel="manifest" href="/manifest.webmanifest">
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='9' fill='%23232f21'/%3E%3Cpath d='M22.9 7.1c4.4 4.4 2.9 12.7-2.7 17.1S7.6 27.1 4.8 22.3 4.3 10.1 10 6.2s9.4-2.5 12.9.9Z' fill='none' stroke='%23d4a96a' stroke-width='2.1'/%3E%3Cpath d='M8.4 23.5c4.7-2.7 3.5-6.2 2.5-9.3-.9-2.8-1.6-5.2 1.7-8.4' fill='none' stroke='%23d4a96a' stroke-width='2.1' stroke-linecap='round'/%3E%3C/svg%3E">
<title>Smart Grind Studio</title>
<style>
:root{
  color-scheme:dark;
  --bg:#0e110e;--surface:#151a15;--card:#1b221b;--raised:#242d24;
  --text:#f4f5ef;--muted:#9ea99a;--line:#2f3a2f;
  --accent:#d4a96a;--accent-ink:#1b1307;--sage:#84b77c;--danger:#df6b63;--warning:#e4b363;
  --shadow:0 16px 44px rgba(0,0,0,.38);--radius:18px;
}
:root[data-theme=light]{
  color-scheme:light;
  --bg:#f4f2eb;--surface:#fbfaf5;--card:#ffffff;--raised:#ecebe2;
  --text:#192019;--muted:#626c5f;--line:#dad7ca;
  --accent:#9c6a28;--accent-ink:#ffffff;--sage:#467841;--danger:#b8453f;--warning:#9a681c;
  --shadow:0 14px 36px rgba(28,36,24,.09);
}
*{box-sizing:border-box}
html{scroll-behavior:smooth}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",Inter,Roboto,"Helvetica Neue",sans-serif;-webkit-font-smoothing:antialiased;transition:background .2s,color .2s}
button,input,select,textarea{font:inherit}
button{cursor:pointer}
.num{font-variant-numeric:tabular-nums}
.shell{width:min(1140px,100%);margin:0 auto;padding:0 22px 64px}
.appbar{position:sticky;top:0;z-index:30;background:color-mix(in srgb,var(--bg) 86%,transparent);backdrop-filter:blur(18px);-webkit-backdrop-filter:blur(18px);border-bottom:1px solid color-mix(in srgb,var(--line) 75%,transparent)}
.barinner{width:min(1140px,100%);min-height:68px;margin:0 auto;padding:10px 22px;display:flex;align-items:center;gap:18px}
.brand{display:flex;align-items:center;gap:11px;font-weight:800;letter-spacing:-.02em;white-space:nowrap}
.mark{width:34px;height:34px;border-radius:11px;background:linear-gradient(155deg,#2c3929,#182017);border:1px solid color-mix(in srgb,var(--accent) 45%,var(--line));display:grid;place-items:center}
:root[data-theme=light] .mark{background:linear-gradient(155deg,#2b3728,#1a2218)}
.mark svg{width:21px;height:21px;fill:none;stroke:#d4a96a;stroke-width:2.1;stroke-linecap:round;stroke-linejoin:round}
.brand small{display:block;font-size:11px;font-weight:600;color:var(--muted);letter-spacing:.04em;text-transform:uppercase;line-height:1.1}
.nav{display:flex;gap:4px;margin-left:auto}
.nav button,.iconbtn{border:0;background:transparent;color:var(--muted);border-radius:11px;padding:9px 14px;font-weight:650;transition:background .15s,color .15s}
.nav button:hover,.iconbtn:hover{background:var(--raised);color:var(--text)}
.nav button.active{background:var(--raised);color:var(--accent)}
.iconbtn{width:40px;height:40px;padding:0;font-size:17px;display:grid;place-items:center}
.page{display:none;animation:fade .18s ease}
.page.active{display:block}
@keyframes fade{from{opacity:.45;transform:translateY(4px)}}
.hero{padding:32px 0 20px;display:flex;justify-content:space-between;align-items:flex-end;gap:16px;flex-wrap:wrap}
.eyebrow{color:var(--accent);font-size:11.5px;font-weight:800;letter-spacing:.12em;text-transform:uppercase}
.hero h1{font-size:clamp(28px,4.5vw,42px);line-height:1.05;letter-spacing:-.035em;margin:6px 0 4px}
.sub{color:var(--muted);margin:0;font-size:14px}
.pills{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.pill{display:inline-flex;align-items:center;gap:8px;padding:7px 12px;border:1px solid var(--line);border-radius:999px;background:var(--surface);color:var(--muted);font-size:13px;font-weight:650}
.pill .dot{width:8px;height:8px;border-radius:50%;background:var(--warning);flex:none}
.pill.live .dot{background:var(--sage)}
.pill.ok{color:var(--sage);border-color:color-mix(in srgb,var(--sage) 40%,var(--line))}
.pill.ok .dot{background:var(--sage)}
.pill.warn{color:var(--warning);border-color:color-mix(in srgb,var(--warning) 40%,var(--line))}
.pill.err{color:var(--danger);border-color:color-mix(in srgb,var(--danger) 40%,var(--line))}
.pill.err .dot{background:var(--danger)}
.pill.action{cursor:pointer;color:var(--accent);border-color:color-mix(in srgb,var(--accent) 42%,var(--line))}
.pill.action:hover{background:var(--raised)}
.grid{display:grid;gap:16px}
.metrics{grid-template-columns:repeat(3,1fr)}
.metric,.card{background:var(--card);border:1px solid var(--line);border-radius:var(--radius);box-shadow:var(--shadow)}
.metric{padding:19px 20px}
.metric label{display:block;color:var(--muted);font-size:11.5px;font-weight:750;letter-spacing:.07em;text-transform:uppercase;margin-bottom:4px}
.metric strong{display:block;font-size:clamp(26px,3.6vw,38px);line-height:1.08;letter-spacing:-.035em}
.metric small{color:var(--muted);font-size:13px}
.card{padding:22px}
.cardhead{display:flex;align-items:flex-start;justify-content:space-between;gap:14px;margin-bottom:16px;flex-wrap:wrap}
.card h2,.card h3{margin:0;letter-spacing:-.025em}
.card h2{font-size:19px}
.card h3{font-size:15.5px}
.bar-row{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;margin-bottom:14px}
.seg{display:inline-flex;background:var(--surface);border:1px solid var(--line);border-radius:12px;padding:3px;gap:3px}
.seg button{border:0;background:transparent;color:var(--muted);padding:7px 13px;border-radius:9px;font-size:13px;font-weight:700;transition:background .15s,color .15s}
.seg button.active{background:var(--raised);color:var(--accent)}
.seg button:disabled{opacity:.45;cursor:not-allowed}
.dose-picker{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin:0 0 16px}
.dose{display:flex;align-items:center;justify-content:space-between;gap:12px;min-height:64px;padding:12px 16px;border:1px solid var(--line);border-radius:14px;background:var(--card);color:var(--text);text-align:left;box-shadow:var(--shadow);transition:border-color .15s,background .15s}
.dose:hover:not(:disabled){border-color:var(--accent)}
.dose.active{border-color:var(--accent);background:color-mix(in srgb,var(--accent) 12%,var(--card))}
.dose span{font-weight:750}
.dose strong{color:var(--accent);font-size:18px}
.dose:disabled{opacity:.5;cursor:not-allowed}
.chartwrap{height:270px;position:relative}
.chartwrap canvas{width:100%;height:100%;display:block}
.progress{height:7px;background:var(--raised);border-radius:999px;overflow:hidden;margin:16px 0 8px}
.progress i{display:block;width:0;height:100%;background:linear-gradient(90deg,var(--sage),var(--accent));border-radius:inherit;transition:width .14s}
.actions{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-top:16px}
.btn{border:0;border-radius:12px;padding:10px 16px;background:var(--accent);color:var(--accent-ink);font-weight:750;transition:transform .1s,opacity .15s,background .15s;display:inline-flex;align-items:center;justify-content:center;gap:8px;text-decoration:none}
.btn:hover:not(:disabled){opacity:.92}
.btn:active:not(:disabled){transform:scale(.97)}
.btn.secondary{background:var(--raised);color:var(--text);border:1px solid var(--line)}
.btn.danger{background:var(--danger);color:#fff}
.btn.small{padding:7px 12px;font-size:13px;border-radius:10px}
.btn:disabled{opacity:.42;cursor:not-allowed}
.grind-control{width:60px;height:60px;padding:0;border-radius:50%;font-size:24px;line-height:1}
.grind-control.stop{background:var(--danger);color:#fff}
.message{min-height:20px;color:var(--muted);font-size:13.5px;margin-top:10px}
.divider{border-top:1px solid var(--line);margin:18px 0}
.note-banner{display:none;align-items:center;justify-content:space-between;gap:12px;padding:12px 16px;margin-bottom:16px;border-radius:14px;background:color-mix(in srgb,var(--accent) 10%,var(--surface));border:1px solid color-mix(in srgb,var(--accent) 35%,var(--line))}
.note-banner.show{display:flex}

/* Morning Note Studio */
.studio-layout{grid-template-columns:minmax(260px,330px) minmax(0,1fr);align-items:start}
.bezel-wrap{display:flex;flex-direction:column;align-items:center;gap:14px}
.bezel{padding:14px;border-radius:30px;background:#080908;border:1px solid #2b332b;box-shadow:0 22px 50px rgba(0,0,0,.55);width:min(100%,308px)}
.bezel canvas{display:block;width:100%;aspect-ratio:280/456;border-radius:18px;background:#000}
.bezel-meta{width:100%;display:flex;justify-content:space-between;align-items:center;font-size:12px;color:var(--muted);padding:0 4px}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:6px}
.chip{border:1px solid var(--line);background:var(--surface);color:var(--text);border-radius:999px;padding:5px 11px;font-size:13px;font-weight:600}
.chip:hover{border-color:var(--accent);background:var(--raised)}
.theme-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.tcard{border:1.5px solid var(--line);background:var(--surface);color:var(--text);border-radius:12px;padding:9px 8px;text-align:center;font-size:12px;font-weight:700}
.tcard.selected{border-color:var(--accent);background:color-mix(in srgb,var(--accent) 12%,var(--surface))}
.swatch{display:block;height:22px;border-radius:7px;margin-bottom:6px;border:1px solid rgba(255,255,255,.1)}
.status-box{padding:12px 14px;border-radius:13px;background:var(--surface);border:1px solid var(--line);display:grid;gap:4px;font-size:13px}

/* History */
.history-layout{grid-template-columns:minmax(270px,.72fr) minmax(0,1.5fr);align-items:start}
.history-list{display:grid;gap:8px;max-height:640px;overflow:auto;padding-right:3px}
.shot{width:100%;text-align:left;border:1px solid var(--line);background:var(--surface);color:var(--text);padding:12px 14px;border-radius:13px;display:grid;grid-template-columns:1fr auto;gap:3px 12px}
.shot:hover,.shot.active{border-color:var(--accent);background:var(--raised)}
.shot strong{font-size:14.5px}
.shot span,.shot small{color:var(--muted);font-size:12.5px}
.shot .result{color:var(--sage);font-weight:700}
.empty{padding:32px 10px;text-align:center;color:var(--muted)}
.summary{grid-template-columns:repeat(3,1fr);margin-bottom:16px}
.summary .metric{padding:15px 18px}
.summary .metric strong{font-size:24px}
.detailmeta{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin:14px 0}
.detailmeta div{background:var(--surface);padding:11px;border-radius:11px;border:1px solid var(--line)}
.detailmeta span{display:block;color:var(--muted);font-size:11px;text-transform:uppercase;letter-spacing:.05em}
.detailmeta strong{font-size:16px}

/* Settings & Precision Control */
.settings-layout{grid-template-columns:210px minmax(0,1fr);align-items:start}
.settingsnav{position:sticky;top:88px;padding:10px}
.settingsnav button{display:block;width:100%;text-align:left;border:0;background:transparent;color:var(--muted);padding:10px 12px;border-radius:10px;font-weight:650;font-size:14px}
.settingsnav button:hover,.settingsnav button.active{background:var(--raised);color:var(--accent)}
.settingssection{scroll-margin-top:88px;margin-bottom:16px}
.formgrid{display:grid;grid-template-columns:repeat(2,1fr);gap:14px}
.field{display:grid;gap:6px}
.field>span,.group-title{color:var(--muted);font-size:12px;font-weight:750;letter-spacing:.03em}
.input,select,textarea{width:100%;border:1px solid var(--line);background:var(--surface);color:var(--text);border-radius:11px;padding:10px 12px;outline:none;transition:border-color .15s}
textarea{resize:vertical;min-height:92px;line-height:1.45}
.input:focus,select:focus,textarea:focus{border-color:var(--accent)}
.profiles{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.profile{background:var(--surface);border:1px solid var(--line);border-radius:14px;padding:14px}
.profile h3{font-size:14px;margin:0 0 10px}
.checks{display:grid;grid-template-columns:repeat(2,1fr);gap:9px}
.check{display:flex;align-items:center;gap:10px;padding:11px 12px;background:var(--surface);border:1px solid var(--line);border-radius:12px;font-size:14px}
.check input{accent-color:var(--accent);width:18px;height:18px;flex:none}
.range{display:grid;grid-template-columns:1fr 56px;gap:10px;align-items:center}
.range input[type=range]{accent-color:var(--accent);width:100%}
.engine-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px;margin-bottom:14px}
.ecard{text-align:left;padding:14px 15px;border-radius:14px;border:1.5px solid var(--line);background:var(--surface);color:var(--text);display:grid;gap:4px}
.ecard.selected{border-color:var(--accent);background:color-mix(in srgb,var(--accent) 11%,var(--surface))}
.ecard strong{font-size:14.5px}
.ecard span{font-size:12.5px;color:var(--muted)}
.meter-box{padding:14px 16px;border-radius:14px;background:var(--surface);border:1px solid var(--line);margin-bottom:14px}
.meter-head{display:flex;justify-content:space-between;align-items:baseline;gap:10px;flex-wrap:wrap;margin-bottom:8px}
.meter-track{position:relative;height:10px;border-radius:999px;background:var(--raised);overflow:hidden}
.meter-zone{position:absolute;top:0;bottom:0;background:color-mix(in srgb,var(--sage) 35%,transparent)}
.meter-fill{position:relative;height:100%;width:0;background:var(--accent);border-radius:999px;transition:width .15s}
.stats-strip{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin:12px 0}
.stat-mini{background:var(--surface);border:1px solid var(--line);border-radius:12px;padding:11px 12px}
.stat-mini span{display:block;font-size:11px;color:var(--muted);text-transform:uppercase;letter-spacing:.05em}
.stat-mini strong{font-size:17px}
.gallery{display:grid;grid-template-columns:repeat(auto-fit,minmax(112px,1fr));gap:10px}
.layout-gallery{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.stylecard{position:relative;border:1.5px solid var(--line);border-radius:14px;background:var(--surface);padding:8px;color:var(--text);text-align:left}
.stylecard.selected{border-color:var(--accent);background:color-mix(in srgb,var(--accent) 10%,var(--surface))}
.stylecard:disabled{opacity:.42;cursor:not-allowed}
.preview{aspect-ratio:280/456;border-radius:9px;background:#050705;display:grid;place-items:center;overflow:hidden;color:#e8eee2;font-weight:800;text-align:center;letter-spacing:-.04em}
.preview.compact{aspect-ratio:16/10;font-size:13px;letter-spacing:0}
.preview.minimal{font-size:16px}
.preview.orbit{place-items:start;padding:10px;font-size:12px}
.preview.blank{color:#2a302a}
.preview.custom{background:linear-gradient(145deg,#253025,#080b08);font-size:11px;color:#aab5a6}
.preview.gaggimate{font-size:12px;color:#84b77c;letter-spacing:0}
.stylecard>span:last-child{display:block;padding:7px 2px 1px;font-size:12.5px;font-weight:700}
.savebar{position:sticky;bottom:14px;display:flex;justify-content:flex-end;align-items:center;gap:10px;background:color-mix(in srgb,var(--surface) 93%,transparent);backdrop-filter:blur(14px);padding:12px 14px;border:1px solid var(--line);border-radius:15px;box-shadow:var(--shadow);z-index:10}
.details{display:grid;grid-template-columns:130px 1fr;gap:8px 14px;margin:0;font-size:14px}
.details dt{color:var(--muted)}
.details dd{margin:0;overflow-wrap:anywhere}
.details a{color:var(--accent)}
.logview{max-height:250px;overflow:auto;white-space:pre-wrap;word-break:break-word;background:#0a0d0a;color:#d2e0cc;border:1px solid var(--line);border-radius:12px;padding:13px;font:12px/1.45 ui-monospace,SFMono-Regular,Consolas,monospace}
.toast{position:fixed;right:22px;bottom:22px;z-index:60;max-width:360px;background:var(--text);color:var(--bg);border-radius:13px;padding:12px 16px;box-shadow:var(--shadow);transform:translateY(90px);opacity:0;transition:.24s;font-weight:600}
.toast.show{transform:none;opacity:1}
.toast.error{background:var(--danger);color:#fff}

@media(max-width:820px){
  .studio-layout{grid-template-columns:1fr}
  .settings-layout,.history-layout{grid-template-columns:1fr}
  .settingsnav{display:none}
}
@media(max-width:760px){
  .shell{padding:0 14px 96px}
  .barinner{padding:9px 14px;min-height:60px}
  .nav{position:fixed;bottom:0;left:0;right:0;z-index:40;justify-content:space-around;background:color-mix(in srgb,var(--surface) 95%,transparent);backdrop-filter:blur(18px);-webkit-backdrop-filter:blur(18px);border-top:1px solid var(--line);padding:7px max(8px,env(safe-area-inset-right)) calc(7px + env(safe-area-inset-bottom))}
  .nav button{flex:1;padding:10px 4px;font-size:12px}
  .hero{padding-top:22px;align-items:flex-start}
  .dose{display:grid;justify-items:start;gap:2px;padding:11px 12px}
  .dose strong{font-size:15px}
  .metrics,.summary{grid-template-columns:repeat(2,1fr)}
  .metrics .metric:last-child,.summary .metric:last-child{grid-column:1/-1}
  .detailmeta,.engine-grid,.formgrid,.checks,.profiles,.stats-strip{grid-template-columns:1fr}
  .theme-grid{grid-template-columns:repeat(2,1fr)}
  .layout-gallery{grid-template-columns:repeat(3,1fr)}
  .card{padding:17px}
  .chartwrap{height:230px}
  .savebar{flex-wrap:wrap;bottom:76px}
  .savebar #saveHint{flex-basis:100%}
  .savebar .btn{flex:1}
  .toast{left:14px;right:14px;bottom:84px}
}
</style>
</head>
<body>
<header class="appbar">
  <div class="barinner">
    <div class="brand">
      <span class="mark"><svg viewBox="0 0 32 32" aria-hidden="true"><path d="M22.9 7.1c4.4 4.4 2.9 12.7-2.7 17.1S7.6 27.1 4.8 22.3 4.3 10.1 10 6.2s9.4-2.5 12.9.9Z"/><path d="M8.4 23.5c4.7-2.7 3.5-6.2 2.5-9.3-.9-2.8-1.6-5.2 1.7-8.4"/></svg></span>
      <div><span>Smart Grind</span><small>Studio</small></div>
    </div>
    <nav class="nav" aria-label="Main navigation">
      <button class="active" data-page="dashboard">Dashboard</button>
      <button data-page="notes">Note Studio</button>
      <button data-page="history">History</button>
      <button data-page="settings">Settings</button>
    </nav>
    <button id="themeButton" class="iconbtn" aria-label="Toggle color theme" title="Toggle color theme">&#9728;</button>
  </div>
</header>

<main class="shell">
  <!-- DASHBOARD -->
  <section id="dashboard" class="page active">
    <div class="hero">
      <div>
        <div class="eyebrow">Grind by weight</div>
        <h1>Precision, dialed in.</h1>
        <p class="sub" id="target">Waiting for grinder state…</p>
      </div>
      <div class="pills">
        <div id="cupPill" class="pill"><span class="dot"></span><span id="cupPillText">Scale check…</span></div>
        <div id="connection" class="pill"><span class="dot"></span><span>Connecting…</span></div>
      </div>
    </div>

    <div id="dashNoteBanner" class="note-banner">
      <div><strong id="dashNoteTitle">Morning Note armed</strong><div class="sub" id="dashNoteSub">Scheduled on the AMOLED screen</div></div>
      <button type="button" id="openNotesBtn" class="btn secondary small">Edit note</button>
    </div>

    <div class="bar-row">
      <div class="seg" role="group" aria-label="Grind mode">
        <button type="button" id="modeWeightBtn" class="active">Weight</button>
        <button type="button" id="modeTimeBtn">Time</button>
      </div>
      <span class="sub" id="engineBadge">Engine: loading…</span>
    </div>

    <div id="dosePicker" class="dose-picker" aria-label="Target profile">
      <button class="dose" data-profile="0" disabled><span>Single</span><strong class="num">—</strong></button>
      <button class="dose" data-profile="1" disabled><span>Double</span><strong class="num">—</strong></button>
      <button class="dose" data-profile="2" disabled><span>Custom</span><strong class="num">—</strong></button>
    </div>

    <div class="grid metrics">
      <div class="metric"><label>Weight</label><strong id="weight" class="num">—</strong><small>grams</small></div>
      <div class="metric"><label>Flow rate</label><strong id="flow" class="num">—</strong><small>g/s</small></div>
      <div class="metric"><label>Progress</label><strong id="progressText" class="num">—</strong><small id="phase">Idle</small></div>
    </div>

    <div class="card" style="margin-top:16px">
      <div class="cardhead">
        <div><h2>Live grind telemetry</h2><span class="sub">Real-time weight &amp; flow curve, followed by full recorded trace</span></div>
        <span class="sub"><span style="color:var(--accent)">&#9679;</span> Weight&nbsp;&nbsp;<span style="color:var(--sage)">&#9679;</span> Flow</span>
      </div>
      <div class="chartwrap"><canvas id="liveChart"></canvas></div>
      <div class="progress"><i id="progressBar"></i></div>
      <div class="actions">
        <button id="grindButton" class="btn grind-control" disabled aria-label="Start grind" title="Start grind">&#9654;</button>
        <button id="tareButton" class="btn secondary" disabled>Tare scale</button>
        <button id="dismissButton" class="btn secondary" disabled>Dismiss result</button>
      </div>
      <div id="commandMessage" class="message"></div>
    </div>
  </section>

  <!-- MORNING NOTE STUDIO -->
  <section id="notes" class="page">
    <div class="hero">
      <div>
        <div class="eyebrow">AMOLED Display Card</div>
        <h1>Morning Note Studio</h1>
        <p class="sub">Leave a scheduled message on the grinder screen. Supports English, Hebrew RTL, and emojis without overwriting your screensaver.</p>
      </div>
      <div class="pills">
        <div id="noteStatusPill" class="pill"><span class="dot"></span><span id="noteStatusText">No active note</span></div>
      </div>
    </div>

    <div class="grid studio-layout">
      <div class="card bezel-wrap">
        <div class="bezel">
          <canvas id="noteCanvas" width="280" height="456"></canvas>
        </div>
        <div class="bezel-meta">
          <span>280 &times; 456 RGB565</span>
          <span>AMOLED pixel-shift safe</span>
        </div>
        <div class="status-box" style="width:100%">
          <strong id="noteClockState">Checking grinder clock…</strong>
          <span class="sub" id="noteScheduleSummary">Compose a message and send it to the grinder.</span>
        </div>
      </div>

      <div class="grid">
        <div class="card">
          <div class="cardhead">
            <div><h2>Message composer</h2><span class="sub">Automatic RTL detection for Hebrew &amp; mixed text</span></div>
            <div class="chips" style="margin:0">
              <button type="button" class="chip" data-preset="hebrew">Hebrew morning</button>
              <button type="button" class="chip" data-preset="coffee">Good morning</button>
            </div>
          </div>

          <div class="grid" style="gap:12px">
            <label class="field">
              <span>Headline (optional)</span>
              <input id="noteHeadline" class="input" dir="auto" maxlength="44" placeholder="Good Morning ☕">
            </label>
            <label class="field">
              <span>Message</span>
              <textarea id="noteBody" class="input" dir="auto" maxlength="180" placeholder="Write your message in English or Hebrew…"></textarea>
              <div class="chips" aria-label="Insert emoji">
                <button type="button" class="chip" data-emoji="☕">☕</button>
                <button type="button" class="chip" data-emoji="❤️">❤️</button>
                <button type="button" class="chip" data-emoji="✨">✨</button>
                <button type="button" class="chip" data-emoji="🌅">🌅</button>
                <button type="button" class="chip" data-emoji="🥐">🥐</button>
                <button type="button" class="chip" data-emoji="🌸">🌸</button>
              </div>
            </label>
            <div class="formgrid">
              <label class="field">
                <span>Signature (optional)</span>
                <input id="noteSignature" class="input" dir="auto" maxlength="40" placeholder="With love">
              </label>
              <label class="field">
                <span>Message text size</span>
                <div class="range"><input id="noteFontSize" type="range" min="20" max="34" value="26"><output id="noteFontSizeOut" class="num">26 px</output></div>
              </label>
            </div>
          </div>

          <div class="divider"></div>
          <div class="group-title" style="margin-bottom:8px">AMOLED Card Theme</div>
          <div class="theme-grid" id="noteThemes">
            <button type="button" class="tcard selected" data-note-theme="espresso"><span class="swatch" style="background:linear-gradient(135deg,#120d08,#d4a96a)"></span>Espresso Gold</button>
            <button type="button" class="tcard" data-note-theme="sage"><span class="swatch" style="background:linear-gradient(135deg,#09120b,#84b77c)"></span>Botanical Sage</button>
            <button type="button" class="tcard" data-note-theme="crema"><span class="swatch" style="background:linear-gradient(135deg,#1c120b,#e29b59)"></span>Warm Crema</button>
            <button type="button" class="tcard" data-note-theme="noir"><span class="swatch" style="background:linear-gradient(135deg,#000000,#e8ebe4)"></span>Minimal Noir</button>
          </div>
          <div class="actions" style="margin-top:12px">
            <input id="noteBgFile" type="file" accept="image/*" hidden>
            <button type="button" id="noteBgChoose" class="btn secondary small">Add subtle photo background</button>
            <button type="button" id="noteBgClear" class="btn secondary small" hidden>Remove photo</button>
          </div>
        </div>

        <div class="card">
          <div class="cardhead">
            <div><h2>Schedule &amp; auto-clear</h2><span class="sub">Wakes the display during the window (max 6 hours for AMOLED health)</span></div>
          </div>
          <div class="seg" id="noteModeSeg" style="margin-bottom:14px;flex-wrap:wrap">
            <button type="button" data-note-mode="tomorrow" class="active">Tomorrow morning</button>
            <button type="button" data-note-mode="immediate">Show now</button>
            <button type="button" data-note-mode="date">Pick date</button>
            <button type="button" data-note-mode="daily">Every morning</button>
          </div>
          <p id="noteModeHint" class="sub" style="margin:-4px 0 14px"></p>

          <div id="noteWindowFields" class="formgrid">
            <label class="field" id="noteDateField" hidden>
              <span>Date</span>
              <input id="noteDate" class="input" type="date">
            </label>
            <label class="field">
              <span>Start time</span>
              <input id="noteStartTime" class="input" type="time" value="06:30">
            </label>
            <label class="field">
              <span>End time</span>
              <input id="noteEndTime" class="input" type="time" value="11:00">
            </label>
          </div>

          <div class="checks" style="margin-top:14px;grid-template-columns:1fr">
            <label class="check">
              <input id="noteClearOnGrind" type="checkbox" checked>
              <span>Clear automatically after the first completed coffee grind</span>
            </label>
          </div>

          <div class="actions">
            <button type="button" id="sendNoteBtn" class="btn">Send note to grinder</button>
            <button type="button" id="previewNoteBtn" class="btn secondary">Preview 10s on screen</button>
            <button type="button" id="clearNoteBtn" class="btn danger">Clear note</button>
          </div>
          <div id="noteActionMessage" class="message"></div>
        </div>
      </div>
    </div>
  </section>

  <!-- HISTORY -->
  <section id="history" class="page">
    <div class="hero">
      <div>
        <div class="eyebrow">On-device archive</div>
        <h1>Grind history</h1>
        <p class="sub">Review dosing accuracy, flow stability, and pulse count from recorded sessions.</p>
      </div>
      <button id="refreshHistory" class="btn secondary">Refresh</button>
    </div>
    <div class="grid summary">
      <div class="metric"><label>Saved grinds</label><strong id="historyCount" class="num">—</strong></div>
      <div class="metric"><label>Average error</label><strong id="averageError" class="num">—</strong></div>
      <div class="metric"><label>Consistency</label><strong id="consistency" class="num">—</strong></div>
    </div>
    <div class="grid history-layout">
      <div class="card">
        <div class="cardhead"><h2>Sessions</h2></div>
        <div id="historyList" class="history-list"><div class="empty">Loading history…</div></div>
      </div>
      <div id="historyDetail" class="card"><div class="empty">Select a grind to inspect its trace and metrics.</div></div>
    </div>
  </section>

  <!-- SETTINGS & PRECISION CONTROL -->
  <section id="settings" class="page">
    <div class="hero">
      <div>
        <div class="eyebrow">Device configuration</div>
        <h1>Settings &amp; Safety</h1>
        <p class="sub">Control precision grinding engine, remote safety interlocks, display layouts, and firmware updates.</p>
      </div>
    </div>
    <p id="settingsPending" role="status" class="sub"></p>

    <div class="grid settings-layout">
      <aside class="card settingsnav">
        <button type="button" data-anchor="safetySection" class="active">Precision &amp; Safety</button>
        <button type="button" data-anchor="profilesSection">Profiles</button>
        <button type="button" data-anchor="behaviourSection">Grinding</button>
        <button type="button" data-anchor="displaySection">Display</button>
        <button type="button" data-anchor="connectivitySection">Connectivity</button>
        <button type="button" data-anchor="systemSection">System &amp; Updates</button>
      </aside>

      <div>
        <!-- PRECISION & REMOTE SAFETY (Immediate JSON /api/v1/safety) -->
        <section id="safetySection" class="card settingssection">
          <div class="cardhead">
            <div><h2>Precision engine &amp; remote safety</h2><span class="sub">Switch between Adaptive Zero-Shot and Classic 1:1 grinding, calibrate fork, and tune interlocks</span></div>
            <button type="button" id="refreshSafetyBtn" class="btn secondary small">Refresh live</button>
          </div>

          <div class="group-title" style="margin-bottom:8px">Grinding Algorithm</div>
          <div class="engine-grid">
            <button type="button" class="ecard selected" id="engineAdaptiveBtn">
              <strong>Adaptive Zero-Shot</strong>
              <span>Flow-compensated coast cutoff + EMA cross-session coast learning for faster, tighter doses.</span>
            </button>
            <button type="button" class="ecard" id="engineClassicBtn">
              <strong>Classic Upstream (1:1)</strong>
              <span>Instant kill-switch back to the original stock predictive + pulse algorithm.</span>
            </button>
          </div>

          <div class="stats-strip">
            <div class="stat-mini"><span>Learned coast</span><strong id="learnedCoastVal" class="num">— ms</strong></div>
            <div class="stat-mini"><span>Reference flow</span><strong id="refFlowVal" class="num">— g/s</strong></div>
            <div class="stat-mini"><span>Learned sessions</span><strong id="learnedSamplesVal" class="num">—</strong></div>
          </div>

          <div class="checks" style="margin-bottom:14px">
            <label class="check"><input id="adaptiveCoastEnabled" type="checkbox"><span>Cross-session coast learning (EMA)</span></label>
            <div style="display:flex;align-items:center;justify-content:flex-end">
              <button type="button" id="resetCoastBtn" class="btn secondary small">Reset learned coast</button>
            </div>
          </div>

          <div class="divider"></div>
          <div class="group-title" style="margin-bottom:8px">Cup &amp; Portafilter Interlock</div>
          <div class="meter-box">
            <div class="meter-head">
              <div><strong id="liveGrossWeight" class="num">— g</strong> <span class="sub">gross weight on fork</span></div>
              <span id="liveCupStatusBadge" class="pill"><span class="dot"></span><span>Checking…</span></span>
            </div>
            <div class="meter-track">
              <div id="cupMeterZone" class="meter-zone"></div>
              <div id="cupMeterFill" class="meter-fill"></div>
            </div>
            <div class="actions" style="margin-top:12px">
              <button type="button" id="calibrateEmptyBtn" class="btn secondary small">Calibrate empty fork</button>
              <span class="sub" id="emptyCalStatus">Checking calibration…</span>
            </div>
          </div>

          <div class="checks" style="margin-bottom:14px">
            <label class="check"><input id="requireCupRemote" type="checkbox"><span>Require cup/portafilter for remote grinds</span></label>
            <label class="field"><span>Wi-Fi TX power cap</span>
              <select id="wifiTxPower">
                <option value="0">Auto (full power)</option>
                <option value="18">18 dBm (balanced)</option>
                <option value="15">15 dBm (low noise)</option>
                <option value="13">13 dBm (quiet scale)</option>
                <option value="11">11 dBm (ultra quiet)</option>
                <option value="8">8 dBm (minimum)</option>
              </select>
            </label>
          </div>

          <div class="formgrid">
            <label class="field"><span>Min cup weight (g)</span><input id="cupMinWeight" class="input num" type="number" min="5" max="940" step="1"></label>
            <label class="field"><span>Max cup weight (g)</span><input id="cupMaxWeight" class="input num" type="number" min="10" max="950" step="1"></label>
            <label class="field"><span>Remote max grind time (s)</span><input id="remoteMaxTime" class="input num" type="number" min="10" max="60" step="1"></label>
            <label class="field"><span>Remote max dose (g, automations)</span><input id="remoteMaxWeight" class="input num" type="number" min="4" max="120" step="0.5"></label>
            <label class="field"><span>No-flow watchdog (ms, 0=off)</span><input id="noFlowTimeout" class="input num" type="number" min="0" max="10000" step="250"></label>
            <label class="field"><span>No-flow min gain (g)</span><input id="noFlowMinWeight" class="input num" type="number" min="0.1" max="3.0" step="0.1"></label>
          </div>

          <div class="actions">
            <button type="button" id="saveSafetyBtn" class="btn">Save precision &amp; safety</button>
            <span id="safetyHint" class="sub">Saved independently to NVS.</span>
          </div>
        </section>

        <!-- CORE GRINDER SETTINGS FORM (/api/v1/settings) -->
        <form id="settingsForm">
          <section id="profilesSection" class="card settingssection">
            <div class="cardhead"><div><h2>Profiles</h2><span class="sub">Dose weights and timed targets</span></div></div>
            <div class="formgrid" style="margin-bottom:14px">
              <label class="field"><span>Active profile</span><select id="currentProfile"><option value="0">Single</option><option value="1">Double</option><option value="2">Custom</option></select></label>
              <label class="field"><span>Grind mode</span><select id="grindMode"><option value="weight">Weight</option><option value="time">Time</option></select></label>
            </div>
            <div class="profiles" id="profileFields"></div>
          </section>

          <section id="behaviourSection" class="card settingssection">
            <div class="cardhead"><div><h2>Grinding behaviour</h2><span class="sub">Automatic actions, purge, and motor response</span></div></div>
            <div class="checks">
              <label class="check"><input id="autoStart" type="checkbox"><span>Auto-start when cup is placed</span></label>
              <label class="check"><input id="autoReturn" type="checkbox"><span>Return when cup is removed</span></label>
              <label class="check"><input id="swipeEnabled" type="checkbox"><span>Swipe between grind modes</span></label>
              <label class="check"><input id="loggingEnabled" type="checkbox"><span>Save grind history</span></label>
            </div>
            <div class="formgrid" style="margin-top:14px">
              <label class="field"><span>Auto-start cup threshold (g)</span><input id="autoStartThreshold" class="input num" type="number" min="5" max="1000" step="5"><small class="sub">Set below empty cup or portafilter weight.</small></label>
              <label class="field"><span>Prime / purge mode</span><select id="purgeMode"><option value="0">Prime — keep coffee</option><option value="1">Purge — discard grinds</option></select></label>
              <label class="field"><span>Purge amount (g)</span><input id="purgeAmount" class="input num" type="number" min="0.1" max="2.5" step="0.1"></label>
              <label class="field"><span>Freshness window (hours)</span><select id="freshness"><option>.5</option><option>1</option><option>2</option><option>3</option><option>4</option><option>8</option><option>12</option><option>24</option><option>48</option></select></label>
              <label class="field"><span>Motor latency (ms)</span><input id="motorLatency" class="input num" type="number" min="30" max="300" step="5"><small class="sub">Pulse Tune sets this automatically.</small></label>
              <label class="field"><span>Coast ratio</span><input id="coastRatio" class="input num" type="number" min="0.7" max="1.5" step="0.05"></label>
            </div>
          </section>

          <section id="displaySection" class="card settingssection">
            <div class="cardhead"><div><h2>Display, layout &amp; screensaver</h2><span class="sub">Grinding animation style, brightness, and AMOLED protection</span></div></div>

            <div class="group-title" style="margin-bottom:8px">On-device grinding screen</div>
            <div class="layout-gallery" id="grindLayoutGallery" style="margin-bottom:18px">
              <button type="button" class="stylecard" data-layout="portafilter"><span class="preview compact" style="color:#d4a96a">Portafilter<br><small style="font-weight:500;color:#9ea99a">Animated fill</small></span><span>Portafilter</span></button>
              <button type="button" class="stylecard" data-layout="chart"><span class="preview compact" style="color:#84b77c">Live Chart<br><small style="font-weight:500;color:#9ea99a">Weight &amp; flow</small></span><span>Telemetry Chart</span></button>
              <button type="button" class="stylecard" data-layout="arc"><span class="preview compact" style="color:#f4f5ef">Minimal Arc<br><small style="font-weight:500;color:#9ea99a">Clean gauge</small></span><span>Minimal Arc</span></button>
            </div>

            <div class="formgrid">
              <label class="field"><span>Display brightness</span><div class="range"><input id="brightness" type="range" min="15" max="100"><output id="brightnessOut" class="num">100%</output></div></label>
              <label class="field"><span>Screensaver brightness</span><div class="range"><input id="screensaverBrightness" type="range" min="15" max="100"><output id="screensaverBrightnessOut" class="num">35%</output></div></label>
              <label class="field"><span>Screensaver after</span><select id="idleTimeout"><option value="30">30 seconds</option><option value="60">1 minute</option><option value="120">2 minutes</option><option value="300">5 minutes</option><option value="600">10 minutes</option><option value="1800">30 minutes</option><option value="3600">1 hour</option></select></label>
              <label class="field"><span>Turn display off later</span><select id="displayOffDelay"><option value="30">30 seconds later</option><option value="60">1 minute later</option><option value="120">2 minutes later</option><option value="300">5 minutes later</option><option value="600">10 minutes later</option><option value="1800">30 minutes later</option><option value="3600">1 hour later</option><option value="7200">2 hours later</option><option value="43200">12 hours later</option></select></label>
              <label class="field"><span>Startup duration (s)</span><input id="startupTimeout" class="input num" type="number" min="1" max="30" step="1"></label>
            </div>
            <div class="checks" style="margin:14px 0">
              <label class="check"><input id="screensaverStartup" type="checkbox"><span>Show on startup</span></label>
              <label class="check"><input id="screensaverSleep" type="checkbox"><span>Show while idle</span></label>
              <label class="check"><input id="displayOffEnabled" type="checkbox"><span>Fully turn AMOLED off after extra delay</span></label>
            </div>

            <div class="group-title" style="margin-bottom:8px">Screensaver design</div>
            <div class="gallery" id="screensaverGallery">
              <button type="button" class="stylecard" data-style="minimal"><span class="preview minimal">SMART<br>GRIND</span><span>Minimal</span></button>
              <button type="button" class="stylecard" data-style="orbit"><span class="preview orbit">SG</span><span>Orbit</span></button>
              <button type="button" class="stylecard" data-style="blank"><span class="preview blank">AMOLED</span><span>Black</span></button>
              <button type="button" class="stylecard" data-style="gaggimate"><span class="preview gaggimate">93.0 C<br>READY</span><span>GaggiMate</span></button>
              <button type="button" class="stylecard" data-style="custom"><span class="preview custom" id="customPreview">Your image</span><span>Custom</span></button>
            </div>
            <label class="field" style="margin-top:14px"><span>GaggiMate host or IP</span><input id="gaggimateHost" class="input" maxlength="63" placeholder="gaggimate.local"></label>
            <div class="actions">
              <input id="screensaverFile" type="file" accept="image/*" hidden>
              <button type="button" id="chooseScreensaver" class="btn secondary small">Upload custom image</button>
              <button type="button" id="deleteScreensaver" class="btn secondary small">Remove custom image</button>
            </div>
            <canvas id="screensaverCanvas" width="280" height="456" hidden></canvas>
            <div id="screensaverMessage" class="message"></div>
          </section>

          <section id="connectivitySection" class="card settingssection">
            <div class="cardhead"><div><h2>Connectivity</h2><span class="sub">Local Wi-Fi network and Bluetooth</span></div></div>
            <div class="checks"><label class="check"><input id="bluetoothStartup" type="checkbox"><span>Enable Bluetooth at startup</span></label></div>
            <dl class="details" style="margin-top:16px">
              <dt>Wi-Fi</dt><dd id="network">Loading…</dd>
              <dt>Web address</dt><dd><a id="hostname" href="/">Loading…</a></dd>
              <dt>IP address</dt><dd id="address" class="num">Loading…</dd>
            </dl>
            <p class="sub" style="margin-top:12px">To change Wi-Fi credentials, open the Wi-Fi panel on the grinder touchscreen to prevent accidental lockout.</p>
          </section>

          <section id="systemSection" class="card settingssection">
            <div class="cardhead"><div><h2>System &amp; updates</h2><span class="sub">Build info, diagnostic buffer, and OTA firmware</span></div></div>
            <dl class="details">
              <dt>Firmware</dt><dd id="firmware" class="num">Loading…</dd>
              <dt>Hardware</dt><dd id="hardware">Loading…</dd>
              <dt>Device ID</dt><dd id="deviceId" class="num">Loading…</dd>
            </dl>
            <div class="divider"></div>
            <div class="cardhead">
              <div><h3>Recent diagnostic log</h3><span class="sub">Latest 4 KB retained in memory</span></div>
              <div class="actions" style="margin:0">
                <button id="refreshLogs" type="button" class="btn secondary small">Refresh</button>
                <a class="btn secondary small" href="/api/v1/logs" download="smartgrind-log.txt">Download</a>
              </div>
            </div>
            <pre id="diagnosticLog" class="logview">Select Refresh to load the recent diagnostic log.</pre>
            <div class="divider"></div>
            <div class="cardhead">
              <div><h3>Automatic updates</h3><span id="releaseStatus" class="sub">Waiting for device information…</span></div>
              <div class="actions" style="margin:0">
                <button id="checkUpdate" type="button" class="btn secondary small">Check again</button>
                <a id="releaseNotes" class="btn secondary small" style="display:none" target="_blank" rel="noopener">Release notes</a>
              </div>
            </div>
            <div class="actions"><button id="installUpdate" type="button" class="btn" style="display:none">Install update</button></div>
            <div class="divider"></div>
            <h3>Manual firmware file</h3>
            <p id="otaMessage" class="sub">Advanced fallback: choose a compatible ESP32 .bin application image.</p>
            <input id="firmwareFile" class="input" type="file" accept=".bin,application/octet-stream" style="margin-top:8px">
            <div class="progress"><i id="otaProgress"></i></div>
            <button id="otaButton" type="button" class="btn secondary">Upload firmware</button>
          </section>

          <div class="savebar">
            <span id="saveHint" class="sub" role="status" style="margin:auto auto auto 4px">Device settings are saved to the grinder.</span>
            <button type="button" id="reloadSettings" class="btn secondary">Undo</button>
            <button type="submit" id="saveSettings" class="btn">Save settings</button>
          </div>
        </form>
      </div>
    </div>
  </section>
</main>

<div id="toast" class="toast" role="status"></div>

<script>
const $=id=>document.getElementById(id);
const liveSamples=[],liveFlows=[];
const state={
  page:'dashboard',settings:null,safety:null,note:null,
  dashboardProfiles:null,dashboardMode:'weight',
  history:[],selected:null,deviceStatus:null,
  latestRelease:null,latestAsset:null,releaseChecked:false,
  screensaverStyle:'minimal',grindLayout:'portafilter',
  noteTheme:'espresso',noteMode:'tomorrow',noteBgImg:null,
  adaptiveEnabled:true,
  theme:localStorage.getItem('smartgrind-theme')||'auto'
};
let ws,retry=500,toastTimer,lastPhase='IDLE',resultSyncToken=0,displayFlow=0,commandPending=false;

function esc(s){return String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function trimUtf8(str,maxBytes){
  const enc=new TextEncoder();
  let out='';
  for(const ch of String(str||'')){
    if(enc.encode(out+ch).length>maxBytes)break;
    out+=ch;
  }
  return out;
}
function applyTheme(){
  const dark=state.theme==='dark'||(state.theme==='auto'&&matchMedia('(prefers-color-scheme:dark)').matches);
  document.documentElement.dataset.theme=dark?'dark':'light';
  $('themeButton').textContent=dark?'\u2600':'\u263E';
  document.querySelector('meta[name=theme-color]').content=dark?'#0e110e':'#f4f2eb';
}
applyTheme();
$('themeButton').onclick=()=>{
  const dark=document.documentElement.dataset.theme==='dark';
  state.theme=dark?'light':'dark';
  localStorage.setItem('smartgrind-theme',state.theme);
  applyTheme();drawLive();if(state.selected)drawHistory(state.selected);
};
function toast(text,error=false){
  clearTimeout(toastTimer);
  const t=$('toast');t.textContent=text;
  t.className='toast show'+(error?' error':'');
  toastTimer=setTimeout(()=>t.className='toast',3500);
}
function showPage(page){
  state.page=page;
  document.querySelectorAll('.page').forEach(p=>p.classList.toggle('active',p.id===page));
  document.querySelectorAll('.nav button').forEach(b=>b.classList.toggle('active',b.dataset.page===page));
  history.replaceState(null,'','#'+page);
  if(page==='dashboard')drawLive();
  if(page==='notes'){renderNoteCanvas();loadNote();}
  if(page==='history'&&!state.history.length)loadHistory();
  if(page==='settings'){if(!state.settings)loadSettings();loadSafety();}
}
document.querySelectorAll('.nav button').forEach(b=>b.onclick=()=>showPage(b.dataset.page));
$('openNotesBtn').onclick=()=>showPage('notes');
const initialHash=location.hash.slice(1);
if(['dashboard','notes','history','settings'].includes(initialHash))showPage(initialHash);

async function api(url,options){
  const r=await fetch(url,{cache:'no-store',...options});
  const type=r.headers.get('content-type')||'';
  const body=type.includes('json')?await r.json():await r.text();
  if(!r.ok)throw new Error(body?.error||body||`Request failed (${r.status})`);
  return body;
}

/* --- Charts & Live Telemetry --- */
function chart(canvas,series,label='live'){
  const d=devicePixelRatio||1,w=canvas.clientWidth,h=canvas.clientHeight;
  if(!w||!h)return;
  if(canvas.width!==w*d||canvas.height!==h*d){canvas.width=w*d;canvas.height=h*d;}
  const c=canvas.getContext('2d');
  c.setTransform(d,0,0,d,0,0);c.clearRect(0,0,w,h);
  const css=getComputedStyle(document.documentElement),line=css.getPropertyValue('--line'),muted=css.getPropertyValue('--muted');
  c.strokeStyle=line;c.lineWidth=1;c.beginPath();
  for(let i=1;i<5;i++){c.moveTo(0,h*i/5);c.lineTo(w,h*i/5);}
  c.stroke();
  series.forEach((s,si)=>{
    if(s.values.length<2)return;
    let lo=Math.min(...s.values),hi=Math.max(...s.values);
    if(hi-lo<.2){lo-=.1;hi+=.1;}
    c.strokeStyle=s.color||css.getPropertyValue(si?'--sage':'--accent');
    c.lineWidth=2.2;c.beginPath();
    s.values.forEach((v,i)=>{
      const x=i*w/(s.values.length-1),y=h-10-(v-lo)*(h-20)/(hi-lo);
      i?c.lineTo(x,y):c.moveTo(x,y);
    });
    c.stroke();
  });
  c.fillStyle=muted;c.font='11px system-ui';c.fillText(label,8,15);
}
function drawLive(){
  const css=getComputedStyle(document.documentElement);
  chart($('liveChart'),[{values:liveSamples,color:css.getPropertyValue('--accent')},{values:liveFlows,color:css.getPropertyValue('--sage')}]);
}
function stableFlow(raw,active){
  displayFlow+=(raw-displayFlow)*(active?0.35:0.12);
  if(Math.abs(displayFlow)<(active?0.04:0.10))displayFlow=0;
  return displayFlow;
}
function updateCupBadge(el,status,gross){
  const g=Number.isFinite(gross)?gross.toFixed(0)+' g':'—';
  const map={
    ok:{cls:'pill ok',text:`Cup ready · ${g}`},
    no_cup_detected:{cls:'pill warn',text:`Place cup · ${g}`},
    cup_already_full:{cls:'pill err',text:`Cup full · ${g}`},
    scale_unstable:{cls:'pill warn',text:'Scale settling…'},
    empty_holder_uncalibrated:{cls:'pill',text:'Fork uncalibrated'}
  };
  const m=map[status]||{cls:'pill',text:status||'Unknown'};
  el.className=m.cls;
  el.lastElementChild.textContent=m.text;
}
function render(s){
  const g=s.grind,terminal=['COMPLETED','TIMEOUT'].includes(g.phase),wasTerminal=['COMPLETED','TIMEOUT'].includes(lastPhase);
  const sampling=g.active&&!terminal,idle=g.phase==='IDLE',flow=stableFlow(s.scale.flow,sampling),control=$('grindButton');
  if(sampling&&!(['PREPARING','PRIMING','GRINDING','COASTING','FINAL_SETTLING','PAUSED'].includes(lastPhase))){
    liveSamples.length=0;liveFlows.length=0;displayFlow=0;resultSyncToken++;
  }
  $('weight').textContent=s.scale.weight.toFixed(2);
  $('flow').textContent=flow.toFixed(2);
  $('phase').textContent=g.phase.replaceAll('_',' ').toLowerCase();
  $('progressText').textContent=`${g.progress}%`;
  $('progressBar').style.width=`${g.progress}%`;
  $('target').textContent=g.mode==='manual'?'Manual grind':g.mode==='time'?`Target ${(g.target_time_ms/1000).toFixed(1)} seconds`:`Target ${g.target_weight.toFixed(1)} grams`;
  updateCupBadge($('cupPill'),s.scale.cup_status,s.scale.gross_weight_g);
  if(state.safety){
    state.safety.live={gross_weight_g:s.scale.gross_weight_g,weight_range_g:state.safety.live?.weight_range_g||0,cup_status:s.scale.cup_status};
    if(state.page==='settings')renderCupMeter();
  }
  $('modeWeightBtn').classList.toggle('active',g.mode!=='time');
  $('modeTimeBtn').classList.toggle('active',g.mode==='time');
  $('modeWeightBtn').disabled=!idle||commandPending;
  $('modeTimeBtn').disabled=!idle||commandPending;
  document.querySelectorAll('.dose').forEach(b=>{
    b.classList.toggle('active',+b.dataset.profile===g.profile);
    b.disabled=!idle||!state.dashboardProfiles;
  });
  if(state.settings){state.settings.current_profile=g.profile;$('currentProfile').value=g.profile;}
  control.textContent=sampling?'\u25A0':'\u25B6';
  control.classList.toggle('stop',sampling);
  control.setAttribute('aria-label',sampling?'Stop grind':'Start grind');
  control.title=sampling?'Stop grind':'Start grind';
  control.disabled=commandPending||terminal||(!idle&&!sampling);
  $('tareButton').disabled=commandPending||!idle;
  $('dismissButton').disabled=commandPending||!terminal;
  if(sampling){
    liveSamples.push(s.scale.weight);liveFlows.push(flow);
    if(liveSamples.length>600){liveSamples.shift();liveFlows.shift();}
  }
  if(terminal&&!wasTerminal){syncCompletedTrace();loadNote();loadSafety();}
  lastPhase=g.phase;
  if(state.page==='dashboard')drawLive();
}
async function syncCompletedTrace(){
  const token=++resultSyncToken;
  for(let attempt=0;attempt<12;attempt++){
    await new Promise(r=>setTimeout(r,250));
    try{
      const h=await api('/api/v1/history'),latest=h.sessions?.[0];
      if(!latest)continue;
      const response=await fetch(`/api/v1/history/session?id=${latest.id}`,{cache:'no-store'});
      if(!response.ok)continue;
      const d=parseSession(await response.arrayBuffer());
      if(token!==resultSyncToken)return;
      liveSamples.splice(0,liveSamples.length,...d.rows.map(r=>r.weight));
      liveFlows.splice(0,liveFlows.length,...d.rows.map(r=>r.flow));
      state.history=[];
      if(state.page==='dashboard')drawLive();
      if(state.page==='history')loadHistory();
      return;
    }catch{}
  }
}
function connect(){
  ws=new WebSocket(`${location.protocol==='https:'?'wss':'ws'}://${location.host}/ws`);
  ws.onopen=()=>{$('connection').className='pill live';$('connection').lastElementChild.textContent='Live';retry=500;};
  ws.onmessage=e=>{
    const m=JSON.parse(e.data);
    if(m.type==='state')render(m);
    if(m.type==='ack'){
      commandPending=false;
      $('commandMessage').textContent=m.reason;
      toast(m.reason,!m.accepted);
      if(m.action==='set_mode'&&m.accepted)loadDashboardProfiles();
    }
  };
  ws.onclose=()=>{
    commandPending=false;
    $('connection').className='pill';$('connection').lastElementChild.textContent='Reconnecting…';
    setTimeout(connect,retry);retry=Math.min(retry*2,10000);
  };
}
connect();
function command(action,extra={}){
  if(commandPending||ws?.readyState!==1)return;
  commandPending=true;$('grindButton').disabled=true;$('dismissButton').disabled=true;$('tareButton').disabled=true;
  ws.send(JSON.stringify({type:'command',action,...extra}));
}
$('grindButton').onclick=()=>command(lastPhase==='IDLE'?'start':'stop');
$('tareButton').onclick=()=>command('tare');
$('dismissButton').onclick=()=>command('dismiss');
$('modeWeightBtn').onclick=()=>command('set_mode',{mode:'weight'});
$('modeTimeBtn').onclick=()=>command('set_mode',{mode:'time'});

function renderDosePicker(settings){
  state.dashboardProfiles=settings.profiles;
  state.dashboardMode=settings.grind_mode;
  document.querySelectorAll('.dose').forEach(b=>{
    const p=settings.profiles[+b.dataset.profile];
    b.querySelector('strong').textContent=settings.grind_mode==='time'?`${p.time.toFixed(1)} s`:`${p.weight.toFixed(1)} g`;
    b.disabled=lastPhase!=='IDLE';
  });
}
async function selectProfile(profile){
  const p=state.dashboardProfiles?.[profile];
  document.querySelectorAll('.dose').forEach(b=>{b.classList.toggle('active',+b.dataset.profile===profile);b.disabled=true;});
  if(p)$('target').textContent=state.dashboardMode==='time'?`Target ${p.time.toFixed(1)} seconds`:`Target ${p.weight.toFixed(1)} grams`;
  try{
    await api('/api/v1/profile',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({profile})});
    toast(`${['Single','Double','Custom'][profile]} profile selected`);
  }catch(e){toast(e.message,true);}
  finally{setTimeout(()=>document.querySelectorAll('.dose').forEach(b=>b.disabled=lastPhase!=='IDLE'),250);}
}
document.querySelectorAll('.dose').forEach(b=>b.onclick=()=>selectProfile(+b.dataset.profile));
async function loadDashboardProfiles(){try{renderDosePicker(await api('/api/v1/settings'));}catch{}}
loadDashboardProfiles();

/* --- Morning Note Studio --- */
const NOTE_THEMES={
  espresso:{cardBg:'#0b0907',border:'#6a5130',accent:'#d4a96a',title:'#e7c58d',body:'#f6f2ea',sig:'#b89d76',rule:'#4c3b24'},
  sage:{cardBg:'#070d08',border:'#395439',accent:'#84b77c',title:'#a7d49f',body:'#f2f6f0',sig:'#8da88a',rule:'#2b402c'},
  crema:{cardBg:'#140c07',border:'#7a4d28',accent:'#e29b59',title:'#f0b880',body:'#faf2e8',sig:'#c9956b',rule:'#55351c'},
  noir:{cardBg:'#060606',border:'#474747',accent:'#e8ebe4',title:'#ffffff',body:'#f2f3ef',sig:'#9ea39b',rule:'#323232'}
};
function hasRtl(str){return /[\u0590-\u05FF\u0600-\u06FF]/.test(str||'');}
function wrapParagraph(ctx,text,maxW){
  const words=text.trim().split(/\s+/).filter(Boolean);
  if(!words.length)return [''];
  const lines=[];let cur=words[0];
  for(let i=1;i<words.length;i++){
    const test=cur+' '+words[i];
    if(ctx.measureText(test).width<=maxW){cur=test;}
    else{lines.push(cur);cur=words[i];}
  }
  lines.push(cur);
  return lines;
}
function rrect(ctx,x,y,w,h,r){
  if(ctx.roundRect){ctx.roundRect(x,y,w,h,r);return;}
  ctx.moveTo(x+r,y);ctx.arcTo(x+w,y,x+w,y+h,r);ctx.arcTo(x+w,y+h,x,y+h,r);
  ctx.arcTo(x,y+h,x,y,r);ctx.arcTo(x,y,x+w,y,r);ctx.closePath();
}
function renderNoteCanvas(){
  const c=$('noteCanvas'),ctx=c.getContext('2d'),W=280,H=456;
  const t=NOTE_THEMES[state.noteTheme]||NOTE_THEMES.espresso;
  const headline=$('noteHeadline').value.trim();
  const body=$('noteBody').value;
  const sig=$('noteSignature').value.trim();
  const fontSize=parseInt($('noteFontSize').value,10)||26;
  const fontStack='system-ui,-apple-system,"Segoe UI","Heebo","Rubik","Arial Hebrew",sans-serif';

  ctx.save();
  // Pure black outer 12px margin guarantees +-3px AMOLED pixel-shift never clips content
  ctx.fillStyle='#000000';
  ctx.fillRect(0,0,W,H);

  // Inner card panel
  const pad=12,rx=18,iw=W-pad*2,ih=H-pad*2;
  ctx.save();
  ctx.beginPath();
  rrect(ctx,pad,pad,iw,ih,rx);
  ctx.clip();
  ctx.fillStyle=t.cardBg;
  ctx.fillRect(pad,pad,iw,ih);
  if(state.noteBgImg){
    const img=state.noteBgImg,sc=Math.max(iw/img.width,ih/img.height);
    const dw=img.width*sc,dh=img.height*sc;
    ctx.globalAlpha=0.26;
    ctx.drawImage(img,pad+(iw-dw)/2,pad+(ih-dh)/2,dw,dh);
    ctx.globalAlpha=1;
    const grad=ctx.createLinearGradient(0,pad,0,pad+ih);
    grad.addColorStop(0,'rgba(0,0,0,0.45)');
    grad.addColorStop(0.5,'rgba(0,0,0,0.62)');
    grad.addColorStop(1,'rgba(0,0,0,0.72)');
    ctx.fillStyle=grad;ctx.fillRect(pad,pad,iw,ih);
  }
  ctx.restore();

  // Hairline inner frame
  ctx.strokeStyle=t.border;ctx.lineWidth=1.5;
  ctx.beginPath();rrect(ctx,pad+1,pad+1,iw-2,ih-2,rx);ctx.stroke();

  // Minimalist coffee emblem at top
  ctx.save();
  ctx.translate(W/2,52);
  ctx.strokeStyle=t.accent;ctx.lineWidth=2;ctx.lineCap='round';
  ctx.beginPath();ctx.ellipse(0,0,10,13,0.28,0,Math.PI*2);ctx.stroke();
  ctx.beginPath();ctx.moveTo(-2,-11);ctx.bezierCurveTo(3,-4,-3,4,2,11);ctx.stroke();
  ctx.restore();

  let topY=88;
  ctx.textAlign='center';ctx.textBaseline='middle';
  if(headline){
    ctx.direction=hasRtl(headline)?'rtl':'ltr';
    ctx.fillStyle=t.title;
    ctx.font=`700 20px ${fontStack}`;
    const hLines=wrapParagraph(ctx,headline,W-56).slice(0,2);
    hLines.forEach((ln,i)=>ctx.fillText(ln,W/2,topY+i*26));
    topY+=hLines.length*26+12;
  }

  // Subtle horizontal rule
  ctx.strokeStyle=t.rule;ctx.lineWidth=1;
  ctx.beginPath();ctx.moveTo(W/2-38,topY);ctx.lineTo(W/2+38,topY);ctx.stroke();

  // Main message body centered in available vertical space
  const contentTop=topY+20,contentBottom=sig?H-82:H-46;
  const maxTextW=W-54;
  ctx.font=`600 ${fontSize}px ${fontStack}`;
  const rawParas=(body||' ').split('\n');
  let lines=[];
  rawParas.forEach(p=>{
    const wrapped=wrapParagraph(ctx,p,maxTextW);
    wrapped.forEach(w=>lines.push({text:w,rtl:hasRtl(w)}));
  });
  // Auto-shrink slightly if message is tall
  let actualSize=fontSize;
  let lineH=Math.round(actualSize*1.36);
  while(lines.length*lineH>(contentBottom-contentTop)&&actualSize>16){
    actualSize-=2;lineH=Math.round(actualSize*1.36);
    ctx.font=`600 ${actualSize}px ${fontStack}`;
    lines=[];
    rawParas.forEach(p=>wrapParagraph(ctx,p,maxTextW).forEach(w=>lines.push({text:w,rtl:hasRtl(w)})));
  }
  const blockH=lines.length*lineH;
  let y=contentTop+Math.max(0,(contentBottom-contentTop-blockH)/2)+lineH/2;
  ctx.fillStyle=t.body;
  lines.forEach(item=>{
    if(y<=contentBottom+4){
      ctx.direction=item.rtl?'rtl':'ltr';
      ctx.fillText(item.text,W/2,y);
    }
    y+=lineH;
  });

  if(sig){
    ctx.strokeStyle=t.rule;ctx.lineWidth=1;
    ctx.beginPath();ctx.moveTo(W/2-24,H-68);ctx.lineTo(W/2+24,H-68);ctx.stroke();
    ctx.direction=hasRtl(sig)?'rtl':'ltr';
    ctx.fillStyle=t.sig;
    ctx.font=`600 17px ${fontStack}`;
    ctx.fillText(sig,W/2,H-44);
  }
  ctx.restore();
}

function setDefaultNoteDate(){
  const d=new Date();d.setDate(d.getDate()+1);
  const pad=n=>String(n).padStart(2,'0');
  $('noteDate').value=`${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())}`;
}
setDefaultNoteDate();
function setNoteTheme(theme){
  state.noteTheme=theme;
  document.querySelectorAll('[data-note-theme]').forEach(b=>b.classList.toggle('selected',b.dataset.noteTheme===theme));
  renderNoteCanvas();
}
const NOTE_MODE_HINTS={
  tomorrow:'Appears tomorrow between the start and end time, even if the screen is asleep.',
  immediate:'Appears as soon as the grinder is idle and stays until the next grind, for at most 6 hours of screen time. To greet someone tomorrow, use Tomorrow morning.',
  date:'Appears once, on the date you pick, between the start and end time.',
  daily:'Appears every day between the start and end time until you clear it.'
};
function setNoteMode(mode){
  state.noteMode=mode;
  document.querySelectorAll('[data-note-mode]').forEach(b=>b.classList.toggle('active',b.dataset.noteMode===mode));
  $('noteWindowFields').hidden=(mode==='immediate');
  $('noteDateField').hidden=(mode!=='date');
  $('noteModeHint').textContent=NOTE_MODE_HINTS[mode]||'';
}
setNoteMode(state.noteMode);
document.querySelectorAll('[data-note-theme]').forEach(b=>b.onclick=()=>setNoteTheme(b.dataset.noteTheme));
document.querySelectorAll('[data-note-mode]').forEach(b=>b.onclick=()=>setNoteMode(b.dataset.noteMode));
['noteHeadline','noteBody','noteSignature'].forEach(id=>$(id).addEventListener('input',renderNoteCanvas));
$('noteFontSize').oninput=e=>{$('noteFontSizeOut').value=`${e.target.value} px`;renderNoteCanvas();};
document.querySelectorAll('[data-emoji]').forEach(b=>b.onclick=()=>{
  const el=$('noteBody'),ins=b.dataset.emoji,s=el.selectionStart??el.value.length,e=el.selectionEnd??el.value.length;
  el.value=el.value.slice(0,s)+ins+el.value.slice(e);
  el.focus();el.setSelectionRange(s+ins.length,s+ins.length);
  renderNoteCanvas();
});
document.querySelectorAll('[data-preset]').forEach(b=>b.onclick=()=>{
  if(b.dataset.preset==='hebrew'){
    $('noteHeadline').value='בוקר טוב ☕';
    $('noteBody').value='היי אהובתי בוקר טוב,\nבואי נכין קפה ❤️';
    $('noteSignature').value='שיהיה יום קסום ✨';
    setNoteTheme('espresso');
  }else{
    $('noteHeadline').value='Good Morning ☕';
    $('noteBody').value='Fresh beans are dialed in.\nHave an amazing day!';
    $('noteSignature').value='With love ❤️';
    setNoteTheme('sage');
  }
  renderNoteCanvas();
});
$('noteBgChoose').onclick=()=>$('noteBgFile').click();
$('noteBgFile').onchange=async e=>{
  const f=e.target.files[0];if(!f)return;
  try{state.noteBgImg=await createImageBitmap(f);$('noteBgClear').hidden=false;renderNoteCanvas();}
  catch{toast('Could not load image',true);}
};
$('noteBgClear').onclick=()=>{state.noteBgImg=null;$('noteBgFile').value='';$('noteBgClear').hidden=true;renderNoteCanvas();};

function parseHm(str){
  const m=String(str||'').match(/^(\d{2}):(\d{2})$/);
  return m?parseInt(m[1],10)*60+parseInt(m[2],10):null;
}
function fmtMin(min){
  const m=((min%1440)+1440)%1440;
  return `${String(Math.floor(m/60)).padStart(2,'0')}:${String(m%60).padStart(2,'0')}`;
}
function buildNotePayload(){
  const nowSec=Math.floor(Date.now()/1000);
  const tzOffset=-new Date().getTimezoneOffset();
  const payload={
    enabled:true,
    clear_on_grind:$('noteClearOnGrind').checked,
    tz_offset_min:tzOffset,
    headline:trimUtf8($('noteHeadline').value.trim(),62),
    body:trimUtf8($('noteBody').value.trim(),252),
    signature:trimUtf8($('noteSignature').value.trim(),62),
    theme:state.noteTheme,
    client_epoch:nowSec
  };
  if(state.noteMode==='immediate'){
    payload.mode='immediate';
    return payload;
  }
  const sMin=parseHm($('noteStartTime').value),eMin=parseHm($('noteEndTime').value);
  if(sMin===null||eMin===null)throw new Error('Choose valid start and end times');
  const span=eMin>sMin?(eMin-sMin):(1440-sMin+eMin);
  if(span<1||span>360)throw new Error('Window must be between 1 minute and 6 hours (AMOLED protection)');
  if(state.noteMode==='daily'){
    payload.mode='daily';
    payload.start_min=sMin;
    payload.end_min=eMin;
    return payload;
  }
  let baseDate;
  if(state.noteMode==='tomorrow'){
    baseDate=new Date();baseDate.setDate(baseDate.getDate()+1);
  }else{
    const parts=($('noteDate').value||'').split('-').map(Number);
    if(parts.length!==3||parts.some(Number.isNaN))throw new Error('Pick a valid date');
    baseDate=new Date(parts[0],parts[1]-1,parts[2]);
  }
  const startDt=new Date(baseDate.getFullYear(),baseDate.getMonth(),baseDate.getDate(),Math.floor(sMin/60),sMin%60,0,0);
  const endDt=new Date(startDt.getTime()+span*60000);
  payload.mode='one_shot';
  payload.start_epoch=Math.floor(startDt.getTime()/1000);
  payload.end_epoch=Math.floor(endDt.getTime()/1000);
  return payload;
}
function canvasToRgb565(canvas){
  const ctx=canvas.getContext('2d'),rgba=ctx.getImageData(0,0,280,456).data,raw=new Uint8Array(280*456*2);
  for(let i=0,j=0;i<rgba.length;i+=4,j+=2){
    const p=((rgba[i]&248)<<8)|((rgba[i+1]&252)<<3)|(rgba[i+2]>>3);
    raw[j]=p&255;raw[j+1]=p>>8;
  }
  return raw;
}
function renderNoteStatus(n){
  state.note=n;
  const pill=$('noteStatusPill'),txt=$('noteStatusText'),banner=$('dashNoteBanner');
  $('previewNoteBtn').disabled=!n.has_image;
  $('noteClockState').textContent=n.clock_synced
    ?`Grinder clock synced (${new Date(n.device_epoch*1000).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'})})`
    :'Grinder clock will sync automatically from this device when you send.';
  if(!n.enabled||!n.has_image){
    pill.className='pill';txt.textContent='No active note';
    $('noteScheduleSummary').textContent='Compose a message and send it to the grinder.';
    banner.classList.remove('show');
  }else{
    pill.className=n.due_now?'pill ok':'pill action';
    txt.textContent=n.due_now?'Showing on screen':'Scheduled';
    let desc='';
    if(n.mode==='immediate')desc='Shows whenever the grinder is idle (up to 6 h of screen time)';
    else if(n.mode==='daily')desc=`Daily ${fmtMin(n.start_min)} – ${fmtMin(n.end_min)}`;
    else desc=`${new Date(n.start_epoch*1000).toLocaleString([],{month:'short',day:'numeric',hour:'2-digit',minute:'2-digit'})} – ${new Date(n.end_epoch*1000).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'})}`;
    if(n.clear_on_grind)desc+=' · clears after first grind';
    $('noteScheduleSummary').textContent=desc;
    $('dashNoteTitle').textContent=n.due_now?'Morning Note active on display':`Morning Note scheduled${n.headline?' — '+n.headline:''}`;
    $('dashNoteSub').textContent=desc;
    banner.classList.add('show');
  }
}
// Keeps a scheduled note on local time: gives the grinder this device's clock
// when it has none, and its current UTC offset after a daylight-saving change.
async function syncNoteClock(n){
  const tz=-new Date().getTimezoneOffset();
  const scheduled=n.enabled&&n.mode!=='immediate';
  if(!scheduled||(n.clock_synced&&(n.mode!=='daily'||n.tz_offset_min===tz)))return n;
  try{
    return await api('/api/v1/note/clock',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:new URLSearchParams({epoch:Math.floor(Date.now()/1000),tz_offset_min:tz})});
  }catch{return n;}
}
let noteHydrated=false;
async function loadNote(){
  try{
    const n=await syncNoteClock(await api('/api/v1/note'));
    renderNoteStatus(n);
    if(!noteHydrated&&(n.headline||n.body||n.signature)){
      noteHydrated=true;
      $('noteHeadline').value=n.headline||'';
      $('noteBody').value=n.body||'';
      $('noteSignature').value=n.signature||'';
      $('noteClearOnGrind').checked=!!n.clear_on_grind;
      setNoteTheme(n.theme||'espresso');
      if(n.mode==='immediate')setNoteMode('immediate');
      else if(n.mode==='daily'){setNoteMode('daily');$('noteStartTime').value=fmtMin(n.start_min);$('noteEndTime').value=fmtMin(n.end_min);}
      else if(n.mode==='one_shot'&&n.start_epoch){
        const sd=new Date(n.start_epoch*1000),ed=new Date(n.end_epoch*1000),pad=v=>String(v).padStart(2,'0');
        setNoteMode('date');
        $('noteDate').value=`${sd.getFullYear()}-${pad(sd.getMonth()+1)}-${pad(sd.getDate())}`;
        $('noteStartTime').value=`${pad(sd.getHours())}:${pad(sd.getMinutes())}`;
        $('noteEndTime').value=`${pad(ed.getHours())}:${pad(ed.getMinutes())}`;
      }
      renderNoteCanvas();
    }
  }catch{}
}
loadNote();
renderNoteCanvas();

$('sendNoteBtn').onclick=async()=>{
  const msg=$('noteActionMessage');
  try{
    if(!$('noteHeadline').value.trim()&&!$('noteBody').value.trim())throw new Error('Write a headline or message first');
    const payload=buildNotePayload();
    renderNoteCanvas();
    $('sendNoteBtn').disabled=true;
    msg.textContent='Encoding 280×456 AMOLED frame…';
    const raw=canvasToRgb565($('noteCanvas'));
    const form=new FormData();
    form.append('image',new Blob([raw],{type:'application/octet-stream'}),'note.rgb565');
    msg.textContent='Uploading frame (249 KB)…';
    await api('/api/v1/note/image',{method:'POST',body:form});
    msg.textContent='Saving schedule…';
    const saved=await api('/api/v1/note',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});
    renderNoteStatus(saved);
    msg.textContent='Morning note saved to grinder.';
    toast('Morning note sent to grinder');
  }catch(e){msg.textContent=e.message;toast(e.message,true);}
  finally{$('sendNoteBtn').disabled=false;}
};
$('previewNoteBtn').onclick=async()=>{
  const btn=$('previewNoteBtn');
  btn.disabled=true;
  try{
    await api('/api/v1/note/preview',{method:'POST'});
    let outcome='pending';
    for(let i=0;i<8&&outcome==='pending';i++){
      await new Promise(r=>setTimeout(r,250));
      outcome=(await api('/api/v1/note')).preview;
    }
    if(outcome==='shown')toast('Previewing note on grinder screen for 10s');
    else if(outcome==='blocked')toast('The grinder screen is busy. Return it to the home screen and try again.',true);
    else toast('The grinder did not confirm the preview. Check its screen.',true);
  }catch(e){toast(e.message,true);}
  finally{btn.disabled=!state.note?.has_image;}
};
$('clearNoteBtn').onclick=async()=>{
  if(!confirm('Clear the morning note from the grinder?'))return;
  try{
    const cleared=await api('/api/v1/note',{method:'DELETE'});
    renderNoteStatus(cleared);
    $('noteActionMessage').textContent='Morning note cleared.';
    toast('Morning note cleared');
  }catch(e){toast(e.message,true);}
};

/* --- Precision Engine & Remote Safety (/api/v1/safety) --- */
function selectEngine(adaptive){
  state.adaptiveEnabled=!!adaptive;
  $('engineAdaptiveBtn').classList.toggle('selected',state.adaptiveEnabled);
  $('engineClassicBtn').classList.toggle('selected',!state.adaptiveEnabled);
}
$('engineAdaptiveBtn').onclick=()=>selectEngine(true);
$('engineClassicBtn').onclick=()=>selectEngine(false);
function renderCupMeter(){
  const s=state.safety;if(!s)return;
  const gross=s.live?.gross_weight_g??0,minG=s.cup_min_weight_g||50,maxG=s.cup_max_weight_g||600;
  const scaleMax=Math.max(maxG*1.2,gross*1.15,200);
  $('liveGrossWeight').textContent=`${gross.toFixed(1)} g`;
  $('cupMeterFill').style.width=`${Math.max(0,Math.min(100,(gross/scaleMax)*100))}%`;
  $('cupMeterZone').style.left=`${Math.min(100,(minG/scaleMax)*100)}%`;
  $('cupMeterZone').style.width=`${Math.max(2,Math.min(100,((maxG-minG)/scaleMax)*100))}%`;
  updateCupBadge($('liveCupStatusBadge'),s.live?.cup_status,gross);
  $('emptyCalStatus').textContent=s.empty_holder_calibrated
    ?`Empty fork calibrated (raw ${s.empty_holder_raw})`
    :'Not calibrated yet — remove cup and tap Calibrate empty fork.';
}
function renderSafety(s){
  state.safety=s;
  selectEngine(s.adaptive_grind_enabled);
  $('engineBadge').textContent=s.adaptive_grind_enabled
    ?`Engine: Adaptive Zero-Shot (${(s.adaptive_coast?.learned_coast_time_ms||0).toFixed(0)} ms coast)`
    :'Engine: Classic Upstream 1:1';
  $('adaptiveCoastEnabled').checked=!!s.adaptive_coast?.enabled;
  $('learnedCoastVal').textContent=`${(s.adaptive_coast?.learned_coast_time_ms??0).toFixed(0)} ms`;
  $('refFlowVal').textContent=`${(s.adaptive_coast?.reference_flow_gps??0).toFixed(2)} g/s`;
  $('learnedSamplesVal').textContent=`${s.adaptive_coast?.sample_count??0}`;
  $('requireCupRemote').checked=!!s.require_cup_on_remote;
  $('cupMinWeight').value=s.cup_min_weight_g;
  $('cupMaxWeight').value=s.cup_max_weight_g;
  $('remoteMaxTime').value=s.remote_max_grind_time_s;
  $('noFlowTimeout').value=s.no_flow_timeout_ms;
  $('noFlowMinWeight').value=s.no_flow_min_weight_g;
  $('wifiTxPower').value=String(s.wifi_tx_power_dbm??0);
  renderCupMeter();
}
async function loadSafety(){
  try{renderSafety(await api('/api/v1/safety'));}
  catch(e){if(state.page==='settings')toast(e.message,true);}
}
loadSafety();
$('refreshSafetyBtn').onclick=()=>loadSafety();
$('saveSafetyBtn').onclick=async()=>{
  $('saveSafetyBtn').disabled=true;
  $('safetyHint').textContent='Saving to grinder…';
  try{
    const cupMin=parseFloat($('cupMinWeight').value),cupMax=parseFloat($('cupMaxWeight').value);
    const maxTime=parseInt($('remoteMaxTime').value,10),noFlowMs=parseInt($('noFlowTimeout').value,10);
    const noFlowG=parseFloat($('noFlowMinWeight').value),txDbm=parseInt($('wifiTxPower').value,10);
    const maxDose=parseFloat($('remoteMaxWeight').value);
    if(![cupMin,cupMax,maxTime,maxDose,noFlowMs,noFlowG,txDbm].every(Number.isFinite)){
      throw new Error('Enter valid numbers for all safety fields');
    }
    const body={
      adaptive_grind_enabled:state.adaptiveEnabled,
      adaptive_coast_enabled:$('adaptiveCoastEnabled').checked,
      require_cup_on_remote:$('requireCupRemote').checked,
      cup_min_weight_g:cupMin,
      cup_max_weight_g:cupMax,
      remote_max_grind_time_s:maxTime,
      no_flow_timeout_ms:noFlowMs,
      no_flow_min_weight_g:noFlowG,
      wifi_tx_power_dbm:txDbm
    };
    const updated=await api('/api/v1/safety',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    renderSafety(updated);
    $('safetyHint').textContent='Precision & safety settings saved.';
    toast('Precision & safety settings saved');
  }catch(e){$('safetyHint').textContent=e.message;toast(e.message,true);}
  finally{$('saveSafetyBtn').disabled=false;}
};
$('resetCoastBtn').onclick=async()=>{
  $('remoteMaxWeight').value=s.remote_max_weight_g;
  if(!confirm('Reset learned coast time back to factory baseline?'))return;
  try{
    const updated=await api('/api/v1/safety',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({reset_adaptive_coast:true})});
    renderSafety(updated);
    toast('Learned coast reset to baseline');
  }catch(e){toast(e.message,true);}
};
$('calibrateEmptyBtn').onclick=async()=>{
  if(!confirm('Remove any cup or portafilter from the fork, wait until still, then tap OK to calibrate.'))return;
  $('calibrateEmptyBtn').disabled=true;
  try{
    const updated=await api('/api/v1/safety/calibrate-empty',{method:'POST'});
    renderSafety(updated);
    toast('Empty fork calibrated');
  }catch(e){toast(e.message,true);}
  finally{$('calibrateEmptyBtn').disabled=false;}
};

/* --- Core Settings, Display, OTA --- */
let otaRequestPending=false,githubInstall=null;
function renderOtaStatus(s){
  const ota=s.ota,busy=otaRequestPending||ota.active||ota.preparing;
  $('otaProgress').style.width=`${ota.progress||0}%`;
  $('otaButton').disabled=busy;$('installUpdate').disabled=busy;
  $('otaMessage').textContent=ota.failed?'Firmware update failed. Previous firmware retained; check diagnostic log.':ota.active?`Installing ${ota.progress||0}% — do not remove power.`:ota.preparing?'Preparing grinder for update…':'Advanced fallback: choose a compatible ESP32 .bin application image.';
  if(githubInstall&&compareVersions(s.firmware.version,githubInstall.tag)===0){
    githubInstall=null;state.releaseChecked=false;toast('Firmware update confirmed');
  }else if(!otaRequestPending&&(ota.failed||(githubInstall&&!ota.active&&!ota.preparing&&performance.now()-githubInstall.startedAt>3000))){
    githubInstall=null;$('releaseStatus').textContent='Update failed — refresh diagnostic log for details.';
  }else if(ota.active){
    $('releaseStatus').textContent=`Installing firmware… ${ota.progress||0}%`;
  }
}
async function refreshStatus(){
  try{
    const s=await api('/api/v1/status');state.deviceStatus=s;
      remote_max_weight_g:maxDose,
    $('firmware').textContent=`${s.firmware.version} · build ${s.firmware.build}`;
    $('hardware').textContent=`${s.device.model} (${s.device.hardware_revision.toUpperCase()})`;
    $('deviceId').textContent=s.device.id;
    $('network').textContent=s.network.ssid||s.network.state;
    $('hostname').textContent=`${s.network.hostname}.local`;
    $('hostname').href=`http://${s.network.hostname}.local/`;
    $('address').textContent=s.network.ip||'Not connected';
    renderOtaStatus(s);
    if(!state.releaseChecked&&!s.ota.failed)checkForUpdate();
  }catch{$('network').textContent='Unavailable';}
}
refreshStatus();setInterval(refreshStatus,1500);

$('profileFields').innerHTML=['Single','Double','Custom'].map((name,i)=>
  `<div class="profile"><h3>${name}</h3><label class="field"><span>Weight (g)</span><input id="weight${i}" class="input num" type="number" min="5" max="1000" step="0.1"></label><label class="field" style="margin-top:9px"><span>Time (seconds)</span><input id="time${i}" class="input num" type="number" min="0.5" max="25" step="0.1"></label></div>`
).join('');
function setCheck(id,v){$(id).checked=!!v;}
function selectStyle(style){document.querySelectorAll('[data-style]').forEach(b=>b.classList.toggle('selected',b.dataset.style===style));state.screensaverStyle=style;}
function selectGrindLayout(layout){document.querySelectorAll('[data-layout]').forEach(b=>b.classList.toggle('selected',b.dataset.layout===layout));state.grindLayout=layout;}
document.querySelectorAll('[data-style]').forEach(b=>b.onclick=()=>{if(!b.disabled)selectStyle(b.dataset.style);});
document.querySelectorAll('[data-layout]').forEach(b=>b.onclick=()=>selectGrindLayout(b.dataset.layout));
document.querySelectorAll('.settingsnav button').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('.settingsnav button').forEach(x=>x.classList.remove('active'));
  b.classList.add('active');$(b.dataset.anchor).scrollIntoView({behavior:'smooth'});
});
async function loadCustomPreview(enabled){
  const p=$('customPreview');p.style.backgroundImage='';p.textContent=enabled?'Loading…':'Your image';
  if(!enabled)return;
  try{
    const r=await fetch('/api/v1/screensaver/image',{cache:'no-store'});if(!r.ok)throw new Error();
    const raw=new Uint8Array(await r.arrayBuffer());if(raw.length!==280*456*2)throw new Error();
    const c=$('screensaverCanvas'),x=c.getContext('2d'),img=x.createImageData(280,456);
    for(let i=0,j=0;i<raw.length;i+=2,j+=4){
      const v=raw[i]|(raw[i+1]<<8);
      img.data[j]=((v>>11)&31)*255/31;img.data[j+1]=((v>>5)&63)*255/63;img.data[j+2]=(v&31)*255/31;img.data[j+3]=255;
    }
    x.putImageData(img,0,0);p.textContent='';
    p.style.background=`center/cover no-repeat url(${c.toDataURL('image/jpeg',.72)})`;
  }catch{p.textContent='Custom image';}
}
async function settingsApi(url,options={},timeoutMs=15000){
  const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),timeoutMs);
  try{return await api(url,{...options,signal:controller.signal});}
  catch(e){if(e.name==='AbortError')throw new Error('Request timed out. Reload settings to check.');throw e;}
  finally{clearTimeout(timer);}
}
async function loadSettings({throwOnError=false}={}){
  try{
    const s=await settingsApi('/api/v1/settings');state.settings=s;renderDosePicker(s);
    $('currentProfile').value=s.current_profile;$('grindMode').value=s.grind_mode;
    s.profiles.forEach((p,i)=>{$(`weight${i}`).value=p.weight;$(`time${i}`).value=p.time;});
    setCheck('autoStart',s.automatic.start);$('autoStartThreshold').value=s.automatic.threshold_g??50;
    setCheck('autoReturn',s.automatic.return);setCheck('swipeEnabled',s.swipe_enabled);setCheck('loggingEnabled',s.logging_enabled);
    $('purgeMode').value=s.purge.mode;$('purgeAmount').value=s.purge.amount_g;$('freshness').value=s.purge.freshness_hours;
    $('motorLatency').value=s.motor_latency_ms??50;$('coastRatio').value=s.coast_ratio;
    $('brightness').value=s.display.brightness;$('screensaverBrightness').value=s.display.screensaver_brightness;
    $('brightnessOut').value=`${s.display.brightness}%`;$('screensaverBrightnessOut').value=`${s.display.screensaver_brightness}%`;
    setCheck('screensaverStartup',s.display.screensaver_startup);setCheck('screensaverSleep',s.display.screensaver_sleep);
    setCheck('displayOffEnabled',s.display.display_off_enabled);
    $('idleTimeout').value=s.display.screensaver_idle_timeout_s;$('displayOffDelay').value=s.display.display_off_delay_s;
    $('displayOffDelay').disabled=!s.display.display_off_enabled;$('startupTimeout').value=s.display.screensaver_startup_timeout_s;
    $('gaggimateHost').value=s.display.gaggimate_host||'gaggimate.local';
    setCheck('bluetoothStartup',s.bluetooth_startup);
    $('deleteScreensaver').disabled=!s.display.has_custom_screensaver;
    document.querySelector('[data-style=custom]').disabled=!s.display.has_custom_screensaver;
    selectStyle(s.display.screensaver_style);
    selectGrindLayout(s.display.grind_layout||'arc');
    loadCustomPreview(s.display.has_custom_screensaver);
  }catch(e){if(throwOnError)throw e;toast(e.message,true);}
}
$('brightness').oninput=e=>$('brightnessOut').value=`${e.target.value}%`;
$('screensaverBrightness').oninput=e=>$('screensaverBrightnessOut').value=`${e.target.value}%`;
$('displayOffEnabled').onchange=e=>$('displayOffDelay').disabled=!e.target.checked;

function settingsBody(){
  const q=new URLSearchParams({
    current_profile:$('currentProfile').value,grind_mode:$('grindMode').value==='time'?'1':'0',
    auto_start:$('autoStart').checked?'1':'0',auto_start_threshold_g:$('autoStartThreshold').value,
    auto_return:$('autoReturn').checked?'1':'0',swipe_enabled:$('swipeEnabled').checked?'1':'0',
    logging_enabled:$('loggingEnabled').checked?'1':'0',purge_mode:$('purgeMode').value,
    purge_amount_g:$('purgeAmount').value,freshness_hours:$('freshness').value,
    motor_latency_ms:$('motorLatency').value,coast_ratio:$('coastRatio').value,
    brightness_percent:$('brightness').value,screensaver_brightness_percent:$('screensaverBrightness').value,
    screensaver_startup:$('screensaverStartup').checked?'1':'0',screensaver_sleep:$('screensaverSleep').checked?'1':'0',
    screensaver_idle_timeout_s:$('idleTimeout').value,screensaver_startup_timeout_s:$('startupTimeout').value,
    display_off_enabled:$('displayOffEnabled').checked?'1':'0',display_off_delay_s:$('displayOffDelay').value,
    screensaver_style:state.screensaverStyle||'minimal',gaggimate_host:$('gaggimateHost').value.trim(),
    grind_layout:state.grindLayout||'arc',bluetooth_startup:$('bluetoothStartup').checked?'1':'0'
  });
  for(let i=0;i<3;i++){q.set(`weight${i}`,$(`weight${i}`).value);q.set(`time${i}`,$(`time${i}`).value);}
  return q;
}
let settingsSavePending=false;
async function waitForSettingsSave(id,timeoutMs=15000){
  if(!Number.isInteger(id)||id<=0)throw new Error('Could not confirm save.');
  const deadline=performance.now()+timeoutMs;
  while(performance.now()<deadline){
    const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),Math.max(1,deadline-performance.now()));
    let result;
    try{result=await api('/api/v1/settings/result?id='+id,{signal:controller.signal});}
    catch(e){if(e.name==='AbortError')break;throw new Error('Could not confirm save. Reload settings to check.');}
    finally{clearTimeout(timer);}
    if(result.request_id!==id)throw new Error('Save result mismatch.');
    if(['saved','failed','busy'].includes(result.status))return result.status;
    await new Promise(r=>setTimeout(r,250));
  }
  throw new Error('Save is still unconfirmed. Reload settings before retrying.');
}
$('settingsForm').onsubmit=async e=>{
  e.preventDefault();if(settingsSavePending)return;
  settingsSavePending=true;$('saveSettings').disabled=true;$('reloadSettings').disabled=true;
  $('settingsForm').inert=true;$('settingsPending').textContent='Saving on the grinder…';
  $('saveHint').textContent='Saving on the grinder…';
  let confirmed=false;
  try{
    const accepted=await settingsApi('/api/v1/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:settingsBody()});
    const result=await waitForSettingsSave(accepted.request_id);
    if(result==='busy')throw new Error('Grinder is busy; try again when idle.');
    if(result==='failed'){await loadSettings();throw new Error('Some settings could not be saved.');}
    confirmed=true;await loadSettings({throwOnError:true});
    $('saveHint').textContent='Saved on the grinder.';toast('Settings saved');
  }catch(err){
    const m=confirmed?'Saved, but could not reload. Please reload to check.':err.message;
    $('saveHint').textContent=m;toast(m,true);
  }finally{
    settingsSavePending=false;$('saveSettings').disabled=false;$('reloadSettings').disabled=false;
    $('settingsForm').inert=false;$('settingsPending').textContent='';
  }
};
$('reloadSettings').onclick=()=>loadSettings();
$('chooseScreensaver').onclick=()=>$('screensaverFile').click();
$('screensaverFile').onchange=async e=>{
  const file=e.target.files[0];if(!file)return;
  $('screensaverMessage').textContent='Preparing image…';
  try{
    const bitmap=await createImageBitmap(file),c=$('screensaverCanvas'),x=c.getContext('2d');
    const scale=Math.max(280/bitmap.width,456/bitmap.height),w=bitmap.width*scale,h=bitmap.height*scale;
    x.fillStyle='#000';x.fillRect(0,0,280,456);x.drawImage(bitmap,(280-w)/2,(456-h)/2,w,h);
    const form=new FormData();
    form.append('image',new Blob([canvasToRgb565(c)],{type:'application/octet-stream'}),'screensaver.rgb565');
    $('screensaverMessage').textContent='Uploading 249 KB…';
    await api('/api/v1/screensaver/image',{method:'POST',body:form});
    await new Promise(r=>setTimeout(r,350));await loadSettings();selectStyle('custom');
    $('screensaverMessage').textContent='Custom screensaver uploaded. Save settings to activate.';
    toast('Custom screensaver uploaded');
  }catch(err){$('screensaverMessage').textContent=err.message;toast(err.message,true);}
};
$('deleteScreensaver').onclick=async()=>{
  if(!confirm('Remove custom screensaver image?'))return;
  try{await api('/api/v1/screensaver/image',{method:'DELETE'});await new Promise(r=>setTimeout(r,350));await loadSettings();toast('Custom screensaver removed');}
  catch(e){toast(e.message,true);}
};

/* --- History --- */
function historyDate(s){return s.timestamp>1609459200?new Date(s.timestamp*1000).toLocaleString():`Session ${s.id}`;}
function signed(n,d=2){return `${n>0?'+':''}${n.toFixed(d)}`;}
async function loadHistory(){
  const list=$('historyList');list.innerHTML='<div class="empty">Loading history…</div>';
  try{
    const h=await api('/api/v1/history');state.history=h.sessions||[];
    $('historyCount').textContent=state.history.length;
    const errors=state.history.filter(s=>s.mode==='weight').map(s=>s.error_grams),abs=errors.map(Math.abs);
    $('averageError').textContent=abs.length?`${(abs.reduce((a,b)=>a+b,0)/abs.length).toFixed(2)} g`:'—';
    if(errors.length>1){
      const avg=errors.reduce((a,b)=>a+b,0)/errors.length;
      const sd=Math.sqrt(errors.reduce((a,b)=>a+(b-avg)**2,0)/errors.length);
      $('consistency').textContent=`±${sd.toFixed(2)} g`;
    }else $('consistency').textContent='—';
    renderHistoryList();
    if(state.history.length)openHistory(state.history[0].id);
  }catch(e){list.innerHTML=`<div class="empty">${esc(e.message)}</div>`;toast(e.message,true);}
}
function renderHistoryList(){
  const list=$('historyList');
  if(!state.history.length){list.innerHTML='<div class="empty">No saved grinds yet. Enable “Save grind history” in Settings.</div>';return;}
  list.innerHTML=state.history.map(s=>`<button class="shot ${state.selected?.session.id===s.id?'active':''}" data-id="${s.id}"><strong>${esc(historyDate(s))}</strong><span class="result">${esc(s.result||'Saved')}</span><small class="num">${s.mode==='time'?`${(s.target_time_ms/1000).toFixed(1)} s`:`${s.target_weight.toFixed(1)} g target`}</small><small class="num">${s.mode==='time'?`${signed(s.time_error_ms/1000,2)} s`:`${signed(-s.error_grams)} g`}</small></button>`).join('');
  list.querySelectorAll('.shot').forEach(b=>b.onclick=()=>openHistory(+b.dataset.id));
}
function parseSession(buffer){
  const v=new DataView(buffer),u=o=>v.getUint32(o,true),i=o=>v.getInt32(o,true),f=o=>v.getFloat32(o,true);
  const id=u(0),events=v.getUint16(16,true),measurements=v.getUint16(18,true),schema=v.getUint16(20,true),base=24;
  const session={id,timestamp:u(base+4),targetTime:u(base+8),duration:u(base+12),motorTime:u(base+16),timeError:i(base+20),target:f(base+24),final:f(base+32),error:f(base+36),profile:v.getUint8(base+56),mode:v.getUint8(base+57),pulses:v.getUint8(base+59),result:new TextDecoder().decode(new Uint8Array(buffer,base+64,16)).replace(/\0/g,'')};
  let off=base+80+events*44,rows=[];
  for(let n=0;n<measurements&&off+24<=buffer.byteLength;n++,off+=24){
    rows.push({time:u(off),weight:f(off+4),delta:f(off+8),flow:f(off+12),target:f(off+16),motor:v.getUint8(off+22),phase:v.getUint8(off+23)});
  }
  return {session,rows,events,schema,buffer};
}
async function openHistory(id){
  try{
    const r=await fetch(`/api/v1/history/session?id=${id}`,{cache:'no-store'});
    if(!r.ok)throw new Error(await r.text());
    state.selected=parseSession(await r.arrayBuffer());
    renderHistoryList();renderHistoryDetail();
  }catch(e){toast(e.message,true);}
}
function renderHistoryDetail(){
  const d=state.selected,s=d.session,el=$('historyDetail');
  el.innerHTML=`<div class="cardhead"><div><h2>${esc(historyDate(s))}</h2><span class="sub">${esc(s.result||'Saved grind')} · ${d.rows.length} samples</span></div><div class="actions" style="margin:0"><button class="btn secondary small" id="csvDownload">CSV</button><button class="btn secondary small" id="jsonDownload">JSON</button><a class="btn secondary small" href="/api/v1/history/session?id=${s.id}&download=1">Raw</a></div></div><div class="detailmeta"><div><span>Target</span><strong class="num">${s.mode?`${(s.targetTime/1000).toFixed(1)} s`:`${s.target.toFixed(1)} g`}</strong></div><div><span>Final</span><strong class="num">${s.final.toFixed(2)} g</strong></div><div><span>Error</span><strong class="num">${s.mode?`${signed(s.timeError/1000)} s`:`${signed(-s.error)} g`}</strong></div><div><span>Motor time</span><strong class="num">${(s.motorTime/1000).toFixed(2)} s</strong></div></div><div class="chartwrap"><canvas id="historyChart"></canvas></div><p class="sub">Weight (brass) and flow (sage). ${s.pulses} correction pulse${s.pulses===1?'':'s'}.</p>`;
  $('csvDownload').onclick=()=>downloadHistory('csv');
  $('jsonDownload').onclick=()=>downloadHistory('json');
  drawHistory(d);
}
function drawHistory(d){
  const css=getComputedStyle(document.documentElement);
  chart($('historyChart'),[{values:d.rows.map(r=>r.weight),color:css.getPropertyValue('--accent')},{values:d.rows.map(r=>r.flow),color:css.getPropertyValue('--sage')}],'recorded');
}
function downloadHistory(type){
  const d=state.selected;if(!d)return;
  let blob,name;
  if(type==='csv'){
    const text=['timestamp_ms,weight_g,flow_g_s,motor_on,phase',...d.rows.map(r=>`${r.time},${r.weight},${r.flow},${r.motor},${r.phase}`)].join('\n');
    blob=new Blob([text],{type:'text/csv'});name=`smartgrind-${d.session.id}.csv`;
  }else{
    blob=new Blob([JSON.stringify({session:d.session,measurements:d.rows},null,2)],{type:'application/json'});name=`smartgrind-${d.session.id}.json`;
  }
  const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download=name;a.click();
  setTimeout(()=>URL.revokeObjectURL(a.href),1000);
}
$('refreshHistory').onclick=loadHistory;
$('refreshLogs').onclick=async()=>{
  try{const log=await api('/api/v1/logs');$('diagnosticLog').textContent=log||'No diagnostic messages yet.';$('diagnosticLog').scrollTop=$('diagnosticLog').scrollHeight;}
  catch(e){toast(e.message,true);}
};

/* --- OTA & Releases --- */
function versionParts(value){const m=String(value||'').trim().match(/^v?(\d+)\.(\d+)\.(\d+)$/);return m?m.slice(1).map(Number):null;}
function compareVersions(a,b){const av=versionParts(a),bv=versionParts(b);if(!av||!bv)return null;for(let i=0;i<3;i++){if(av[i]!==bv[i])return av[i]>bv[i]?1:-1;}return 0;}
function showReleaseLink(release){const link=$('releaseNotes');if(release?.html_url){link.href=release.html_url;link.style.display='inline-flex';}else link.style.display='none';}
async function checkForUpdate(){
  if(!state.deviceStatus)return;
  state.releaseChecked=true;$('checkUpdate').disabled=true;$('installUpdate').style.display='none';
  $('releaseStatus').textContent='Checking latest GitHub release…';
  try{
    const response=await fetch('https://api.github.com/repos/Clinteastman/smart-grind-by-weight/releases/latest',{cache:'no-store',headers:{Accept:'application/vnd.github+json'}});
    if(!response.ok)throw new Error(`GitHub check failed (${response.status})`);
    const release=await response.json();
    if(release.draft||release.prerelease)throw new Error('No stable release found.');
    state.latestRelease=release;showReleaseLink(release);
    const rev=String(state.deviceStatus.device.hardware_revision||'').toLowerCase(),tag=release.tag_name;
    const expected=rev==='v2'?`smart-grind-by-weight-${tag}-waveshare-164-v2.bin`:rev==='v1'?`smart-grind-by-weight-${tag}.bin`:'';
    state.latestAsset=expected?release.assets.find(a=>a.name===expected):null;
    const cmp=compareVersions(state.deviceStatus.firmware.version,tag);
    if(cmp===null){$('releaseStatus').textContent='Could not compare firmware versions.';return;}
    if(cmp>0){$('releaseStatus').textContent=`Ahead of latest stable release (${tag}).`;return;}
    if(cmp===0){$('releaseStatus').textContent=`Up to date (${tag}).`;return;}
    if(!state.latestAsset){$('releaseStatus').textContent=`${tag} is available, missing ${rev.toUpperCase()} asset.`;return;}
    $('releaseStatus').textContent=`${tag} available for ${rev.toUpperCase()}.`;
    $('installUpdate').textContent=`Install ${tag}`;$('installUpdate').style.display='inline-flex';
  }catch(e){state.latestRelease=null;state.latestAsset=null;showReleaseLink(null);$('releaseStatus').textContent=`Update check: ${e.message}`;}
  finally{$('checkUpdate').disabled=false;}
}
$('checkUpdate').onclick=()=>{state.releaseChecked=false;checkForUpdate();};
async function installFirmwareBlob(blob,name,label=name){
  if(!name.toLowerCase().endsWith('.bin'))return toast('Choose an ESP32 .bin image',true);
  const magic=new Uint8Array(await blob.slice(0,1).arrayBuffer())[0];
  if(magic!==0xe9)return toast('Not a valid ESP32 firmware image',true);
  if(!confirm(`Install ${label}? Grinder will restart when complete.`))return false;
  $('otaButton').disabled=true;$('installUpdate').disabled=true;
  try{
    await api('/api/v1/ota/prepare',{method:'POST'});
    let ready=false;
    for(let n=0;n<40;n++){await new Promise(r=>setTimeout(r,250));const s=await api('/api/v1/status');if(s.ota.ready){ready=true;break;}}
    if(!ready)throw new Error('Could not free enough memory for OTA.');
    const form=new FormData();form.append('firmware',blob,name);
    await api('/api/v1/ota',{method:'POST',body:form});
    toast('Firmware accepted; grinder restarting');return true;
  }catch(e){toast(e.message,true);refreshStatus();return false;}
}
$('otaButton').onclick=async()=>{const f=$('firmwareFile').files[0];if(!f)return toast('Choose a .bin file first',true);await installFirmwareBlob(f,f.name);};
$('installUpdate').onclick=async()=>{
  if(otaRequestPending||githubInstall)return;
  const release=state.latestRelease,asset=state.latestAsset;
  if(!release||!asset)return checkForUpdate();
  if(!confirm(`Install ${release.tag_name}? Grinder will download and restart.`))return;
  otaRequestPending=true;$('installUpdate').disabled=true;$('otaButton').disabled=true;
  $('releaseStatus').textContent='Preparing grinder for update…';
  try{
    await api('/api/v1/ota/prepare',{method:'POST'});
    let ready=false;
    for(let n=0;n<60;n++){await new Promise(r=>setTimeout(r,250));const s=await api('/api/v1/status');if(s.ota.ready){ready=true;break;}}
    if(!ready)throw new Error('Could not free enough memory for OTA.');
    await api('/api/v1/ota/github',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({tag:release.tag_name})});
    githubInstall={tag:release.tag_name,startedAt:performance.now()};
    $('releaseStatus').textContent=`Installing ${release.tag_name}…`;
    toast('Update started on grinder');
  }catch(e){$('releaseStatus').textContent=`Update error: ${e.message}`;toast(e.message,true);}
  finally{otaRequestPending=false;refreshStatus();}
};
window.onresize=()=>{if(state.page==='dashboard')drawLive();if(state.page==='history'&&state.selected)drawHistory(state.selected);};
</script>
</body>
</html>
)WEB";
