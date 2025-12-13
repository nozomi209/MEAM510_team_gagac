/*
 * gagac-param.h - 参数调节页面（完整版）
 */
#pragma once
#include <Arduino.h>

const char paramPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1.0"/>
<title>GAGAC 参数面板</title>
<style>
:root{--bg:#f4f7f6;--text:#444;--card:#fff;--shadow:0 10px 25px rgba(0,0,0,0.1);}
*{box-sizing:border-box;}
body{margin:0;font-family:system-ui,-apple-system,sans-serif;background:var(--bg);color:var(--text);}
.wrap{max-width:1000px;margin:0 auto;padding:14px 16px;}
.topbar{display:flex;gap:12px;align-items:center;justify-content:space-between;margin-bottom:14px;}
h1{font-size:18px;margin:0;}
a{color:#3b82f6;}
.card{background:var(--card);border-radius:20px;padding:16px;box-shadow:var(--shadow);margin-bottom:14px;}
.card h2{margin:0 0 10px;font-size:1.05em;font-weight:800;}
.row{display:flex;gap:10px;flex-wrap:wrap;}
.btn{border:none;border-radius:12px;padding:10px 14px;font-size:1em;font-weight:700;cursor:pointer;color:#222;background:#ddd;}
.btn:active{transform:translateY(1px);}
.kbd{padding:2px 6px;border:1px solid #ddd;border-radius:8px;background:#fff;font-size:12px;font-family:monospace;}
.hint{color:#666;font-size:12px;line-height:1.5;}
details{border:1px solid #eee;border-radius:12px;background:#fafafa;padding:10px 10px 0;margin-top:10px;}
summary{cursor:pointer;list-style:none;font-weight:900;color:#444;font-size:0.95em;margin-bottom:10px;}
summary::-webkit-details-marker{display:none;}
.paramGrid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px;padding-bottom:10px;}
@media(max-width:720px){.paramGrid{grid-template-columns:1fr;}}
.param{border:1px solid #eee;border-radius:12px;padding:10px;background:#fff;}
.param .k{font-weight:900;font-size:12px;color:#333;}
.param .meta{font-size:11px;color:#777;margin-top:4px;min-height:14px;}
.param .ctrl{margin-top:8px;}
.param label{display:flex;justify-content:space-between;align-items:center;font-size:0.9em;color:#555;font-weight:700;}
input[type="range"]{width:100%;margin-top:8px;height:10px;border-radius:5px;cursor:pointer;}
.footer{margin-top:14px;color:#aaa;font-size:12px;text-align:center;}
</style>
</head>
<body>
<div class="wrap">
<div class="topbar">
<h1>参数面板</h1>
<a href="/">← 返回主控台</a>
</div>

<div class="card">
<h2>参数面板（实时发送 PARAM / MP_PARAM）</h2>
<div class="row">
<button class="btn" id="btnSendAll" style="background:#a4d7a7;">发送全部参数</button>
<button class="btn" id="btnResetDefaults" style="background:#f08080;">恢复默认值</button>
</div>
<div class="hint" style="margin-top:8px;">
- 参数滑块会自动实时发送（做了轻量节流）。<br/>
- 默认值按当前代码写死值设置；浏览器会用 localStorage 记住你上次调的值。
</div>

<details open>
<summary>P控制巡墙参数</summary>
<div class="paramGrid" id="secWall"></div>
</details>

<details open>
<summary>胡同逃脱（倒车+小半径转向）</summary>
<div class="paramGrid" id="secAlley"></div>
</details>

<details>
<summary>ToF 传感器标定</summary>
<div class="paramGrid" id="secToF"></div>
</details>

<details open>
<summary>手动规划（MP_PARAM）</summary>
<div class="paramGrid" id="secMp"></div>
</details>
</div>

<div class="footer">提示：参数调节后会实时发送到设备 | <a href="/">返回主控台</a></div>
</div>

<script>
function sendCommand(cmd){fetch('/cmd?data='+encodeURIComponent(cmd));}
function sendParam(key,val){sendCommand('PARAM:'+key+'='+val);}
function sendMpParam(key,val){sendCommand('MP_PARAM:'+key+'='+val);}

function fmt(v){const n=Number(v);if(Number.isNaN(n))return v;if(String(v).includes('.'))return n.toFixed(2);return String(n);}
function throttle(fn,ms){let last=0,t=null,pending=null;return(...args)=>{const now=Date.now();pending=args;const run=()=>{last=Date.now();t=null;fn(...pending);pending=null;};if(now-last>=ms)run();else if(!t)t=setTimeout(run,ms-(now-last));};}
function makeParamCard(p){const id='p_'+p.key,vid='v_'+p.key;return '<div class="param"><div class="k">'+p.key+'</div><div class="meta">'+(p.desc||'')+'</div><div class="ctrl"><label for="'+id+'"><span>'+(p.unit||'')+'</span><span class="kbd" id="'+vid+'">'+fmt(p.def)+'</span></label><input type="range" id="'+id+'" min="'+p.min+'" max="'+p.max+'" step="'+p.step+'" value="'+p.def+'"></div></div>';}

const WALL_PARAMS=[
{key:'WF_SPEED_FWD',min:0,max:100,step:1,def:50,desc:'巡墙速度'},
{key:'WALL_DIST_KP',min:0,max:1,step:0.01,def:0.10,desc:'距离Kp'},
{key:'WALL_ANGLE_KP',min:0,max:10,step:0.1,def:1.5,desc:'角度Kp'},
{key:'WALL_TARGET_DIST',min:50,max:800,step:5,def:200,unit:'mm',desc:'目标离墙距离'},
{key:'WF_MAX_TURN_RIGHT',min:0,max:100,step:1,def:70,desc:'最大右转力度'},
{key:'WF_MAX_TURN_LEFT',min:0,max:100,step:1,def:70,desc:'最大左转力度'},
{key:'WF_TURN_DEADBAND',min:0,max:50,step:1,def:14,desc:'直行死区'},
{key:'FRONT_OBS_DIST',min:50,max:1000,step:10,def:300,unit:'mm',desc:'前方障碍阈值'},
{key:'WALL_LOST_DIST',min:100,max:1500,step:10,def:650,unit:'mm',desc:'丢墙判定距离'},
{key:'FRONT_PANIC_DIST',min:20,max:300,step:5,def:60,unit:'mm',desc:'紧急倒车距离'},
{key:'TOF_SPACING_MM',min:50,max:300,step:1,def:143,unit:'mm',desc:'ToF间距'},
];

const ALLEY_PARAMS=[
{key:'ALLEY_FRONT_CLOSE',min:50,max:1000,step:10,def:250,unit:'mm',desc:'前方近距触发'},
{key:'ALLEY_RIGHT_CLOSE',min:50,max:1000,step:10,def:350,unit:'mm',desc:'右边近距触发'},
{key:'ALLEY_BACKUP_SPEED',min:0,max:100,step:1,def:40,desc:'倒车速度'},
{key:'ALLEY_BACKUP_TURN',min:0,max:100,step:1,def:30,desc:'倒车转向力度'},
{key:'ALLEY_BACKUP_MS',min:50,max:3000,step:50,def:600,unit:'ms',desc:'每次倒车时间'},
{key:'ALLEY_CHECK_MS',min:0,max:1000,step:25,def:100,unit:'ms',desc:'倒车后检测时间'},
{key:'ALLEY_FRONT_CLEAR',min:100,max:1500,step:10,def:400,unit:'mm',desc:'出胡同阈值'},
{key:'ALLEY_EXIT_TURN_STRENGTH',min:0,max:100,step:1,def:50,desc:'出胡同转向力度'},
{key:'ALLEY_EXIT_TURN_MS',min:50,max:2000,step:50,def:400,unit:'ms',desc:'转向时间'},
{key:'ALLEY_EXIT_FWD_SPEED',min:0,max:100,step:1,def:40,desc:'直行速度'},
{key:'ALLEY_EXIT_FWD_MS',min:50,max:2000,step:50,def:300,unit:'ms',desc:'直行稳定时间'},
];

const TOF_PARAMS=[
{key:'TOF_OFFSET_F',min:-200,max:200,step:1,def:0,unit:'mm',desc:'前ToF偏移'},
{key:'TOF_OFFSET_R1',min:-200,max:200,step:1,def:0,unit:'mm',desc:'右前ToF偏移'},
{key:'TOF_OFFSET_R2',min:-200,max:300,step:1,def:15,unit:'mm',desc:'右后ToF偏移'},
{key:'TOF_SCALE_F',min:0.8,max:1.3,step:0.01,def:1.03,desc:'前ToF比例系数'},
{key:'TOF_SCALE_R1',min:0.8,max:1.3,step:0.01,def:1.00,desc:'右前ToF比例系数'},
{key:'TOF_SCALE_R2',min:0.8,max:1.3,step:0.01,def:1.00,desc:'右后ToF比例系数'},
{key:'TOF_ALPHA',min:0,max:1,step:0.01,def:0.30,desc:'滤波系数'},
{key:'TOF_JUMP_MM',min:0,max:800,step:10,def:200,unit:'mm',desc:'跳变限幅'},
{key:'TOF_MIN_MM',min:0,max:50,step:1,def:2,unit:'mm',desc:'最小有效距离'},
{key:'TOF_MAX_MM',min:500,max:8000,step:100,def:5000,unit:'mm',desc:'最大有效距离'},
];

const MP_PARAMS=[
{key:'MP_DIST_TOL',min:20,max:150,step:5,def:50,desc:'到点距离阈值(mm)'},
{key:'MP_ANGLE_TOL',min:10,max:90,step:5,def:45,desc:'朝向角容差(deg)'},
{key:'MP_SPEED_FAR',min:20,max:100,step:5,def:60,desc:'远距离速度'},
{key:'MP_SPEED_NEAR',min:10,max:80,step:5,def:35,desc:'近距离减速'},
{key:'MP_TURN_RATE',min:30,max:150,step:5,def:65,desc:'原地转向力度'},
{key:'MP_BUMP_FWD_MS',min:100,max:1500,step:50,def:500,unit:'ms',desc:'撞击前冲时间'},
{key:'MP_BUMP_STOP_MS',min:50,max:1000,step:50,def:300,unit:'ms',desc:'撞后停顿时间'},
{key:'MP_DEBUG',min:0,max:1,step:1,def:0,desc:'调试输出'},
{key:'MP_MF_ENABLED',min:0,max:1,step:1,def:1,desc:'中频模式开关'},
{key:'MP_MF_EXEC_MS',min:200,max:800,step:25,def:400,unit:'ms',desc:'中频执行周期'},
{key:'MP_MF_STOP_MIN_MS',min:50,max:300,step:25,def:100,unit:'ms',desc:'中频停车时间'},
{key:'MP_MF_FIX_N',min:2,max:8,step:1,def:4,desc:'中频采样数'},
{key:'MP_MF_POS_STD_MAX',min:20,max:100,step:5,def:40,unit:'mm',desc:'中频位置稳定阈值'},
{key:'MP_MF_ANG_STD_MAX',min:5,max:30,step:1,def:15,unit:'deg',desc:'中频角度稳定阈值'},
{key:'MP_LF_ENABLED',min:0,max:1,step:1,def:0,desc:'低频模式开关'},
{key:'MP_LF_EXEC_MS',min:100,max:2500,step:50,def:900,unit:'ms',desc:'低频执行周期'},
{key:'MP_LF_STOP_MIN_MS',min:0,max:2500,step:50,def:250,unit:'ms',desc:'低频停车时间'},
{key:'MP_LF_FIX_N',min:1,max:20,step:1,def:8,desc:'低频采样数'},
{key:'MP_LF_POS_STD_MAX',min:0,max:200,step:1,def:30,unit:'mm',desc:'低频位置稳定阈值'},
{key:'MP_LF_ANG_STD_MAX',min:0,max:90,step:1,def:12,unit:'deg',desc:'低频角度稳定阈值'},
];

function renderSection(containerId,params,sender){
const el=document.getElementById(containerId);
el.innerHTML=params.map(makeParamCard).join('');
const throttled=throttle((k,v)=>sender(k,v),120);
for(const p of params){
const slider=document.getElementById('p_'+p.key);
const lab=document.getElementById('v_'+p.key);
const storeKey='gagac_'+p.key;
const saved=localStorage.getItem(storeKey);
if(saved!==null){slider.value=saved;lab.innerText=fmt(saved);}
else{lab.innerText=fmt(slider.value);}
slider.addEventListener('input',()=>{const v=slider.value;lab.innerText=fmt(v);localStorage.setItem(storeKey,v);throttled(p.key,v);});
}
}

renderSection('secWall',WALL_PARAMS,sendParam);
renderSection('secAlley',ALLEY_PARAMS,sendParam);
renderSection('secToF',TOF_PARAMS,sendParam);
renderSection('secMp',MP_PARAMS,sendMpParam);

const ALL_PARAMS=[...WALL_PARAMS,...ALLEY_PARAMS,...TOF_PARAMS];

document.getElementById('btnSendAll').onclick=()=>{
for(const p of ALL_PARAMS)sendParam(p.key,document.getElementById('p_'+p.key).value);
for(const p of MP_PARAMS)sendMpParam(p.key,document.getElementById('p_'+p.key).value);
alert('已发送全部参数');
};
document.getElementById('btnResetDefaults').onclick=()=>{
if(!confirm('确定要恢复默认值？'))return;
const all=[...ALL_PARAMS,...MP_PARAMS];
for(const p of all)localStorage.removeItem('gagac_'+p.key);
location.reload();
};
</script>
</body>
</html>
)rawliteral";
