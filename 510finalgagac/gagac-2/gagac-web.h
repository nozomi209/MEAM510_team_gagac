/*
 * gagac-web.h - Desktop Web UI resources (Servant)
 *
 * 目标：
 * - 面向电脑端操作（更简洁清晰）
 * - 保留键盘映射：↑↓←→ 控制；Q/W 调速度；A/S 调转向；松开方向键自动 S
 * - 支持实时调参：PARAM:... 与 MP_PARAM:...
 *
 * 说明：
 * - 网页只负责对 Servant 发 `/cmd?data=...`，Servant 在 `gagac-2.ino` 里执行/转发
 */

#pragma once
#include <Arduino.h>

const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>GAGAC 桌面控制台</title>
  <style>
    /* 按你“之前网页”的配色风格：浅色背景 + pastel 按钮 */
    :root{
      --bg:#f4f7f6;
      --text:#444;
      --muted:#666;
      --line:#eee;
      --card:#ffffff;
      --shadow: 0 10px 25px rgba(0, 0, 0, 0.10);
    }
    *{ box-sizing:border-box; }
    body{
      margin:0;
      font-family: "Poppins", ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, PingFang SC, "Microsoft YaHei", Arial;
      background: var(--bg);
      color: var(--text);
    }
    /* 横屏/电脑优先：全宽铺开 */
    .wrap{ max-width:none; margin:0 auto; padding:14px 16px 18px; }
    .topbar{ display:flex; gap:12px; align-items:center; justify-content:space-between; margin-bottom:14px; }
    .title{ display:flex; flex-direction:column; gap:2px; }
    .title h1{ font-size:18px; margin:0; letter-spacing:0.3px; }
    .title .sub{ font-size:12px; color:var(--muted); }
    .pill{
      padding:6px 10px;
      border:1px solid #ddd;
      border-radius:999px;
      font-size:12px;
      color:#555;
      background:#fff;
      white-space:nowrap;
      max-width: 48vw;
      overflow:hidden;
      text-overflow:ellipsis;
    }
    /* 三列横屏：左=手动控制，中=状态/快捷，右=参数（可滚动） */
    .grid{ display:grid; grid-template-columns: 380px minmax(340px, 1fr) 520px; gap:12px; align-items:start; }
    @media (max-width: 1100px){ .grid{ grid-template-columns: 380px 1fr; } }
    @media (max-width: 820px){ .grid{ grid-template-columns:1fr; } }
    .card{
      background: var(--card);
      border-radius: 20px;
      padding: 16px;
      box-shadow: var(--shadow);
    }
    .card h2{ margin:0 0 10px 0; font-size: 1.05em; color:#444; font-weight: 800; }
    .scrollY{ max-height: calc(100vh - 110px); overflow:auto; }
    .scrollY::-webkit-scrollbar{ width:10px; }
    .scrollY::-webkit-scrollbar-thumb{ background: rgba(0,0,0,0.12); border-radius:999px; }
    .scrollY::-webkit-scrollbar-track{ background: rgba(0,0,0,0.04); border-radius:999px; }
    .row{ display:flex; gap:10px; flex-wrap:wrap; }
    .btn{
      border:none;
      border-radius:12px;
      padding:12px 12px;
      font-size:1em;
      font-weight:800;
      cursor:pointer;
      transition: all 0.2s ease;
      color:#222;
      background:#ddd;
    }
    .btn:active{ transform: translateY(1px); }
    .btn.ghost{
      background:#d3d3d3;
      color:#555;
    }
    .kbd{
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace;
      padding:2px 6px;
      border:1px solid #ddd;
      border-bottom-width:2px;
      border-radius:8px;
      background:#fff;
      color:#333;
      font-size:12px;
    }
    .hint{ color:#666; font-size:12px; line-height:1.5; }
    .bigpad{ display:grid; grid-template-columns: repeat(3, 1fr); gap:10px; margin-top:10px; }
    .big{
      padding:18px 10px;
      border-radius:15px;
      border:none;
      color:white;
      font-size:1.05em;
      font-weight:900;
      cursor:pointer;
      transition: 0.2s;
    }
    /* 保留你原网页的底盘按钮配色 */
    #btnF{ background:#92C08E; }
    #btnB{ background:#f4a261; }
    #btnL{ background:#6baed6; }
    #btnR{ background:#6baed6; }
    #btnS{ background:#f08080; }
    .big:active{ transform: translateY(1px); }
    .slider{ margin-top:10px; padding-top:10px; border-top:1px solid #f0f0f0; }
    label{ display:flex; justify-content:space-between; align-items:center; font-size:0.9em; color:#555; font-weight:700; }
    input[type="range"]{
      width:100%;
      margin-top:8px;
      height:10px;
      border-radius:5px;
      outline:none;
      background: linear-gradient(to right, #92C08E 0%, #92C08E 50%, #ececec 50%, #ececec 100%);
      cursor:pointer;
    }
    .section{ margin-top:10px; border-top:1px solid #f0f0f0; padding-top:10px; }
    details{
      border:1px solid #eee;
      border-radius:12px;
      background:#fafafa;
      padding:10px 10px 0;
      margin-top:10px;
    }
    summary{
      cursor:pointer;
      list-style:none;
      font-weight:900;
      color:#444;
      font-size:0.95em;
      margin-bottom:10px;
    }
    summary::-webkit-details-marker{ display:none; }
    .paramGrid{ display:grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap:10px; padding-bottom:10px; }
    @media (max-width: 720px){ .paramGrid{ grid-template-columns:1fr; } }
    .param{
      border:1px solid #eee;
      border-radius:12px;
      padding:10px;
      background:#fff;
    }
    .param .k{ font-weight:900; font-size:12px; color:#333; }
    .param .meta{ font-size:11px; color:#777; margin-top:4px; min-height: 14px; }
    .param .ctrl{ margin-top:8px; }
    .footer{ margin-top:14px; color:#aaa; font-size:12px; text-align:center; }
    .kv{ display:grid; grid-template-columns: 1fr 1fr; gap:10px; }
    .kv .box{ border:1px solid #eee; border-radius:12px; padding:10px; background:#fff; }
    .kv .box .k{ font-size:11px; color:#777; }
    .kv .box .v{ margin-top:6px; font-size:14px; font-weight:900; color:#333; }
    input[type="text"]{
      width:100%;
      padding:10px 12px;
      border-radius:12px;
      border:1px solid var(--line);
      background:rgba(255,255,255,0.03);
      color:var(--text);
      outline:none;
    }
    input[type="text"]::placeholder{ color: rgba(147,164,191,0.75); }
    textarea{
      width:100%;
      min-height:78px;
      resize:vertical;
      padding:10px 12px;
      border-radius:12px;
      border:1px solid #ddd;
      background:#fff;
      color:#333;
      outline:none;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace;
      font-size:12px;
      line-height:1.4;
    }
    canvas{
      width:100%;
      border-radius:12px;
      border:1px solid #ddd;
      background:#fff;
      display:block;
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="topbar">
      <div class="title">
        <h1>GAGAC 桌面控制台</h1>
        <div class="sub">电脑键盘控制 + 参数实时调节（通过 <span class="kbd">/cmd?data=...</span>）</div>
      </div>
      <div class="pill" id="statusPill">就绪</div>
    </div>

    <div class="grid">
      <!-- 左：手动控制 -->
      <div class="card">
        <h2>手动控制（保留键盘映射）</h2>
        <div class="hint">
          - 方向控制：<span class="kbd">↑</span>/<span class="kbd">↓</span>/<span class="kbd">←</span>/<span class="kbd">→</span><br />
          - 调速度：<span class="kbd">Q</span> -5，<span class="kbd">W</span> +5（百分比）<br />
          - 调转向：<span class="kbd">A</span> -5，<span class="kbd">S</span> +5（百分比）<br />
          - 松开方向键自动发送 <span class="kbd">S</span>；也可按 <span class="kbd">Space</span> 急停
        </div>

        <div class="section">
          <div class="row">
            <!-- 保留原按钮配色（由 JS 设置 background） -->
            <button class="btn" id="btnAuto">Start Auto</button>
            <button class="btn" id="btnVive">Enable VIVE</button>
            <button class="btn" id="btnMp">Start Manual Plan</button>
          </div>
          <div class="hint" style="margin-top:8px;">手动操作会自动停止 Auto（避免抢控制）。</div>
        </div>

        <div class="slider">
          <label for="speedSlider">速度（发送 F/B 的百分比）<span><span id="speedVal">50</span>%</span></label>
          <input type="range" id="speedSlider" min="0" max="100" value="50" />
        </div>
        <div class="slider">
          <label for="turnSlider">转向力度（发送 L/R 的百分比）<span><span id="turnVal">30</span>%</span></label>
          <input type="range" id="turnSlider" min="0" max="100" value="30" />
        </div>

        <div class="bigpad">
          <div></div>
          <button class="big ok" id="btnF">前进 F</button>
          <div></div>
          <button class="big" id="btnL">差速左 L</button>
          <button class="big bad" id="btnS">停止 S</button>
          <button class="big" id="btnR">差速右 R</button>
          <div></div>
          <button class="big" id="btnB">后退 B</button>
          <div></div>
        </div>
        <div class="hint" style="margin:8px 0 4px;">原地转向（一轮前进一轮后退）</div>
        <div class="row" style="gap:10px;">
          <button class="big" id="btnPL" style="background:#ffd54f;">原地左转 PL</button>
          <button class="big" id="btnPR" style="background:#ffd54f;">原地右转 PR</button>
        </div>

        <div class="section hint">最近命令：<span class="kbd" id="lastCmd">-</span></div>

        <!-- 曼哈顿路径规划 Canvas -->
        <div class="section">
          <h2 style="margin-top:8px;">安全区域路径规划</h2>
          <div style="position:relative;background:#1a1a2e;border-radius:12px;padding:8px;">
            <canvas id="planCanvas" width="360" height="400" style="cursor:crosshair;display:block;"></canvas>
            <div style="position:absolute;top:12px;left:12px;background:rgba(0,0,0,0.7);padding:6px 10px;border-radius:8px;font-size:10px;color:#fff;">
              <div><span style="color:#4ade80;">■</span> 安全区域（可通行）</div>
              <div><span style="color:#ef4444;">■</span> 障碍区域（禁止）</div>
              <div><span style="color:#ff6b35;">●</span> 当前位置</div>
              <div><span style="color:#60a5fa;">●</span> 目标点</div>
              <div><span style="color:#fbbf24;">━</span> 规划路径</div>
            </div>
            <div id="coordDisplay" style="position:absolute;bottom:12px;right:12px;background:rgba(0,0,0,0.7);padding:4px 8px;border-radius:6px;font-size:11px;color:#fff;font-family:monospace;">
              X: - Y: -
            </div>
          </div>
          <div style="margin-top:10px;padding:10px;background:#f8f9fa;border-radius:10px;">
            <div style="display:grid;grid-template-columns:1fr 1fr;gap:8px;font-size:12px;">
              <div><b>当前位置：</b><span class="kbd" id="curPosDisplay">等待VIVE...</span></div>
              <div><b>目标点：</b><span class="kbd" id="planTargetDisplay">未设置</span></div>
              <div><b>距离：</b><span class="kbd" id="distDisplay">-</span></div>
              <div><b>状态：</b><span class="kbd" id="planStatusDisplay">就绪</span></div>
            </div>
          </div>
          <div class="row" style="margin-top:10px;gap:6px;flex-wrap:wrap;">
            <button class="btn" id="btnPlanExec" style="background:#22c55e;color:#fff;flex:1;">▶ 中频导航</button>
            <button class="btn" id="btnPlanExecLF" style="background:#3b82f6;color:#fff;flex:1;">▶ 低频导航</button>
            <button class="btn" id="btnPlanClear" style="background:#6b7280;color:#fff;">清除</button>
            <button class="btn" id="btnPlanStopNav" style="background:#ef4444;color:#fff;">■ 停止</button>
          </div>
          <div class="slider" style="margin-top:8px;">
            <label for="mfExecSlider">中频周期<span><span id="mfExecVal">400</span>ms</span></label>
            <input type="range" id="mfExecSlider" min="200" max="800" step="50" value="400" />
          </div>

          <!-- 导航日志 -->
          <div style="margin-top:12px;">
            <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;">
              <span style="font-weight:800;font-size:12px;">导航日志</span>
              <button class="btn" id="btnClearNavLog" style="background:#6b7280;color:#fff;padding:4px 10px;font-size:11px;">清空</button>
            </div>
            <div id="navLogArea" style="background:#1e1e1e;border-radius:10px;padding:8px;height:160px;overflow-y:auto;font-family:monospace;font-size:11px;color:#0f0;line-height:1.5;">
              <div style="color:#666;">[等待导航开始...]</div>
            </div>
          </div>
        </div>
      </div>

      <!-- 中：状态 & 快捷命令 -->
      <div class="card">
        <h2>状态 / 快捷命令</h2>
        <div class="kv">
          <div class="box">
            <div class="k">Auto</div>
            <div class="v" id="uiAutoState">OFF</div>
          </div>
          <div class="box">
            <div class="k">VIVE</div>
            <div class="v" id="uiViveState">OFF</div>
          </div>
          <div class="box">
            <div class="k">Manual Plan</div>
            <div class="v" id="uiMpState">OFF</div>
          </div>
          <div class="box">
            <div class="k">最近命令</div>
            <div class="v" style="font-size:12px; font-weight:700;"><span class="kbd" id="uiLastCmd">-</span></div>
          </div>
        </div>

        <div class="section">
          <div class="hint" style="margin-bottom:8px;">可直接输入一条命令（会走 <span class="kbd">/cmd?data=</span>）。例如：<span class="kbd">AUTO_ON</span> / <span class="kbd">PARAM:WALL_TARGET_DIST=200</span></div>
          <input id="cmdInput" type="text" placeholder='例如: PARAM:WF_TURN_DEADBAND=12' />
          <div class="row" style="margin-top:10px;">
                <button class="btn" id="btnSendCmd" style="background:#a4d7a7;">发送命令</button>
                <button class="btn" id="btnStopNow" style="background:#f08080;">急停 S</button>
          </div>
        </div>

        <div class="section hint">
          提示：你现在的底盘接口是"直走 Fxx / 原地转 Lxx/Rxx"，所以想更直可以把 <span class="kbd">WF_TURN_DEADBAND</span> 调大一点（例如 12~18）。
        </div>

        <!-- 调试模式面板 -->
        <details>
          <summary>🔧 调试模式（Debug Mode）</summary>
          <div class="row" style="margin-bottom:10px;">
            <button class="btn" id="btnDebugOn" style="background:#a5d6a7;">开启调试</button>
            <button class="btn" id="btnDebugOff" style="background:#ef9a9a;">关闭调试</button>
            <button class="btn ghost" id="btnDebugClear">清空日志</button>
          </div>
          <div class="kv" style="margin-bottom:10px;">
            <div class="box"><div class="k">状态</div><div class="v" id="debugStatus">OFF</div></div>
          </div>
          <div class="hint" style="margin-bottom:6px;">调试日志（自动刷新）：</div>
          <textarea id="debugLogArea" readonly style="width:100%;height:200px;font-size:11px;background:#1e1e1e;color:#0f0;font-family:monospace;"></textarea>
        </details>

        <!-- 之前网页的功能：都保留在这里 -->
        <details open>
          <summary>高级功能（旧版功能保留）</summary>

          <details open>
            <summary>VIVE 数据（/viveData）</summary>
            <div class="kv">
              <div class="box"><div class="k">X</div><div class="v" id="viveXVal">0</div></div>
              <div class="box"><div class="k">Y</div><div class="v" id="viveYVal">0</div></div>
              <div class="box"><div class="k">Angle</div><div class="v" id="viveAngleVal">0</div></div>
              <div class="box"><div class="k">Status</div><div class="v" id="viveStatusVal">-</div></div>
            </div>
            <div class="section">
              <div class="row">
                <button class="btn ghost" id="btnViveRefresh">刷新</button>
              </div>
              <div class="hint" style="margin-top:8px;">
                FrontRaw: (<span class="kbd" id="frontRawX">0</span>, <span class="kbd" id="frontRawY">0</span>) |
                BackRaw: (<span class="kbd" id="backRawX">0</span>, <span class="kbd" id="backRawY">0</span>)<br/>
                FrontFilt: (<span class="kbd" id="frontFiltX">0</span>, <span class="kbd" id="frontFiltY">0</span>) |
                BackFilt: (<span class="kbd" id="backFiltX">0</span>, <span class="kbd" id="backFiltY">0</span>)
              </div>
            </div>
          </details>

          <details open>
            <summary>ToF 实时数据（从 Owner 转发）</summary>
            <div class="kv">
              <div class="box"><div class="k">Front F (mm)</div><div class="v" id="tofFVal">-</div></div>
              <div class="box"><div class="k">RightFront R1 (mm)</div><div class="v" id="tofR1Val">-</div></div>
              <div class="box"><div class="k">RightBack R2 (mm)</div><div class="v" id="tofR2Val">-</div></div>
              <div class="box"><div class="k">Age</div><div class="v" id="tofAgeVal">-</div></div>
            </div>
            <div class="hint" style="margin-top:8px;">
              若 Age 很大：说明 Owner 没在发 TOF:...（检查 Owner 是否已烧录、UART 线是否正常）。
            </div>
          </details>

          <details open>
            <summary>巡墙策略/状态（从 Owner 转发）</summary>
            <div class="kv">
              <div class="box"><div class="k">策略</div><div class="v">P控制 + 状态机</div></div>
              <div class="box"><div class="k">Auto(Owner)</div><div class="v" id="wfAutoVal">-</div></div>
              <div class="box"><div class="k">状态</div><div class="v" id="wfStateVal">-</div></div>
              <div class="box"><div class="k">Last Cmd</div><div class="v" id="wfCmdVal">-</div></div>
              <div class="box"><div class="k">Turn</div><div class="v" id="wfTurnVal">-</div></div>
              <div class="box"><div class="k">Angle(deg)</div><div class="v" id="wfAngleVal">-</div></div>
              <div class="box"><div class="k">DistErr</div><div class="v" id="wfErrVal">-</div></div>
              <div class="box"><div class="k">Age</div><div class="v" id="wfAgeVal">-</div></div>
            </div>
          </details>

          <details>
            <summary>手动规划 / 路线 / 规划命令（PLAN* / MP_ROUTE / MP_ON）</summary>
            <div class="section">
              <div class="hint" style="margin-bottom:6px;">MP_ROUTE（格式：x,y,h,b;...）</div>
              <input id="routeInput" type="text" placeholder="例如: 4500,3200,90,1; 4700,3400,0,0" />
              <div class="row" style="margin-top:10px;">
              <button class="btn" id="btnSendRoute" style="background:#a4d7a7;">发送 MP_ROUTE</button>
              </div>
            </div>

            <div class="section">
              <div class="hint" style="margin-bottom:6px;">PLAN1 目标点（x,y）</div>
              <input id="planTarget" type="text" placeholder="例如: 4500,3200" />
              <div class="row" style="margin-top:10px;">
                <button class="btn" id="btnPlan1" style="background:#92C08E;">开始规划并执行（PLAN1）</button>
                <button class="btn" id="btnPlanStop" style="background:#f08080;">停止（PLAN_STOP）</button>
              </div>
            </div>

            <div class="section">
              <div class="hint" style="margin-bottom:6px;">障碍物（left,right,top,bottom,margin）</div>
              <input id="planObs" type="text" placeholder="例如: 4100,4945,4240,3130,150" />
              <div class="row" style="margin-top:10px;">
                <button class="btn" id="btnPlanObsDefault" style="background:#ffe8a1;">加载默认</button>
                <button class="btn" id="btnPlanObs" style="background:#ffd28e;">应用（PLAN_OBS）</button>
                <button class="btn" id="btnPlanObsOff" style="background:#d3d3d3;color:#555;">禁用（PLAN_OBS_OFF）</button>
              </div>
            </div>

            <div class="section">
              <div class="hint" style="margin-bottom:6px;">边界（xmin,xmax,ymax,ymin）</div>
              <input id="planBound" type="text" value="3920,5100,5700,1390" />
              <div class="row" style="margin-top:10px;">
                <button class="btn" id="btnPlanBound" style="background:#cde4ff;color:#444;">应用边界（PLAN_BOUND）</button>
                <button class="btn" id="btnPlanSetStart" style="background:#8fd3f4;color:#234;">锁定起点（PLAN_SET_START）</button>
                <button class="btn" id="btnPlanClearStart" style="background:#d3d3d3;color:#555;">清除起点（PLAN_CLEAR_START）</button>
              </div>
            </div>

            <div class="section">
              <div class="hint" style="margin-bottom:6px;">路径预览（简化示意，仅用于调参/核对坐标）</div>
              <canvas id="pathCanvas" width="520" height="320"></canvas>
              <div class="hint" style="margin-top:8px;"><span class="kbd" id="pathStatus">等待...</span></div>
            </div>
          </details>

          <details>
            <summary>SEQ 时间序列（SEQ / SEQ_START / SEQ_STOP）</summary>
            <div class="hint" style="margin-bottom:6px;">格式：MODE,VAL,DurationMs;... 例如：F,50,2000;S,0,200;L,80,600</div>
            <textarea id="seqInput">F,50,5600;S,0,100;L,100,1500</textarea>
            <!-- Sequence：要求保留之前的文案与配色 -->
            <div class="row" style="margin-top:10px;">
              <button class="btn" id="btnSendSeq" style="background:#a4d7a7;">Send Sequence</button>
              <button class="btn" id="btnSeqStart" style="background:#ffa94d;">Start Sequence</button>
              <button class="btn" id="btnSeqStop" style="background:#f08080;">Stop Sequence</button>
            </div>
          </details>

          <details>
            <summary>攻击伺服（SV1 / SV0）</summary>
            <div class="row">
              <button class="btn" id="btnAttackStart" style="background:#f7b5b5;">Start Attack（SV1）</button>
              <button class="btn" id="btnAttackStop" style="background:#90caf9;">Stop Attack（SV0）</button>
            </div>
          </details>
        </details>
      </div>

      <!-- 右：参数面板（固定高度可滚动） -->
      <div class="card scrollY">
        <h2>参数面板（实时发送 PARAM / MP_PARAM）</h2>
        <div class="row">
          <button class="btn ghost" id="btnSendAll">发送全部参数</button>
          <button class="btn bad" id="btnResetDefaults">恢复默认值</button>
        </div>
        <div class="hint" style="margin-top:8px;">
          - 参数滑块会自动实时发送（做了轻量节流）。<br />
          - 默认值按当前代码写死值设置；浏览器会用 localStorage 记住你上次调的值。<br />
        </div>

        <details open>
          <summary>P控制巡墙参数</summary>
          <div class="paramGrid" id="secWall"></div>
        </details>

        <details open>
          <summary>🔄 胡同逃脱（倒车+小半径转向）</summary>
          <div class="paramGrid" id="secAlley"></div>
        </details>

        <details>
          <summary>📡 ToF 传感器标定</summary>
          <div class="paramGrid" id="secToF"></div>
        </details>

        <details>
          <summary>手动规划（MP_PARAM）</summary>
          <div class="paramGrid" id="secMp"></div>
        </details>
      </div>
    </div>

    <div class="footer">提示：连接 Servant AP 后打开本页；常见地址 <span class="kbd">192.168.4.1</span></div>
  </div>

  <script>
    const statusPill = document.getElementById("statusPill");
    const lastCmd = document.getElementById("lastCmd");
    const uiLastCmd = document.getElementById("uiLastCmd");
    const uiAutoState = document.getElementById("uiAutoState");
    const uiViveState = document.getElementById("uiViveState");
    const uiMpState = document.getElementById("uiMpState");
    const cmdInput = document.getElementById("cmdInput");
    const btnSendCmd = document.getElementById("btnSendCmd");
    const btnStopNow = document.getElementById("btnStopNow");
    // Advanced / legacy controls
    const btnViveRefresh = document.getElementById("btnViveRefresh");
    const viveXVal = document.getElementById("viveXVal");
    const viveYVal = document.getElementById("viveYVal");
    const viveAngleVal = document.getElementById("viveAngleVal");
    const viveStatusVal = document.getElementById("viveStatusVal");
    const frontRawX = document.getElementById("frontRawX");
    const frontRawY = document.getElementById("frontRawY");
    const backRawX = document.getElementById("backRawX");
    const backRawY = document.getElementById("backRawY");
    const frontFiltX = document.getElementById("frontFiltX");
    const frontFiltY = document.getElementById("frontFiltY");
    const backFiltX = document.getElementById("backFiltX");
    const backFiltY = document.getElementById("backFiltY");

    // ToF live
    const tofFVal = document.getElementById("tofFVal");
    const tofR1Val = document.getElementById("tofR1Val");
    const tofR2Val = document.getElementById("tofR2Val");
    const tofAgeVal = document.getElementById("tofAgeVal");

    // Wall-follow live
    const wfAutoVal = document.getElementById("wfAutoVal");
    const wfStateVal = document.getElementById("wfStateVal");
    const wfCmdVal = document.getElementById("wfCmdVal");
    const wfTurnVal = document.getElementById("wfTurnVal");
    const wfAngleVal = document.getElementById("wfAngleVal");
    const wfErrVal = document.getElementById("wfErrVal");
    const wfAgeVal = document.getElementById("wfAgeVal");

    const routeInput = document.getElementById("routeInput");
    const btnSendRoute = document.getElementById("btnSendRoute");
    const planTarget = document.getElementById("planTarget");
    const btnPlan1 = document.getElementById("btnPlan1");
    const btnPlanStop = document.getElementById("btnPlanStop");
    const planObs = document.getElementById("planObs");
    const btnPlanObs = document.getElementById("btnPlanObs");
    const btnPlanObsOff = document.getElementById("btnPlanObsOff");
    const btnPlanObsDefault = document.getElementById("btnPlanObsDefault");
    const planBound = document.getElementById("planBound");
    const btnPlanBound = document.getElementById("btnPlanBound");
    const btnPlanSetStart = document.getElementById("btnPlanSetStart");
    const btnPlanClearStart = document.getElementById("btnPlanClearStart");
    const pathCanvas = document.getElementById("pathCanvas");
    const pathStatus = document.getElementById("pathStatus");

    const seqInput = document.getElementById("seqInput");
    const btnSendSeq = document.getElementById("btnSendSeq");
    const btnSeqStart = document.getElementById("btnSeqStart");
    const btnSeqStop = document.getElementById("btnSeqStop");

    const btnAttackStart = document.getElementById("btnAttackStart");
    const btnAttackStop = document.getElementById("btnAttackStop");
    const speedSlider = document.getElementById("speedSlider");
    const turnSlider = document.getElementById("turnSlider");
    const speedVal = document.getElementById("speedVal");
    const turnVal = document.getElementById("turnVal");
    const btnAuto = document.getElementById("btnAuto");
    const btnVive = document.getElementById("btnVive");
    const btnMp = document.getElementById("btnMp");
    const buttons = {
      F: document.getElementById("btnF"),
      B: document.getElementById("btnB"),
      L: document.getElementById("btnL"),
      R: document.getElementById("btnR"),
      S: document.getElementById("btnS"),
      PL: document.getElementById("btnPL"),  // 原地左转
      PR: document.getElementById("btnPR"),  // 原地右转
    };

    let autoMode = false;
    let viveEnabled = false;
    let mpEnabled = false;

    function setStatus(text) { statusPill.innerText = text; }
    function sendCommand(cmd) {
      lastCmd.innerText = cmd;
      uiLastCmd.innerText = cmd;
      setStatus("发送: " + cmd);
      fetch("/cmd?data=" + encodeURIComponent(cmd))
        .then(() => setStatus("就绪"))
        .catch(() => setStatus("网络错误（检查连接/AP）"));
    }
    function sendParam(key, val) { sendCommand("PARAM:" + key + "=" + val); }
    function sendMpParam(key, val) { sendCommand("MP_PARAM:" + key + "=" + val); }

    // --- VIVE data ---
    const vivePose = { x:0, y:0, angle:0, statusFront:0, statusBack:0 };
    function updateViveData() {
      fetch("/viveData")
        .then(r => r.json())
        .then(data => {
          vivePose.x = parseFloat(data.x) || 0;
          vivePose.y = parseFloat(data.y) || 0;
          vivePose.angle = parseFloat(data.angle) || 0;
          vivePose.statusFront = (data.status && typeof data.status.front !== "undefined") ? data.status.front : 0;
          vivePose.statusBack  = (data.status && typeof data.status.back  !== "undefined") ? data.status.back  : 0;
          viveXVal.innerText = vivePose.x.toFixed(1);
          viveYVal.innerText = vivePose.y.toFixed(1);
          viveAngleVal.innerText = vivePose.angle.toFixed(1);
          viveStatusVal.innerText = `${vivePose.statusFront}/${vivePose.statusBack}`;

          if (data.frontRaw) { frontRawX.innerText = data.frontRaw.x ?? 0; frontRawY.innerText = data.frontRaw.y ?? 0; }
          if (data.backRaw)  { backRawX.innerText  = data.backRaw.x ?? 0;  backRawY.innerText  = data.backRaw.y ?? 0; }
          if (data.frontFiltered) { frontFiltX.innerText = data.frontFiltered.x ?? 0; frontFiltY.innerText = data.frontFiltered.y ?? 0; }
          if (data.backFiltered)  { backFiltX.innerText  = data.backFiltered.x ?? 0;  backFiltY.innerText  = data.backFiltered.y ?? 0; }
        })
        .catch(() => {});
    }
    if (btnViveRefresh) btnViveRefresh.onclick = () => updateViveData();
    setInterval(updateViveData, 1000);

    // --- ToF data ---
    function updateToFData() {
      fetch("/tofData")
        .then(r => r.json())
        .then(d => {
          tofFVal.innerText = (typeof d.f !== "undefined") ? d.f : "-";
          tofR1Val.innerText = (typeof d.r1 !== "undefined") ? d.r1 : "-";
          tofR2Val.innerText = (typeof d.r2 !== "undefined") ? d.r2 : "-";
          const age = (typeof d.age_ms !== "undefined") ? d.age_ms : 999999;
          tofAgeVal.innerText = age + " ms";
          // 简单变色：<500ms 绿色，<2000ms 橙色，否则红色
          const c = (age < 500) ? "#22c55e" : (age < 2000 ? "#f59e0b" : "#ef4444");
          tofAgeVal.style.color = c;
        })
        .catch(() => {});
    }
    setInterval(updateToFData, 200); // 原 80ms → 200ms，减少 WiFi 负载

    // --- Wall-follow status ---
    const WF_STATE_NAMES = [
      "正常巡墙",        // WF_NORMAL
      "前方障碍",        // WF_OBSTACLE_FRONT
      "紧急倒车",        // WF_PANIC_BACKUP
      "丢墙",            // WF_LOST_WALL
      "拐角",            // WF_CORNER
      "出拐角",          // WF_EXITING
      "胡同倒车",        // WF_ALLEY_BACKUP
      "胡同检测",        // WF_ALLEY_CHECK
      "胡同小半径右转",   // WF_ALLEY_EXIT_TURN
      "胡同直行稳定",     // WF_ALLEY_EXIT_FWD
      "找墙右转",        // WF_SEEK_TURN
      "找墙前进",        // WF_SEEK_FWD
    ];
    function updateWfData() {
      fetch("/wfData")
        .then(r => r.json())
        .then(d => {
          const auto = (typeof d.auto !== "undefined") ? d.auto : 0;
          const st = (typeof d.state !== "undefined") ? d.state : -1;
          wfAutoVal.innerText = auto ? "ON" : "OFF";
          wfAutoVal.style.color = auto ? "#22c55e" : "";
          wfStateVal.innerText = (st >= 0 && st < WF_STATE_NAMES.length) ? WF_STATE_NAMES[st] : ("#" + st);
          wfCmdVal.innerText = (typeof d.cmd !== "undefined") ? d.cmd : "-";
          wfTurnVal.innerText = (typeof d.turn !== "undefined") ? d.turn : "-";
          wfAngleVal.innerText = (typeof d.angle !== "undefined") ? d.angle : "-";
          wfErrVal.innerText = (typeof d.err !== "undefined") ? d.err : "-";
          const age = (typeof d.age_ms !== "undefined") ? d.age_ms : 999999;
          wfAgeVal.innerText = age + " ms";
          const c = (age < 500) ? "#22c55e" : (age < 2000 ? "#f59e0b" : "#ef4444");
          wfAgeVal.style.color = c;
        })
        .catch(() => {});
    }
    setInterval(updateWfData, 200); // 原 80ms → 200ms，减少 WiFi 负载

    // --- Debug Mode ---
    const debugStatus = document.getElementById("debugStatus");
    const debugLogArea = document.getElementById("debugLogArea");
    const btnDebugOn = document.getElementById("btnDebugOn");
    const btnDebugOff = document.getElementById("btnDebugOff");
    const btnDebugClear = document.getElementById("btnDebugClear");
    let debugEnabled = false;

    function updateDebugLog() {
      if (!debugEnabled) return;
      fetch("/debugLog")
        .then(r => r.json())
        .then(d => {
          debugStatus.innerText = d.enabled ? "ON" : "OFF";
          debugStatus.style.color = d.enabled ? "#22c55e" : "#ef4444";
          // 解码转义字符
          let log = d.log || "";
          log = log.replace(/\\n/g, "\n").replace(/\\"/g, '"');
          debugLogArea.value = log;
          // 自动滚动到底部
          debugLogArea.scrollTop = debugLogArea.scrollHeight;
        })
        .catch(() => {});
    }

    btnDebugOn.onclick = () => {
      fetch("/debugOn").then(() => {
        debugEnabled = true;
        debugStatus.innerText = "ON";
        debugStatus.style.color = "#22c55e";
        setStatus("调试模式已开启");
      });
    };
    btnDebugOff.onclick = () => {
      fetch("/debugOff").then(() => {
        debugEnabled = false;
        debugStatus.innerText = "OFF";
        debugStatus.style.color = "#ef4444";
        setStatus("调试模式已关闭");
      });
    };
    btnDebugClear.onclick = () => {
      fetch("/debugClear").then(() => {
        debugLogArea.value = "";
        setStatus("日志已清空");
      });
    };

    // 调试日志每 500ms 刷新一次
    setInterval(updateDebugLog, 500);

    // --- Auto/VIVE/MP toggles ---
    // 初始化按钮配色（保持“之前网页”的颜色）
    btnAuto.style.background = "#F3CD35";
    btnVive.style.background = "#E79DC3";
    btnMp.style.background = "#9fc5e8";

    btnAuto.onclick = () => {
      autoMode = !autoMode;
      if (autoMode) {
        btnAuto.innerText = "STOP Auto";
        btnAuto.style.background = "#ef9a9a";
        uiAutoState.innerText = "ON";
        uiAutoState.style.color = "#22c55e";
        sendCommand("AUTO_ON");
      } else {
        btnAuto.innerText = "Start Auto";
        btnAuto.style.background = "#F3CD35";
        uiAutoState.innerText = "OFF";
        uiAutoState.style.color = "";
        sendCommand("AUTO_OFF");
      }
    };
    btnVive.onclick = () => {
      viveEnabled = !viveEnabled;
      btnVive.innerText = viveEnabled ? "Disable VIVE" : "Enable VIVE";
      btnVive.style.background = viveEnabled ? "#ce93d8" : "#E79DC3";
      uiViveState.innerText = viveEnabled ? "ON" : "OFF";
      uiViveState.style.color = viveEnabled ? "#22c55e" : "";
      sendCommand(viveEnabled ? "VIVE_ON" : "VIVE_OFF");
    };
    btnMp.onclick = () => {
      mpEnabled = !mpEnabled;
      btnMp.innerText = mpEnabled ? "Stop Manual Plan" : "Start Manual Plan";
      btnMp.style.background = mpEnabled ? "#f4a261" : "#9fc5e8";
      uiMpState.innerText = mpEnabled ? "ON" : "OFF";
      uiMpState.style.color = mpEnabled ? "#22c55e" : "";
      sendCommand(mpEnabled ? "MP_ON" : "MP_OFF");
    };

    function checkManualOverride() { if (autoMode) btnAuto.click(); }

    // --- Sliders (manual) ---
    speedSlider.oninput = function() { speedVal.innerText = this.value; };
    turnSlider.oninput = function() { turnVal.innerText = this.value; };

    // --- Manual state ---
    let isMoving = false;
    let currentMoveDirection = null;
    let isTurning = false;
    let currentTurnDirection = null;

    function stopNow() {
      isMoving = false;
      isTurning = false;
      currentMoveDirection = null;
      currentTurnDirection = null;
      sendCommand("S");
    }

    function bindHold(btn, onPress) {
      btn.onmousedown = () => { checkManualOverride(); onPress(); };
      btn.onmouseup = stopNow;
      btn.onmouseleave = stopNow;
      btn.ontouchstart = (e) => { e.preventDefault(); checkManualOverride(); onPress(); };
      btn.ontouchend = (e) => { e.preventDefault(); stopNow(); };
    }
    bindHold(buttons.F, () => { isMoving = true; currentMoveDirection = "F"; isTurning=false; sendCommand("F" + speedSlider.value); });
    bindHold(buttons.B, () => { isMoving = true; currentMoveDirection = "B"; isTurning=false; sendCommand("B" + speedSlider.value); });
    bindHold(buttons.L, () => { isTurning = true; currentTurnDirection = "L"; isMoving=false; sendCommand("L" + turnSlider.value); });
    bindHold(buttons.R, () => { isTurning = true; currentTurnDirection = "R"; isMoving=false; sendCommand("R" + turnSlider.value); });
    // 原地转向（Pivot）
    bindHold(buttons.PL, () => { isTurning = true; currentTurnDirection = "PL"; isMoving=false; sendCommand("PL" + turnSlider.value); });
    bindHold(buttons.PR, () => { isTurning = true; currentTurnDirection = "PR"; isMoving=false; sendCommand("PR" + turnSlider.value); });
    buttons.S.onclick = () => { checkManualOverride(); stopNow(); };

    // Keyboard mapping (保持：↑↓←→ 控车；Q/W 调速度；A/S 调转向)
    function adjustSpeed(delta) {
      let v = parseInt(speedSlider.value) + delta;
      v = Math.max(0, Math.min(100, v));
      speedSlider.value = v;
      speedVal.innerText = v;
      if (isMoving && currentMoveDirection) sendCommand(currentMoveDirection + v);
    }
    function adjustTurn(delta) {
      let v = parseInt(turnSlider.value) + delta;
      v = Math.max(0, Math.min(100, v));
      turnSlider.value = v;
      turnVal.innerText = v;
      if (isTurning && currentTurnDirection) sendCommand(currentTurnDirection + v);
    }

    document.addEventListener("keydown", (e) => {
      if (e.repeat) return;
      // 如果正在输入框/文本框里打字，不抢键盘控制（避免误触发小车）
      const tag = (e.target && e.target.tagName) ? e.target.tagName.toUpperCase() : "";
      if (tag === "INPUT" || tag === "TEXTAREA") return;
      switch (e.key) {
        case "ArrowUp":
          checkManualOverride();
          isMoving = true; currentMoveDirection = "F";
          isTurning = false; currentTurnDirection = null;
          sendCommand("F" + speedSlider.value);
          break;
        case "ArrowDown":
          checkManualOverride();
          isMoving = true; currentMoveDirection = "B";
          isTurning = false; currentTurnDirection = null;
          sendCommand("B" + speedSlider.value);
          break;
        case "ArrowLeft":
          checkManualOverride();
          isTurning = true; currentTurnDirection = "L";
          isMoving = false; currentMoveDirection = null;
          sendCommand("L" + turnSlider.value);
          break;
        case "ArrowRight":
          checkManualOverride();
          isTurning = true; currentTurnDirection = "R";
          isMoving = false; currentMoveDirection = null;
          sendCommand("R" + turnSlider.value);
          break;
        case " ":
          checkManualOverride();
          stopNow();
          break;
        case "q":
        case "Q":
          adjustSpeed(-5);
          break;
        case "w":
        case "W":
          adjustSpeed(+5);
          break;
        case "a":
        case "A":
          adjustTurn(-5);
          break;
        case "s":
        case "S":
          adjustTurn(+5);
          break;
      }
    });
    document.addEventListener("keyup", (e) => {
      const tag = (e.target && e.target.tagName) ? e.target.tagName.toUpperCase() : "";
      if (tag === "INPUT" || tag === "TEXTAREA") return;
      if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"].includes(e.key)) stopNow();
    });
    window.addEventListener("blur", () => stopNow());

    // Quick command box
    btnSendCmd.onclick = () => {
      const v = (cmdInput.value || "").trim();
      if (v.length === 0) return;
      // 如果用户只输入了 "S"/"AUTO_ON" 等，直接发送
      sendCommand(v);
    };
    btnStopNow.onclick = () => { checkManualOverride(); stopNow(); };
    cmdInput.addEventListener("keydown", (e) => {
      if (e.key === "Enter") {
        e.preventDefault();
        btnSendCmd.click();
      }
    });

    // --- Legacy command buttons wiring ---
    if (btnSendRoute) btnSendRoute.onclick = () => {
      const v = (routeInput.value || "").trim();
      if (v.length) sendCommand("MP_ROUTE:" + v);
    };
    if (btnPlan1) btnPlan1.onclick = () => {
      const t = (planTarget.value || "").trim();
      if (!t.includes(",")) { pathStatus.innerText = "目标格式错误"; return; }
      sendCommand("PLAN1:" + t);
      pathStatus.innerText = "已发送 PLAN1";
      setTimeout(drawPreview, 300);
    };
    if (btnPlanStop) btnPlanStop.onclick = () => { sendCommand("PLAN_STOP"); pathStatus.innerText = "已停止"; };
    if (btnPlanObsDefault) btnPlanObsDefault.onclick = () => {
      planObs.value = "4072,4950,4257,3120,150";
    };
    if (btnPlanObs) btnPlanObs.onclick = () => {
      const o = (planObs.value || "").trim();
      if (!o.includes(",")) { pathStatus.innerText = "障碍格式错误"; return; }
      sendCommand("PLAN_OBS:" + o);
      pathStatus.innerText = "已更新障碍";
      setTimeout(drawPreview, 300);
    };
    if (btnPlanObsOff) btnPlanObsOff.onclick = () => { sendCommand("PLAN_OBS_OFF"); pathStatus.innerText = "障碍已禁用"; setTimeout(drawPreview, 300); };
    if (btnPlanBound) btnPlanBound.onclick = () => {
      const b = (planBound.value || "").trim();
      if (b.split(",").length !== 4) { pathStatus.innerText = "边界格式错误"; return; }
      sendCommand("PLAN_BOUND:" + b);
      pathStatus.innerText = "边界已更新";
      setTimeout(drawPreview, 300);
    };
    if (btnPlanSetStart) btnPlanSetStart.onclick = () => {
      // 使用当前 VIVE 坐标作为起点锁定
      sendCommand(`PLAN_SET_START:${vivePose.x},${vivePose.y}`);
      pathStatus.innerText = "起点已锁定";
      setTimeout(drawPreview, 300);
    };
    if (btnPlanClearStart) btnPlanClearStart.onclick = () => { sendCommand("PLAN_CLEAR_START"); pathStatus.innerText = "已清除起点"; setTimeout(drawPreview, 300); };

    if (btnSendSeq) btnSendSeq.onclick = () => {
      const s = (seqInput.value || "").trim();
      if (s.length) sendCommand("SEQ:" + s);
    };
    if (btnSeqStart) btnSeqStart.onclick = () => sendCommand("SEQ_START");
    if (btnSeqStop) btnSeqStop.onclick = () => sendCommand("SEQ_STOP");

    if (btnAttackStart) btnAttackStart.onclick = () => sendCommand("SV1");
    if (btnAttackStop) btnAttackStop.onclick = () => sendCommand("SV0");

    // --- Simple preview drawing (not the real planner, just visualization) ---
    function parseCSV(str, n) {
      const parts = (str || "").split(",").map(s => s.trim()).filter(s => s.length);
      if (n && parts.length !== n) return null;
      const vals = parts.map(s => parseFloat(s));
      if (vals.some(v => Number.isNaN(v))) return null;
      return vals;
    }
    function drawPreview() {
      if (!pathCanvas) return;
      const ctx = pathCanvas.getContext("2d");
      const w = pathCanvas.width, h = pathCanvas.height;
      ctx.clearRect(0, 0, w, h);

      const b = parseCSV(planBound.value, 4) || [3920,5100,5700,1390];
      const bound = { xmin:b[0], xmax:b[1], ymax:b[2], ymin:b[3] };
      const tx = x => (x - bound.xmin) / (bound.xmax - bound.xmin) * w;
      const ty = y => h - (y - bound.ymin) / (bound.ymax - bound.ymin) * h;

      // boundary
      ctx.strokeStyle = "rgba(134,239,172,0.9)";
      ctx.lineWidth = 3;
      ctx.strokeRect(0, 0, w, h);

      // obstacle (optional)
      const o = parseCSV(planObs.value, null);
      if (o && o.length >= 4) {
        const left = Math.min(o[0], o[1]);
        const right = Math.max(o[0], o[1]);
        const top = Math.max(o[2], o[3]);
        const bottom = Math.min(o[2], o[3]);
        const margin = (o.length >= 5) ? o[4] : 0;
        ctx.fillStyle = "rgba(255,99,71,0.20)";
        ctx.strokeStyle = "rgba(255,99,71,0.70)";
        ctx.lineWidth = 2;
        const x = tx(left - margin), y = ty(top + margin);
        const ww = (right - left + 2*margin) / (bound.xmax - bound.xmin) * w;
        const hh = (top - bottom + 2*margin) / (bound.ymax - bound.ymin) * h;
        ctx.fillRect(x, y, ww, hh);
        ctx.strokeRect(x, y, ww, hh);
      }

      // start (current vive)
      const S = { x:vivePose.x, y:vivePose.y };
      const Tarr = parseCSV(planTarget.value, 2);
      const T = Tarr ? { x:Tarr[0], y:Tarr[1] } : null;

      function dot(p, color, r, label) {
        ctx.fillStyle = color;
        ctx.beginPath();
        ctx.arc(tx(p.x), ty(p.y), r, 0, Math.PI*2);
        ctx.fill();
        ctx.strokeStyle = "rgba(255,255,255,0.9)";
        ctx.lineWidth = 2;
        ctx.stroke();
        if (label) {
          ctx.fillStyle = color;
          ctx.font = "bold 12px Arial";
          ctx.fillText(label, tx(p.x)+r+4, ty(p.y)+4);
        }
      }
      dot(S, "#ff6b35", 6, "当前");
      if (T) dot(T, "#60a5fa", 6, "目标");

      // simple L path preview
      if (T) {
        ctx.strokeStyle = "rgba(59,130,246,0.9)";
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.moveTo(tx(S.x), ty(S.y));
        ctx.lineTo(tx(T.x), ty(S.y));
        ctx.lineTo(tx(T.x), ty(T.y));
        ctx.stroke();
      }
    }
    setInterval(drawPreview, 1500);

    // --- Params UI (render + localStorage + throttle) ---
    function fmt(v) {
      const n = Number(v);
      if (Number.isNaN(n)) return v;
      if (String(v).includes(".")) return n.toFixed(2);
      return String(n);
    }
    function throttle(fn, ms) {
      let last = 0, t = null, pending = null;
      return (...args) => {
        const now = Date.now();
        pending = args;
        const run = () => { last = Date.now(); t = null; fn(...pending); pending = null; };
        if (now - last >= ms) run();
        else if (!t) t = setTimeout(run, ms - (now - last));
      };
    }
    function makeParamCard(p) {
      const id = "p_" + p.key;
      const vid = "v_" + p.key;
      return `
        <div class="param">
          <div class="k">${p.key}</div>
          <div class="meta">${p.desc || ""}</div>
          <div class="ctrl">
            <label for="${id}">
              <span>${p.unit || ""}</span>
              <span class="kbd" id="${vid}">${fmt(p.def)}</span>
            </label>
            <input type="range" id="${id}" min="${p.min}" max="${p.max}" step="${p.step}" value="${p.def}">
          </div>
        </div>
      `;
    }

    // ===== 参数分类（按调参.md 组织）=====

    // 简洁巡墙模式（双轮原地转）
    // P控制巡墙参数
    const WALL_PARAMS = [
      { key:"WF_SPEED_FWD", min:0, max:100, step:1, def:50, desc:"巡墙速度" },
      { key:"WALL_DIST_KP", min:0.00, max:1.00, step:0.01, def:0.10, desc:"距离Kp" },
      { key:"WALL_ANGLE_KP", min:0.0, max:10.0, step:0.1, def:1.5, desc:"角度Kp" },
      { key:"WALL_TARGET_DIST", min:50, max:800, step:5, def:200, unit:"mm", desc:"目标离墙距离" },
      { key:"WF_MAX_TURN_RIGHT", min:0, max:100, step:1, def:70, desc:"最大右转力度" },
      { key:"WF_MAX_TURN_LEFT", min:0, max:100, step:1, def:70, desc:"最大左转力度" },
      { key:"WF_TURN_DEADBAND", min:0, max:50, step:1, def:14, desc:"直行死区" },
      { key:"FRONT_OBS_DIST", min:50, max:1000, step:10, def:300, unit:"mm", desc:"前方障碍阈值" },
      { key:"WALL_LOST_DIST", min:100, max:1500, step:10, def:650, unit:"mm", desc:"丢墙判定距离" },
      { key:"FRONT_PANIC_DIST", min:20, max:300, step:5, def:60, unit:"mm", desc:"紧急倒车距离" },
      { key:"TOF_SPACING_MM", min:50, max:300, step:1, def:143, unit:"mm", desc:"ToF间距" },
    ];

    // 胡同逃脱参数（倒车改变姿态 + 小半径转向）
    const ALLEY_PARAMS = [
      { key:"ALLEY_FRONT_CLOSE", min:50, max:1000, step:10, def:250, unit:"mm", desc:"前方近距触发（进胡同判定）" },
      { key:"ALLEY_RIGHT_CLOSE", min:50, max:1000, step:10, def:350, unit:"mm", desc:"右边近距触发（进胡同判定）" },
      { key:"ALLEY_BACKUP_SPEED", min:0, max:100, step:1, def:40, desc:"倒车速度" },
      { key:"ALLEY_BACKUP_TURN", min:0, max:100, step:1, def:30, desc:"倒车转向力度（车尾向右甩）" },
      { key:"ALLEY_BACKUP_MS", min:50, max:3000, step:50, def:600, unit:"ms", desc:"每次倒车时间" },
      { key:"ALLEY_CHECK_MS", min:0, max:1000, step:25, def:100, unit:"ms", desc:"倒车后检测时间" },
      { key:"ALLEY_FRONT_CLEAR", min:100, max:1500, step:10, def:400, unit:"mm", desc:"前方空了的阈值（出胡同判定）" },
      { key:"ALLEY_EXIT_TURN_STRENGTH", min:0, max:100, step:1, def:50, desc:"出胡同小半径右转力度" },
      { key:"ALLEY_EXIT_TURN_MS", min:50, max:2000, step:50, def:400, unit:"ms", desc:"小半径转向时间" },
      { key:"ALLEY_EXIT_FWD_SPEED", min:0, max:100, step:1, def:40, desc:"转完后直行速度" },
      { key:"ALLEY_EXIT_FWD_MS", min:50, max:2000, step:50, def:300, unit:"ms", desc:"转完后直行稳定时间" },
    ];

    // 📡 ToF 传感器标定
    const TOF_PARAMS = [
      { key:"TOF_OFFSET_F", min:-200, max:200, step:1, def:0, unit:"mm", desc:"前ToF偏移（测距偏大→设负值）" },
      { key:"TOF_OFFSET_R1", min:-200, max:200, step:1, def:0, unit:"mm", desc:"右前ToF偏移" },
      { key:"TOF_OFFSET_R2", min:-200, max:300, step:1, def:15, unit:"mm", desc:"右后ToF偏移" },
      { key:"TOF_SCALE_F", min:0.80, max:1.30, step:0.01, def:1.03, desc:"前ToF比例系数" },
      { key:"TOF_SCALE_R1", min:0.80, max:1.30, step:0.01, def:1.00, desc:"右前ToF比例系数" },
      { key:"TOF_SCALE_R2", min:0.80, max:1.30, step:0.01, def:1.00, desc:"右后ToF比例系数" },
      { key:"TOF_ALPHA", min:0.00, max:1.00, step:0.01, def:0.30, desc:"滤波系数（读数抖→调小0.2，响应慢→调大0.7）" },
      { key:"TOF_JUMP_MM", min:0, max:800, step:10, def:200, unit:"mm", desc:"跳变限幅（跳动大→调小100）" },
      { key:"TOF_MIN_MM", min:0, max:50, step:1, def:2, unit:"mm", desc:"最小有效距离" },
      { key:"TOF_MAX_MM", min:500, max:8000, step:100, def:5000, unit:"mm", desc:"最大有效距离" },
    ];

    // 手动规划（中频模式为主）
    const MP_PARAMS = [
      { key:"MP_DIST_TOL", min:20, max:150, step:5, def:50, desc:"到点距离阈值(mm)" },
      { key:"MP_ANGLE_TOL", min:5, max:30, step:1, def:12, desc:"朝向角容差(deg)" },
      { key:"MP_SPEED_FAR", min:20, max:100, step:5, def:60, desc:"远距离速度" },
      { key:"MP_SPEED_NEAR", min:10, max:80, step:5, def:35, desc:"近距离减速" },
      { key:"MP_TURN_RATE", min:30, max:150, step:5, def:65, desc:"原地转向力度" },
      { key:"MP_BUMP_FWD_MS", min:100, max:1500, step:50, def:500, unit:"ms", desc:"撞击前冲时间" },
      { key:"MP_BUMP_STOP_MS", min:50, max:1000, step:50, def:300, unit:"ms", desc:"撞后停顿时间" },
      { key:"MP_DEBUG", min:0, max:1, step:1, def:0, desc:"调试输出" },
      { key:"MP_MF_ENABLED", min:0, max:1, step:1, def:1, desc:"中频模式开关(默认开)" },
      { key:"MP_MF_EXEC_MS", min:200, max:800, step:25, def:400, unit:"ms", desc:"中频执行周期" },
      { key:"MP_MF_STOP_MIN_MS", min:50, max:300, step:25, def:100, unit:"ms", desc:"中频停车时间" },
      { key:"MP_MF_FIX_N", min:2, max:8, step:1, def:4, desc:"中频采样数" },
      { key:"MP_MF_POS_STD_MAX", min:20, max:100, step:5, def:40, unit:"mm", desc:"中频位置稳定阈值" },
      { key:"MP_MF_ANG_STD_MAX", min:5, max:30, step:1, def:15, unit:"deg", desc:"中频角度稳定阈值" },
      { key:"MP_LF_ENABLED", min:0, max:1, step:1, def:0, desc:"低频模式开关(默认关)" },
      { key:"MP_LF_EXEC_MS", min:100, max:2500, step:50, def:900, unit:"ms", desc:"低频执行周期" },
      { key:"MP_LF_STOP_MIN_MS", min:0, max:2500, step:50, def:250, unit:"ms", desc:"低频停车时间" },
      { key:"MP_LF_FIX_N", min:1, max:20, step:1, def:8, desc:"低频采样数" },
      { key:"MP_LF_POS_STD_MAX", min:0, max:200, step:1, def:30, unit:"mm", desc:"低频位置稳定阈值" },
      { key:"MP_LF_ANG_STD_MAX", min:0, max:90, step:1, def:12, unit:"deg", desc:"低频角度稳定阈值" },
    ];

    function renderSection(containerId, params, sender) {
      const el = document.getElementById(containerId);
      el.innerHTML = params.map(makeParamCard).join("");
      const throttled = throttle((k, v) => sender(k, v), 120);
      for (const p of params) {
        const slider = document.getElementById("p_" + p.key);
        const lab = document.getElementById("v_" + p.key);
        const storeKey = "gagac_" + p.key;
        const saved = localStorage.getItem(storeKey);
        if (saved !== null) {
          slider.value = saved;
          lab.innerText = fmt(saved);
        } else {
          lab.innerText = fmt(slider.value);
        }
        slider.addEventListener("input", () => {
          const v = slider.value;
          lab.innerText = fmt(v);
          localStorage.setItem(storeKey, v);
          throttled(p.key, v);
        });
      }
    }

    // 渲染各个参数面板
    renderSection("secWall", WALL_PARAMS, sendParam);
    renderSection("secAlley", ALLEY_PARAMS, sendParam);
    renderSection("secToF", TOF_PARAMS, sendParam);
    renderSection("secMp", MP_PARAMS, sendMpParam);

    // 所有参数数组（用于发送全部/重置）
    const ALL_PARAMS = [...WALL_PARAMS, ...ALLEY_PARAMS, ...TOF_PARAMS];

    function sendAll() {
      for (const p of ALL_PARAMS) sendParam(p.key, document.getElementById("p_" + p.key).value);
      for (const p of MP_PARAMS) sendMpParam(p.key, document.getElementById("p_" + p.key).value);
    }
    document.getElementById("btnSendAll").onclick = () => sendAll();
    document.getElementById("btnResetDefaults").onclick = () => {
      if (!confirm("确定要恢复默认值？（会清空浏览器保存的参数）")) return;
      const all = [...ALL_PARAMS, ...MP_PARAMS];
      for (const p of all) localStorage.removeItem("gagac_" + p.key);
      location.reload();
    };

    // init labels
    speedVal.innerText = speedSlider.value;
    turnVal.innerText = turnSlider.value;

    // ========== 曼哈顿路径规划 Canvas ==========
    const planCanvas = document.getElementById("planCanvas");
    const planTargetDisplay = document.getElementById("planTargetDisplay");
    const planStatusDisplay = document.getElementById("planStatusDisplay");
    const curPosDisplay = document.getElementById("curPosDisplay");
    const distDisplay = document.getElementById("distDisplay");
    const coordDisplay = document.getElementById("coordDisplay");
    const btnPlanExec = document.getElementById("btnPlanExec");
    const btnPlanExecLF = document.getElementById("btnPlanExecLF");
    const btnPlanClear = document.getElementById("btnPlanClear");
    const btnPlanStopNav = document.getElementById("btnPlanStopNav");
    const mfExecSlider = document.getElementById("mfExecSlider");
    const mfExecVal = document.getElementById("mfExecVal");

    // 场地边界（安全区域外边界）
    const fieldBound = { xmin: 3920, xmax: 5100, ymin: 1390, ymax: 5700 };
    // 障碍框（中间禁止区域）：X 4072~4950, Y 3120~4257
    const obstacleBox = { left: 4072, right: 4950, top: 4257, bottom: 3120 };
    // 当前选中的目标点
    let selectedTarget = null;

    // Canvas 坐标转换
    function canvasToField(cx, cy) {
      const w = planCanvas.width, h = planCanvas.height;
      const fx = fieldBound.xmin + (cx / w) * (fieldBound.xmax - fieldBound.xmin);
      const fy = fieldBound.ymax - (cy / h) * (fieldBound.ymax - fieldBound.ymin);
      return { x: fx, y: fy };
    }
    function fieldToCanvas(fx, fy) {
      const w = planCanvas.width, h = planCanvas.height;
      const cx = (fx - fieldBound.xmin) / (fieldBound.xmax - fieldBound.xmin) * w;
      const cy = h - (fy - fieldBound.ymin) / (fieldBound.ymax - fieldBound.ymin) * h;
      return { x: cx, y: cy };
    }

    // 检查点是否在障碍内
    function isInObstacle(fx, fy) {
      return fx >= obstacleBox.left && fx <= obstacleBox.right &&
             fy >= obstacleBox.bottom && fy <= obstacleBox.top;
    }

    // 检查点是否在安全区域内
    function isInSafeZone(fx, fy) {
      const inBound = fx >= fieldBound.xmin && fx <= fieldBound.xmax &&
                      fy >= fieldBound.ymin && fy <= fieldBound.ymax;
      return inBound && !isInObstacle(fx, fy);
    }

    // 计算曼哈顿路径（简单版：先X后Y，检测是否穿过障碍）
    function computeManhattanPath(sx, sy, tx, ty) {
      // 方案1: 先X后Y
      const mid1 = { x: tx, y: sy };
      const path1CrossObs = doesSegmentCrossObstacle(sx, sy, mid1.x, mid1.y) ||
                            doesSegmentCrossObstacle(mid1.x, mid1.y, tx, ty);
      if (!path1CrossObs) return [{ x: sx, y: sy }, mid1, { x: tx, y: ty }];

      // 方案2: 先Y后X
      const mid2 = { x: sx, y: ty };
      const path2CrossObs = doesSegmentCrossObstacle(sx, sy, mid2.x, mid2.y) ||
                            doesSegmentCrossObstacle(mid2.x, mid2.y, tx, ty);
      if (!path2CrossObs) return [{ x: sx, y: sy }, mid2, { x: tx, y: ty }];

      // 绕行（上/下/左/右）
      const detours = [
        { via: { x: sx, y: obstacleBox.top + 50 }, mid: { x: tx, y: obstacleBox.top + 50 } },   // 上绕
        { via: { x: sx, y: obstacleBox.bottom - 50 }, mid: { x: tx, y: obstacleBox.bottom - 50 } }, // 下绕
        { via: { x: obstacleBox.left - 50, y: sy }, mid: { x: obstacleBox.left - 50, y: ty } },  // 左绕
        { via: { x: obstacleBox.right + 50, y: sy }, mid: { x: obstacleBox.right + 50, y: ty } }, // 右绕
      ];
      for (const d of detours) {
        if (isInSafeZone(d.via.x, d.via.y) && isInSafeZone(d.mid.x, d.mid.y)) {
          return [{ x: sx, y: sy }, d.via, d.mid, { x: tx, y: ty }];
        }
      }
      // 无解，返回直线（实际会被障碍挡住）
      return [{ x: sx, y: sy }, { x: tx, y: ty }];
    }

    // 线段是否穿过障碍
    function doesSegmentCrossObstacle(x1, y1, x2, y2) {
      // 简化：检查线段是否与障碍矩形相交
      const horizontal = Math.abs(y1 - y2) < 1;
      const vertical = Math.abs(x1 - x2) < 1;
      if (horizontal) {
        const y = y1;
        if (y >= obstacleBox.bottom && y <= obstacleBox.top) {
          const minX = Math.min(x1, x2), maxX = Math.max(x1, x2);
          if (maxX >= obstacleBox.left && minX <= obstacleBox.right) return true;
        }
      } else if (vertical) {
        const x = x1;
        if (x >= obstacleBox.left && x <= obstacleBox.right) {
          const minY = Math.min(y1, y2), maxY = Math.max(y1, y2);
          if (maxY >= obstacleBox.bottom && minY <= obstacleBox.top) return true;
        }
      }
      return false;
    }

    // 绘制规划 Canvas
    function drawPlanCanvas() {
      const ctx = planCanvas.getContext("2d");
      const w = planCanvas.width, h = planCanvas.height;

      // 深色背景
      ctx.fillStyle = "#1a1a2e";
      ctx.fillRect(0, 0, w, h);

      // 绘制安全区域（外边界 - 内障碍 = 回字形）
      // 先填充整个外边界为绿色
      ctx.fillStyle = "rgba(74, 222, 128, 0.25)";
      ctx.fillRect(0, 0, w, h);

      // 再用障碍区域覆盖（红色）
      const obsP1 = fieldToCanvas(obstacleBox.left, obstacleBox.top);
      const obsP2 = fieldToCanvas(obstacleBox.right, obstacleBox.bottom);
      ctx.fillStyle = "rgba(239, 68, 68, 0.4)";
      ctx.fillRect(obsP1.x, obsP1.y, obsP2.x - obsP1.x, obsP2.y - obsP1.y);

      // 障碍边框
      ctx.strokeStyle = "#ef4444";
      ctx.lineWidth = 2;
      ctx.strokeRect(obsP1.x, obsP1.y, obsP2.x - obsP1.x, obsP2.y - obsP1.y);

      // 外边界框
      ctx.strokeStyle = "#4ade80";
      ctx.lineWidth = 3;
      ctx.strokeRect(2, 2, w - 4, h - 4);

      // 网格线
      ctx.strokeStyle = "rgba(255,255,255,0.1)";
      ctx.lineWidth = 1;
      for (let i = 1; i < 10; i++) {
        ctx.beginPath();
        ctx.moveTo(i * w / 10, 0);
        ctx.lineTo(i * w / 10, h);
        ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(0, i * h / 10);
        ctx.lineTo(w, i * h / 10);
        ctx.stroke();
      }

      // 坐标刻度标注
      ctx.fillStyle = "rgba(255,255,255,0.6)";
      ctx.font = "9px monospace";
      // X轴
      for (let x = fieldBound.xmin; x <= fieldBound.xmax; x += 200) {
        const cp = fieldToCanvas(x, fieldBound.ymin);
        ctx.fillText(x.toString(), cp.x - 12, h - 4);
      }
      // Y轴
      for (let y = fieldBound.ymin; y <= fieldBound.ymax; y += 500) {
        const cp = fieldToCanvas(fieldBound.xmin, y);
        ctx.fillText(y.toString(), 4, cp.y + 3);
      }

      // 规划路径
      if (selectedTarget && vivePose.x > 0 && vivePose.y > 0) {
        const path = computeManhattanPath(vivePose.x, vivePose.y, selectedTarget.x, selectedTarget.y);
        if (path.length >= 2) {
          ctx.strokeStyle = "#fbbf24";
          ctx.lineWidth = 3;
          ctx.setLineDash([6, 4]);
          ctx.beginPath();
          const p0 = fieldToCanvas(path[0].x, path[0].y);
          ctx.moveTo(p0.x, p0.y);
          for (let i = 1; i < path.length; i++) {
            const pi = fieldToCanvas(path[i].x, path[i].y);
            ctx.lineTo(pi.x, pi.y);
          }
          ctx.stroke();
          ctx.setLineDash([]);

          // 路径拐点标记
          ctx.fillStyle = "#fbbf24";
          for (let i = 1; i < path.length - 1; i++) {
            const pi = fieldToCanvas(path[i].x, path[i].y);
            ctx.beginPath();
            ctx.arc(pi.x, pi.y, 4, 0, Math.PI * 2);
            ctx.fill();
          }
        }
      }

      // 当前 VIVE 位置
      if (vivePose.x > 0 && vivePose.y > 0) {
        const cp = fieldToCanvas(vivePose.x, vivePose.y);
        // 光晕效果
        const gradient = ctx.createRadialGradient(cp.x, cp.y, 0, cp.x, cp.y, 20);
        gradient.addColorStop(0, "rgba(255,107,53,0.4)");
        gradient.addColorStop(1, "rgba(255,107,53,0)");
        ctx.fillStyle = gradient;
        ctx.beginPath();
        ctx.arc(cp.x, cp.y, 20, 0, Math.PI * 2);
        ctx.fill();
        // 主圆点
        ctx.fillStyle = "#ff6b35";
        ctx.beginPath();
        ctx.arc(cp.x, cp.y, 8, 0, Math.PI * 2);
        ctx.fill();
        ctx.strokeStyle = "#fff";
        ctx.lineWidth = 2;
        ctx.stroke();
        // 朝向箭头
        const angRad = (90 - vivePose.angle) * Math.PI / 180;
        const arrowLen = 22;
        ctx.strokeStyle = "#ff6b35";
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.moveTo(cp.x, cp.y);
        ctx.lineTo(cp.x + arrowLen * Math.cos(angRad), cp.y - arrowLen * Math.sin(angRad));
        ctx.stroke();
        // 箭头头部
        const headLen = 8;
        const headAng = 0.5;
        ctx.beginPath();
        ctx.moveTo(cp.x + arrowLen * Math.cos(angRad), cp.y - arrowLen * Math.sin(angRad));
        ctx.lineTo(cp.x + (arrowLen - headLen) * Math.cos(angRad - headAng), cp.y - (arrowLen - headLen) * Math.sin(angRad - headAng));
        ctx.moveTo(cp.x + arrowLen * Math.cos(angRad), cp.y - arrowLen * Math.sin(angRad));
        ctx.lineTo(cp.x + (arrowLen - headLen) * Math.cos(angRad + headAng), cp.y - (arrowLen - headLen) * Math.sin(angRad + headAng));
        ctx.stroke();

        // 更新当前位置显示
        curPosDisplay.innerText = `(${vivePose.x.toFixed(0)}, ${vivePose.y.toFixed(0)}) ${vivePose.angle.toFixed(1)}°`;
      } else {
        curPosDisplay.innerText = "等待VIVE...";
      }

      // 目标点
      if (selectedTarget) {
        const tp = fieldToCanvas(selectedTarget.x, selectedTarget.y);
        // 光晕效果
        const gradient = ctx.createRadialGradient(tp.x, tp.y, 0, tp.x, tp.y, 18);
        gradient.addColorStop(0, "rgba(96,165,250,0.4)");
        gradient.addColorStop(1, "rgba(96,165,250,0)");
        ctx.fillStyle = gradient;
        ctx.beginPath();
        ctx.arc(tp.x, tp.y, 18, 0, Math.PI * 2);
        ctx.fill();
        // 十字准星
        ctx.strokeStyle = "#60a5fa";
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(tp.x - 12, tp.y);
        ctx.lineTo(tp.x + 12, tp.y);
        ctx.moveTo(tp.x, tp.y - 12);
        ctx.lineTo(tp.x, tp.y + 12);
        ctx.stroke();
        // 主圆点
        ctx.fillStyle = "#60a5fa";
        ctx.beginPath();
        ctx.arc(tp.x, tp.y, 6, 0, Math.PI * 2);
        ctx.fill();
        ctx.strokeStyle = "#fff";
        ctx.lineWidth = 2;
        ctx.stroke();

        // 计算距离
        if (vivePose.x > 0 && vivePose.y > 0) {
          const dx = selectedTarget.x - vivePose.x;
          const dy = selectedTarget.y - vivePose.y;
          const dist = Math.sqrt(dx * dx + dy * dy);
          distDisplay.innerText = `${dist.toFixed(0)} mm`;
        }
      } else {
        distDisplay.innerText = "-";
      }
    }

    // 鼠标移动显示坐标
    planCanvas.addEventListener("mousemove", (e) => {
      const rect = planCanvas.getBoundingClientRect();
      const cx = e.clientX - rect.left;
      const cy = e.clientY - rect.top;
      const fp = canvasToField(cx, cy);
      const inSafe = isInSafeZone(fp.x, fp.y);
      coordDisplay.innerText = `X:${fp.x.toFixed(0)} Y:${fp.y.toFixed(0)} ${inSafe ? "[OK]" : "[!]"}`;
      coordDisplay.style.color = inSafe ? "#4ade80" : "#ef4444";
    });

    // Canvas 点击选点
    planCanvas.addEventListener("click", (e) => {
      const rect = planCanvas.getBoundingClientRect();
      const cx = e.clientX - rect.left;
      const cy = e.clientY - rect.top;
      const fp = canvasToField(cx, cy);
      const fx = Math.round(fp.x), fy = Math.round(fp.y);

      // 检查是否在安全区域内
      if (!isInSafeZone(fx, fy)) {
        planStatusDisplay.innerText = "目标点在障碍区域内！";
        planStatusDisplay.style.color = "#ef4444";
        addNavLog(`点击位置 (${fx}, ${fy}) 在障碍区域内`, "#ef4444");
        return;
      }

      selectedTarget = { x: fx, y: fy };
      planTargetDisplay.innerText = `(${fx}, ${fy})`;
      planStatusDisplay.innerText = "✓ 已选择目标，点击执行";
      planStatusDisplay.style.color = "#22c55e";
      // 计算距离
      let distInfo = "";
      if (vivePose.x > 0 && vivePose.y > 0) {
        const dx = fx - vivePose.x;
        const dy = fy - vivePose.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        distInfo = `, 距离 ${dist.toFixed(0)}mm`;
      }
      addNavLog(`选择目标: (${fx}, ${fy})${distInfo}`, "#60a5fa");
      drawPlanCanvas();
    });

    // 中频执行滑块
    mfExecSlider.oninput = function() {
      mfExecVal.innerText = this.value;
    };

    // 规划并执行（中频）
    btnPlanExec.onclick = () => {
      if (!selectedTarget) {
        planStatusDisplay.innerText = "请先点击选择目标点";
        planStatusDisplay.style.color = "#f59e0b";
        addNavLog("未选择目标点", "#f59e0b");
        return;
      }
      const mfMs = mfExecSlider.value;
      sendCommand(`MP_PARAM:MP_MF_EXEC_MS=${mfMs}`);
      sendCommand(`PLAN_MF:${selectedTarget.x},${selectedTarget.y}`);
      planStatusDisplay.innerText = `▶ 中频导航中... (${mfMs}ms)`;
      planStatusDisplay.style.color = "#22c55e";
      addNavLog(`发送中频规划命令 (周期=${mfMs}ms)`, "#22c55e");
    };

    // 执行（低频）
    btnPlanExecLF.onclick = () => {
      if (!selectedTarget) {
        planStatusDisplay.innerText = "请先点击选择目标点";
        planStatusDisplay.style.color = "#f59e0b";
        addNavLog("未选择目标点", "#f59e0b");
        return;
      }
      sendCommand(`PLAN1:${selectedTarget.x},${selectedTarget.y}`);
      planStatusDisplay.innerText = "▶ 低频导航中...";
      planStatusDisplay.style.color = "#3b82f6";
      addNavLog("发送低频规划命令", "#3b82f6");
    };

    // 清除目标
    btnPlanClear.onclick = () => {
      selectedTarget = null;
      planTargetDisplay.innerText = "未设置";
      planStatusDisplay.innerText = "就绪";
      planStatusDisplay.style.color = "";
      drawPlanCanvas();
      addNavLog("已清除目标点", "#6b7280");
    };

    // 停止导航
    btnPlanStopNav.onclick = () => {
      sendCommand("PLAN_STOP");
      planStatusDisplay.innerText = "■ 已停止";
      planStatusDisplay.style.color = "#ef4444";
      addNavLog("导航已停止", "#ef4444");
    };

    // ========== 导航日志 ==========
    const navLogArea = document.getElementById("navLogArea");
    const btnClearNavLog = document.getElementById("btnClearNavLog");
    let navLogCount = 0;
    let lastNavState = null;
    let lastLoggedPos = { x: 0, y: 0 };
    let isNavigating = false;
    let navStartTime = 0;
    let stepCount = 0;

    function addNavLog(msg, color = "#0f0") {
      const time = new Date().toLocaleTimeString("zh-CN", { hour12: false });
      const div = document.createElement("div");
      div.style.color = color;
      div.innerHTML = `<span style="color:#666;">[${time}]</span> ${msg}`;
      navLogArea.appendChild(div);
      navLogArea.scrollTop = navLogArea.scrollHeight;
      navLogCount++;
      // 限制日志数量
      if (navLogCount > 100) {
        navLogArea.removeChild(navLogArea.firstChild);
        navLogCount--;
      }
    }

    btnClearNavLog.onclick = () => {
      navLogArea.innerHTML = '<div style="color:#666;">[日志已清空]</div>';
      navLogCount = 1;
      stepCount = 0;
    };

    // 导航状态跟踪
    function trackNavigation() {
      if (!vivePose.x || !vivePose.y) return;

      // 检测导航开始
      const statusText = planStatusDisplay.innerText;
      const nowNavigating = statusText.includes("导航中");

      if (nowNavigating && !isNavigating) {
        // 导航刚开始
        isNavigating = true;
        navStartTime = Date.now();
        stepCount = 0;
        const mode = statusText.includes("中频") ? "中频" : "低频";
        addNavLog(`开始${mode}导航`, "#22c55e");
        if (selectedTarget) {
          addNavLog(`起点: (${vivePose.x.toFixed(0)}, ${vivePose.y.toFixed(0)})`, "#ff6b35");
          addNavLog(`目标: (${selectedTarget.x}, ${selectedTarget.y})`, "#60a5fa");
          const dx = selectedTarget.x - vivePose.x;
          const dy = selectedTarget.y - vivePose.y;
          const dist = Math.sqrt(dx * dx + dy * dy);
          addNavLog(`初始距离: ${dist.toFixed(0)} mm`, "#fbbf24");
        }
        lastLoggedPos = { x: vivePose.x, y: vivePose.y };
      }

      if (!nowNavigating && isNavigating) {
        // 导航结束
        isNavigating = false;
        const elapsed = ((Date.now() - navStartTime) / 1000).toFixed(1);
        if (statusText.includes("停止")) {
          addNavLog(`导航停止 (用时 ${elapsed}s, ${stepCount} 步)`, "#ef4444");
        } else {
          addNavLog(`导航完成 (用时 ${elapsed}s, ${stepCount} 步)`, "#22c55e");
        }
        if (selectedTarget) {
          const dx = selectedTarget.x - vivePose.x;
          const dy = selectedTarget.y - vivePose.y;
          const finalDist = Math.sqrt(dx * dx + dy * dy);
          addNavLog(`终点: (${vivePose.x.toFixed(0)}, ${vivePose.y.toFixed(0)})`, "#ff6b35");
          addNavLog(`终点误差: ${finalDist.toFixed(0)} mm`, finalDist < 100 ? "#22c55e" : "#f59e0b");
        }
      }

      // 导航中记录位置变化
      if (isNavigating) {
        const dx = vivePose.x - lastLoggedPos.x;
        const dy = vivePose.y - lastLoggedPos.y;
        const moved = Math.sqrt(dx * dx + dy * dy);
        // 移动超过 50mm 记录一次
        if (moved > 50) {
          stepCount++;
          let distToTarget = "-";
          if (selectedTarget) {
            const tdx = selectedTarget.x - vivePose.x;
            const tdy = selectedTarget.y - vivePose.y;
            distToTarget = Math.sqrt(tdx * tdx + tdy * tdy).toFixed(0);
          }
          addNavLog(`#${stepCount} (${vivePose.x.toFixed(0)}, ${vivePose.y.toFixed(0)}) θ=${vivePose.angle.toFixed(1)}° → 目标 ${distToTarget}mm`, "#aaa");
          lastLoggedPos = { x: vivePose.x, y: vivePose.y };
        }
      }
    }

    // 周期刷新 Canvas 和导航跟踪
    setInterval(() => {
      drawPlanCanvas();
      trackNavigation();
    }, 300);
  </script>
</body>
</html>
)rawliteral";


