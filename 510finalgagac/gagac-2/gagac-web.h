// 嵌入式网页（AP 模式下访问 192.168.4.1）：控制底盘运动、模式切换、VIVE 数据展示与参数调节
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Mobile Base Control</title>
<style>
  body {
    background: #f4f7f6;
    font-family: "Poppins", sans-serif;
    display: flex;
    justify-content: center;
    align-items: flex-start;
    min-height: 100vh;
    margin: 0;
    padding: 20px 0;
  }
  .control-card {
    background: #fff;
    border-radius: 24px;
    box-shadow: 0 12px 30px rgba(0,0,0,0.08);
    padding: 24px;
    width: 320px;
    text-align: center;
    margin-top: 10px;
  }
  h2 {
    color: #444;
    font-size: 1.4em;
    margin-bottom: 25px;
    font-weight: 600;
  }
  .slider-group {
    margin-bottom: 25px;
    text-align: left;
  }
  label {
    font-size: 0.9em;
    color: #666;
    font-weight: 500;
    margin-left: 2px;
  }
  
  /* Custom Slider Styling */
  input[type=range] {
    -webkit-appearance: none;
    width: 100%;
    margin-top: 12px;
    height: 8px;
    border-radius: 5px;
    background: #ececec; /* Light grey background */
    outline: none;
    cursor: pointer;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    appearance: none;
    width: 20px;
    height: 20px;
    border-radius: 50%;
    background: #92C08E; /* Pastel Green */
    box-shadow: 0 2px 5px rgba(0,0,0,0.2);
    cursor: pointer;
    transition: transform 0.1s;
  }
  input[type=range]::-webkit-slider-thumb:hover {
    transform: scale(1.1);
  }
  
  .control-pad {
    display: grid;
    grid-template-columns: 80px 80px 80px;
    grid-template-rows: 80px 80px 80px;
    justify-content: center;
    align-items: center;
    margin-top: 15px;
    margin-bottom: 25px;
  }
  .btn {
    background: #92C08E; /* Pastel Green */
    border: none;
    color: white;
    font-size: 1.2em;
    border-radius: 14px;
    height: 60px;
    width: 60px;
    cursor: pointer;
    transition: all 0.2s ease;
    box-shadow: 0 4px 10px rgba(146, 192, 142, 0.4);
  }
  .btn:active {
    transform: scale(0.95);
    box-shadow: 0 2px 5px rgba(146, 192, 142, 0.3);
  }
  .btn:hover {
    background: #81b37d;
  }
  
  .mode-btn-group {
    display: flex;
    gap: 15px;
    margin-bottom: 25px;
  }
  .mode-btn {
    flex: 1;
    border: none;
    color: white;
    font-size: 0.95em;
    font-weight: 600;
    border-radius: 14px;
    height: 50px;
    cursor: pointer;
    transition: all 0.3s;
    box-shadow: 0 4px 10px rgba(0,0,0,0.05);
    text-shadow: 0 1px 2px rgba(0,0,0,0.1);
  }
  .mode-btn:active {
    transform: translateY(2px);
    box-shadow: 0 2px 5px rgba(0,0,0,0.05);
  }

  footer {
    color: #ccc;
    font-size: 0.75em;
    margin-top: 25px;
  }
  @media (max-height: 700px) {
    body { padding: 10px 0; }
    .control-card { padding: 20px; width: 95vw; max-width: 360px; }
  }
</style>
</head>

<body>
  <div class="control-card">
    <h2>Mobile Base Control</h2>

    <div class="slider-group">
      <label for="speedSlider">Speed: <span id="speedVal">50%</span></label>
      <input type="range" id="speedSlider" min="0" max="100" value="50">
    </div>

    <div class="slider-group">
      <label for="turnSlider">Turn Factor: <span id="turnVal">30%</span></label>
      <input type="range" id="turnSlider" min="0" max="100" value="30">
    </div>

    <div class="control-pad">
      <div></div>
      <button class="btn" id="btnF">F</button>
      <div></div>

      <button class="btn" id="btnL">L</button>
      <button class="btn" id="btnS">S</button>
      <button class="btn" id="btnR">R</button>

      <div></div>
      <button class="btn" id="btnB">B</button>
      <div></div>
    </div>

    <div class="mode-btn-group">
      <button class="mode-btn" id="btnAuto" style="background:#F7E290;">
        Start Auto
      </button>
      
      <button class="mode-btn" id="btnVive" style="background:#E79DC3;">
        Enable VIVE
      </button>
    </div>

    <!-- 手动规划控制 -->
    <div class="mode-btn-group" style="margin-top:10px;">
      <button class="mode-btn" id="btnMp" style="background:#9fc5e8;">
        Start Manual Plan
      </button>
      <button class="mode-btn" id="btnSendRoute" style="background:#a4d7a7;">
        Send Route
      </button>
    </div>
    <div class="slider-group" style="margin-top:10px;">
      <label for="routeInput">Route (x,y,heading,bumps;...)</label>
      <textarea id="routeInput" style="width:100%; height:70px; margin-top:8px; border-radius:10px; border:1px solid #ddd; padding:8px; font-size:0.9em;">5120,4080,90,0;4661,4164,180,0;4661,4164,-90,1;4500,5800,90,4</textarea>
      <small style="color:#777;">示例: x,y,heading,bumps 多组用分号分隔</small>
    </div>

    <!-- 本地序列（时间控制直行/转向，不依赖 VIVE） -->
    <div class="mode-btn-group" style="margin-top:10px;">
      <button class="mode-btn" id="btnSeqStart" style="background:#ffa94d;">
        Start Sequence
      </button>
      <button class="mode-btn" id="btnSeqStop" style="background:#f08080;">
        Stop Sequence
      </button>
    </div>
    <div class="slider-group" style="margin-top:10px;">
      <label for="seqInput">Sequence (MODE,SPEED/Degree,DurationMs;...)</label>
      <textarea id="seqInput" style="width:100%; height:70px; margin-top:8px; border-radius:10px; border:1px solid #ddd; padding:8px; font-size:0.9em;">F,50,5600;S,0,100;L,100,1500</textarea>
      <small style="color:#777;">模式 F/B/L/R/S（S=暂停/停车），数值=速度或转向力度，持续时间 ms；用分号分隔。示例：F,50,2000;S,0,500;L,80,600;F,50,1500</small>
      <div style="margin-top:8px; display:flex; gap:10px;">
        <button class="mode-btn" id="btnSendSeq" style="flex:1; background:#a4d7a7;">Send Sequence</button>
      </div>
    </div>

    <div class="slider-group" style="margin-top: 20px; padding-top: 20px; border-top: 1px solid #f0f0f0;">
      <h3 style="font-size: 0.9em; color: #888; margin-bottom: 10px; font-weight:500;">VIVE Tracking Data</h3>
      <div style="text-align: left; font-size: 0.85em; color: #666; display:flex; flex-direction: column; gap:10px; background:#f8f9fa; padding:12px; border-radius:10px;">
        <div style="display:flex; justify-content: space-between;">
          <div>Center X: <span id="viveXVal">0</span></div>
          <div>Center Y: <span id="viveYVal">0</span></div>
          <div>Angle: <span id="viveAngleVal">0</span>°</div>
        </div>
        <div style="display:flex; gap:12px; flex-wrap: wrap;">
          <div style="flex:1; min-width:130px; background:#fff; border-radius:8px; padding:8px;">
            <div style="font-weight:600; color:#555;">Front (GPIO15 左)</div>
            <div>Raw: X=<span id="frontRawX">0</span>, Y=<span id="frontRawY">0</span></div>
            <div>Filtered: X=<span id="frontFiltX">0</span>, Y=<span id="frontFiltY">0</span></div>
            <div>Status: <span id="frontStatus">0</span></div>
          </div>
          <div style="flex:1; min-width:130px; background:#fff; border-radius:8px; padding:8px;">
            <div style="font-weight:600; color:#555;">Back (GPIO16 右)</div>
            <div>Raw: X=<span id="backRawX">0</span>, Y=<span id="backRawY">0</span></div>
            <div>Filtered: X=<span id="backFiltX">0</span>, Y=<span id="backFiltY">0</span></div>
            <div>Status: <span id="backStatus">0</span></div>
          </div>
        </div>
      </div>
    </div>

    <!-- 参数调整面板 -->
    <div class="slider-group" style="margin-top: 20px; padding-top: 20px; border-top: 1px solid #f0f0f0;">
      <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px;">
        <h3 style="font-size: 0.9em; color: #888; margin: 0; font-weight:500;">巡墙参数调整</h3>
        <button id="paramToggle" style="background: #92C08E; border: none; color: white; padding: 5px 15px; border-radius: 8px; cursor: pointer; font-size: 0.8em;">展开</button>
      </div>
      <div id="paramPanel" style="display: none;">
        <!-- 前方避障参数 -->
        <div style="margin-bottom: 15px; padding: 10px; background: #f8f9fa; border-radius: 8px;">
          <h4 style="font-size: 0.85em; color: #666; margin: 0 0 10px 0;">前方避障</h4>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>前方避障距离: <span id="frontTurnThVal">250</span>mm</label>
            <input type="range" id="frontTurnTh" min="100" max="500" value="250" step="10">
          </div>
          <div class="slider-group">
            <label>紧急倒车距离: <span id="frontBackupThVal">50</span>mm</label>
            <input type="range" id="frontBackupTh" min="20" max="150" value="50" step="5">
          </div>
        </div>

        <!-- 巡墙距离参数 -->
        <div style="margin-bottom: 15px; padding: 10px; background: #f8f9fa; border-radius: 8px;">
          <h4 style="font-size: 0.85em; color: #666; margin: 0 0 10px 0;">巡墙距离</h4>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>太近阈值: <span id="wallTooCloseVal">50</span>mm</label>
            <input type="range" id="wallTooClose" min="20" max="100" value="50" step="5">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>理想距离: <span id="wallIdealVal">80</span>mm</label>
            <input type="range" id="wallIdeal" min="50" max="150" value="80" step="5">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>太远阈值: <span id="wallTooFarVal">120</span>mm</label>
            <input type="range" id="wallTooFar" min="80" max="200" value="120" step="5">
          </div>
          <div class="slider-group">
            <label>丢墙阈值: <span id="rightLostWallVal">200</span>mm</label>
            <input type="range" id="rightLostWall" min="150" max="300" value="200" step="10">
          </div>
        </div>

        <!-- 速度参数 -->
        <div style="margin-bottom: 15px; padding: 10px; background: #f8f9fa; border-radius: 8px;">
          <h4 style="font-size: 0.85em; color: #666; margin: 0 0 10px 0;">速度参数</h4>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>前进速度: <span id="speedFwdVal">50</span></label>
            <input type="range" id="speedFwd" min="20" max="100" value="50" step="5">
          </div>
          <div class="slider-group">
            <label>倒车速度: <span id="speedBackVal">25</span></label>
            <input type="range" id="speedBack" min="10" max="50" value="25" step="5">
          </div>
        </div>

        <!-- 转向力度参数 -->
        <div style="margin-bottom: 15px; padding: 10px; background: #f8f9fa; border-radius: 8px;">
          <h4 style="font-size: 0.85em; color: #666; margin: 0 0 10px 0;">转向力度</h4>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>原地旋转: <span id="turnSpinVal">120</span></label>
            <input type="range" id="turnSpin" min="50" max="200" value="102" step="2">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>左转修正: <span id="turnCorrectVal">12</span></label>
            <input type="range" id="turnCorrect" min="5" max="50" value="12" step="1">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>右转找墙: <span id="turnGentleVal">12</span></label>
            <input type="range" id="turnGentle" min="5" max="50" value="12" step="1">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>强力找墙: <span id="turnHardFindVal">100</span></label>
            <input type="range" id="turnHardFind" min="50" max="200" value="100" step="5">
          </div>
          <div class="slider-group">
            <label>微调力度: <span id="turnTinyVal">10</span></label>
            <input type="range" id="turnTiny" min="5" max="30" value="10" step="1">
          </div>
        </div>

        <!-- 时间参数 -->
        <div style="margin-bottom: 15px; padding: 10px; background: #f8f9fa; border-radius: 8px;">
          <h4 style="font-size: 0.85em; color: #666; margin: 0 0 10px 0;">时间参数 (ms)</h4>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>卡死检测周期: <span id="stallCheckTimeVal">2000</span>ms</label>
            <input type="range" id="stallCheckTime" min="500" max="4000" value="2000" step="100">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>出胡同直行: <span id="seqExitStraightVal">700</span>ms</label>
            <input type="range" id="seqExitStraight" min="100" max="2000" value="700" step="50">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>出胡同转弯: <span id="seqExitTurnVal">200</span>ms</label>
            <input type="range" id="seqExitTurn" min="50" max="1500" value="200" step="50">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>出胡同停车: <span id="seqExitStopVal">100</span>ms</label>
            <input type="range" id="seqExitStop" min="50" max="1000" value="100" step="50">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>前方避障倒车: <span id="seqFrontBackVal">300</span>ms</label>
            <input type="range" id="seqFrontBack" min="50" max="1500" value="300" step="50">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>前方避障预停: <span id="seqFrontPreStopVal">100</span>ms</label>
            <input type="range" id="seqFrontPreStop" min="50" max="1000" value="100" step="50">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>前方避障转向: <span id="seqFrontTurnVal">500</span>ms</label>
            <input type="range" id="seqFrontTurn" min="100" max="2000" value="500" step="50">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>前方避障后停: <span id="seqFrontPostStopVal">100</span>ms</label>
            <input type="range" id="seqFrontPostStop" min="50" max="1000" value="100" step="50">
          </div>
          <div class="slider-group" style="margin-bottom: 10px;">
            <label>卡死倒车: <span id="seqStuckBackVal">800</span>ms</label>
            <input type="range" id="seqStuckBack" min="100" max="2000" value="800" step="50">
          </div>
          <div class="slider-group">
            <label>卡死旋转: <span id="seqStuckTurnVal">100</span>ms</label>
            <input type="range" id="seqStuckTurn" min="50" max="1000" value="100" step="50">
          </div>
        </div>
      </div>
    </div>

    <footer>Lab4.2 Team7 - Yui, Boru, Qingyun</footer>
  </div>

<script>
  const speedSlider = document.getElementById("speedSlider");
  const turnSlider = document.getElementById("turnSlider");
  const speedVal = document.getElementById("speedVal");
  const turnVal = document.getElementById("turnVal");

  const buttons = {
    F: document.getElementById("btnF"),
    B: document.getElementById("btnB"),
    L: document.getElementById("btnL"),
    R: document.getElementById("btnR"),
    S: document.getElementById("btnS")
  };

  // Update slider background gradient (Green to Light Grey)
  function updateSliderBackground(slider) {
    const value = (slider.value - slider.min) / (slider.max - slider.min) * 100;
    slider.style.background = `linear-gradient(to right, #92C08E 0%, #92C08E ${value}%, #ececec ${value}%, #ececec 100%)`;
  }

  // Init sliders
  updateSliderBackground(speedSlider);
  updateSliderBackground(turnSlider);

  // Auto Mode Logic
  const btnAuto = document.getElementById("btnAuto");
  let autoMode = false;
  
  btnAuto.onclick = () => {
    autoMode = !autoMode;
    if (autoMode) {
      btnAuto.innerText = "STOP Auto";
      btnAuto.style.background = "#ef9a9a"; // Soft Red
      sendCommand("AUTO_ON");
    } else {
      btnAuto.innerText = "Start Auto";
      btnAuto.style.background = "#F3CD35"; // Pastel Yellow
      sendCommand("AUTO_OFF");
    }
  };

  // Safety check: Stop Auto Mode on manual input
  function checkManualOverride() {
    if (autoMode) {
      btnAuto.click(); // Trigger stop logic
      console.log("Manual Override: Auto Mode Stopped");
    }
  }

  // VIVE Switch Logic
  const btnVive = document.getElementById("btnVive");
  let viveEnabled = false; 

  btnVive.onclick = () => {
    viveEnabled = !viveEnabled;
    if (viveEnabled) {
      btnVive.innerText = "Disable VIVE";
      btnVive.style.background = "#ce93d8"; // Soft Purple
      sendCommand("VIVE_ON");
    } else {
      btnVive.innerText = "Enable VIVE";
      btnVive.style.background = "#E79DC3"; // Pastel Pink
      sendCommand("VIVE_OFF");
    }
  };

  // Manual Planner Switch & Route Sender
  const btnMp = document.getElementById("btnMp");
  const btnSendRoute = document.getElementById("btnSendRoute");
  const routeInput = document.getElementById("routeInput");
  const btnSeqStart = document.getElementById("btnSeqStart");
  const btnSeqStop = document.getElementById("btnSeqStop");
  const btnSendSeq = document.getElementById("btnSendSeq");
  const seqInput = document.getElementById("seqInput");
  let mpEnabled = false;

  btnMp.onclick = () => {
    mpEnabled = !mpEnabled;
    if (mpEnabled) {
      btnMp.innerText = "Stop Manual Plan";
      btnMp.style.background = "#f4a261";
      sendCommand("MP_ON");
    } else {
      btnMp.innerText = "Start Manual Plan";
      btnMp.style.background = "#9fc5e8";
      sendCommand("MP_OFF");
    }
  };

  btnSendRoute.onclick = () => {
    const routeStr = routeInput.value.trim();
    if (routeStr.length === 0) return;
    sendCommand("MP_ROUTE:" + routeStr);
  };

  // Manual planner param update helper
  function sendMpParam(key, val) {
    sendCommand("MP_PARAM:" + key + "=" + val);
  }

  // Sequence control (local timed straight/turn)
  btnSendSeq.onclick = () => {
    const seq = seqInput.value.trim();
    if (seq.length === 0) return;
    sendCommand("SEQ:" + seq);
  };
  btnSeqStart.onclick = () => sendCommand("SEQ_START");
  btnSeqStop.onclick = () => sendCommand("SEQ_STOP");

  // State
  let isMoving = false;
  let currentMoveDirection = null;    
  let isTurning = false;
  let currentTurnDirection = null;    

  // Slider events
  speedSlider.oninput = function() {
    speedVal.innerText = this.value + "%";
    updateSliderBackground(this);
  };
  turnSlider.oninput = function() {
    turnVal.innerText = this.value + "%";
    updateSliderBackground(this);
  };

  function sendCommand(cmd) {
    fetch("/cmd?data=" + encodeURIComponent(cmd)).catch(err => console.log(err));
  }

  // Adjust Speed slider (Q/W)
  function adjustSpeed(delta) {
    let v = parseInt(speedSlider.value) + delta;
    v = Math.max(0, Math.min(100, v));
    speedSlider.value = v;
    speedVal.innerText = v + "%";
    updateSliderBackground(speedSlider);

    if (isMoving && currentMoveDirection) {
      sendCommand(currentMoveDirection + v);
    }
  }

  // Adjust Turn slider (A/S)
  function adjustTurn(delta) {
    let v = parseInt(turnSlider.value) + delta;
    v = Math.max(0, Math.min(100, v));
    turnSlider.value = v;
    turnVal.innerText = v + "%";
    updateSliderBackground(turnSlider);

    if (isTurning && currentTurnDirection) {
      sendCommand(currentTurnDirection + v);
    }
  }

  // Mouse Control
  buttons.F.onmousedown = () => {
    checkManualOverride();
    isMoving = true;
    currentMoveDirection = "F";
    isTurning = false;
    currentTurnDirection = null;
    sendCommand("F" + speedSlider.value);
  };
  buttons.B.onmousedown = () => {
    checkManualOverride();
    isMoving = true;
    currentMoveDirection = "B";
    isTurning = false;
    currentTurnDirection = null;
    sendCommand("B" + speedSlider.value);
  };
  buttons.L.onmousedown = () => {
    checkManualOverride();
    isTurning = true;
    currentTurnDirection = "L";
    isMoving = false;
    currentMoveDirection = null;
    sendCommand("L" + turnSlider.value);
  };
  buttons.R.onmousedown = () => {
    checkManualOverride();
    isTurning = true;
    currentTurnDirection = "R";
    isMoving = false;
    currentMoveDirection = null;
    sendCommand("R" + turnSlider.value);
  };
  buttons.S.onmousedown = () => {
    checkManualOverride();
    isMoving = false;
    isTurning = false;
    currentMoveDirection = null;
    currentTurnDirection = null;
    sendCommand("S");
  };

  buttons.F.onmouseup =
  buttons.B.onmouseup =
  buttons.L.onmouseup =
  buttons.R.onmouseup = () => {
    isMoving = false;
    isTurning = false;
    currentMoveDirection = null;
    currentTurnDirection = null;
    sendCommand("S");
  };

  // Keyboard Control
  document.addEventListener("keydown", (e) => {
    if (e.repeat) return;

    switch (e.key) {
      case "ArrowUp":
        checkManualOverride();
        isMoving = true;
        currentMoveDirection = "F";
        isTurning = false;
        currentTurnDirection = null;
        sendCommand("F" + speedSlider.value);
        break;
      case "ArrowDown":
        checkManualOverride();
        isMoving = true;
        currentMoveDirection = "B";
        isTurning = false;
        currentTurnDirection = null;
        sendCommand("B" + speedSlider.value);
        break;
      case "ArrowLeft":
        checkManualOverride();
        isTurning = true;
        currentTurnDirection = "L";
        isMoving = false;
        currentMoveDirection = null;
        sendCommand("L" + turnSlider.value);
        break;
      case "ArrowRight":
        checkManualOverride();
        isTurning = true;
        currentTurnDirection = "R";
        isMoving = false;
        currentMoveDirection = null;
        sendCommand("R" + turnSlider.value);
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
    if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"].includes(e.key)) {
      isMoving = false;
      isTurning = false;
      currentMoveDirection = null;
      currentTurnDirection = null;
      sendCommand("S");
    }
  });

  // VIVE Data Update
  function updateViveData() {
    fetch("/viveData")
      .then(response => response.json())
      .then(data => {
        document.getElementById("viveXVal").innerText = parseFloat(data.x).toFixed(1);
        document.getElementById("viveYVal").innerText = parseFloat(data.y).toFixed(1);
        document.getElementById("viveAngleVal").innerText = parseFloat(data.angle).toFixed(1);
        if (data.frontRaw && data.backRaw) {
          document.getElementById("frontRawX").innerText = data.frontRaw.x || 0;
          document.getElementById("frontRawY").innerText = data.frontRaw.y || 0;
          document.getElementById("backRawX").innerText = data.backRaw.x || 0;
          document.getElementById("backRawY").innerText = data.backRaw.y || 0;
        }
        if (data.frontFiltered && data.backFiltered) {
          document.getElementById("frontFiltX").innerText = data.frontFiltered.x || 0;
          document.getElementById("frontFiltY").innerText = data.frontFiltered.y || 0;
          document.getElementById("backFiltX").innerText = data.backFiltered.x || 0;
          document.getElementById("backFiltY").innerText = data.backFiltered.y || 0;
        }
        if (data.status) {
          document.getElementById("frontStatus").innerText = data.status.front;
          document.getElementById("backStatus").innerText = data.status.back;
        }
      })
      .catch(err => console.log("VIVE data error:", err));
  }

  setInterval(updateViveData, 1000);

  // 参数调整面板切换
  const paramToggle = document.getElementById("paramToggle");
  const paramPanel = document.getElementById("paramPanel");
  // 手动规划参数面板
  const mpParamToggle = document.createElement("button");
  mpParamToggle.id = "mpParamToggle";
  mpParamToggle.innerText = "展开";
  mpParamToggle.style = "background:#92C08E;border:none;color:white;padding:5px 15px;border-radius:8px;cursor:pointer;font-size:0.8em;";

  const mpParamPanel = document.createElement("div");
  mpParamPanel.id = "mpParamPanel";
  mpParamPanel.style.display = "none";
  mpParamPanel.innerHTML = `
    <div style="margin-top:15px; padding:10px; background:#f8f9fa; border-radius:8px;">
      <h4 style="font-size:0.85em; color:#666; margin:0 0 10px 0;">手动规划参数</h4>
      <div id="mpParamSliders"></div>
    </div>
  `;

  paramToggle.onclick = () => {
    if (paramPanel.style.display === "none") {
      paramPanel.style.display = "block";
      paramToggle.innerText = "收起";
    } else {
      paramPanel.style.display = "none";
      paramToggle.innerText = "展开";
    }
  };

  // 参数滑块初始化
  const paramSliders = {
    "FRONT_TURN_TH": { slider: document.getElementById("frontTurnTh"), val: document.getElementById("frontTurnThVal") },
    "FRONT_BACKUP_TH": { slider: document.getElementById("frontBackupTh"), val: document.getElementById("frontBackupThVal") },
    "WALL_TOO_CLOSE": { slider: document.getElementById("wallTooClose"), val: document.getElementById("wallTooCloseVal") },
    "WALL_IDEAL": { slider: document.getElementById("wallIdeal"), val: document.getElementById("wallIdealVal") },
    "WALL_TOO_FAR": { slider: document.getElementById("wallTooFar"), val: document.getElementById("wallTooFarVal") },
    "RIGHT_LOST_WALL": { slider: document.getElementById("rightLostWall"), val: document.getElementById("rightLostWallVal") },
    "SPEED_FWD": { slider: document.getElementById("speedFwd"), val: document.getElementById("speedFwdVal") },
    "SPEED_BACK": { slider: document.getElementById("speedBack"), val: document.getElementById("speedBackVal") },
    "TURN_SPIN": { slider: document.getElementById("turnSpin"), val: document.getElementById("turnSpinVal") },
    "TURN_CORRECT": { slider: document.getElementById("turnCorrect"), val: document.getElementById("turnCorrectVal") },
    "TURN_GENTLE": { slider: document.getElementById("turnGentle"), val: document.getElementById("turnGentleVal") },
    "TURN_HARD_FIND": { slider: document.getElementById("turnHardFind"), val: document.getElementById("turnHardFindVal") },
    "TURN_TINY": { slider: document.getElementById("turnTiny"), val: document.getElementById("turnTinyVal") },
    "STALL_CHECK_TIME": { slider: document.getElementById("stallCheckTime"), val: document.getElementById("stallCheckTimeVal") },
    "SEQ_EXIT_STRAIGHT_MS": { slider: document.getElementById("seqExitStraight"), val: document.getElementById("seqExitStraightVal") },
    "SEQ_EXIT_TURN_MS": { slider: document.getElementById("seqExitTurn"), val: document.getElementById("seqExitTurnVal") },
    "SEQ_EXIT_STOP_MS": { slider: document.getElementById("seqExitStop"), val: document.getElementById("seqExitStopVal") },
    "SEQ_FRONT_BACK_MS": { slider: document.getElementById("seqFrontBack"), val: document.getElementById("seqFrontBackVal") },
    "SEQ_FRONT_PRE_STOP_MS": { slider: document.getElementById("seqFrontPreStop"), val: document.getElementById("seqFrontPreStopVal") },
    "SEQ_FRONT_TURN_MS": { slider: document.getElementById("seqFrontTurn"), val: document.getElementById("seqFrontTurnVal") },
    "SEQ_FRONT_POST_STOP_MS": { slider: document.getElementById("seqFrontPostStop"), val: document.getElementById("seqFrontPostStopVal") },
    "SEQ_STUCK_BACK_MS": { slider: document.getElementById("seqStuckBack"), val: document.getElementById("seqStuckBackVal") },
    "SEQ_STUCK_TURN_MS": { slider: document.getElementById("seqStuckTurn"), val: document.getElementById("seqStuckTurnVal") }
  };

  // Manual planner sliders (VIVE 点对点)
  const mpSliders = {
    "MP_DIST_TOL": { id: "mpDistTol", label: "mpDistTolVal", min: 20, max: 150, step: 5, def: 50 },
    "MP_ANGLE_TOL": { id: "mpAngleTol", label: "mpAngleTolVal", min: 5, max: 45, step: 1, def: 15 },
    "MP_SPEED_FAR": { id: "mpSpeedFar", label: "mpSpeedFarVal", min: 20, max: 100, step: 5, def: 70 },
    "MP_SPEED_NEAR": { id: "mpSpeedNear", label: "mpSpeedNearVal", min: 10, max: 80, step: 5, def: 40 },
    "MP_TURN_RATE": { id: "mpTurnRate", label: "mpTurnRateVal", min: 30, max: 150, step: 5, def: 80 },
    "MP_BUMP_FWD_MS": { id: "mpBumpFwd", label: "mpBumpFwdVal", min: 100, max: 1500, step: 50, def: 500 },
    "MP_BUMP_STOP_MS": { id: "mpBumpStop", label: "mpBumpStopVal", min: 50, max: 1000, step: 50, def: 300 }
  };

  // 初始化所有参数滑块
  for (const [paramName, obj] of Object.entries(paramSliders)) {
    if (obj.slider && obj.val) {
      updateSliderBackground(obj.slider);
      obj.slider.oninput = function() {
        const value = parseFloat(this.value);
        obj.val.innerText = value;
        updateSliderBackground(this);
        // 实时发送参数更新
        sendCommand("PARAM:" + paramName + "=" + value);
      };
    }
  }

  // 在页面末尾追加 MP 参数面板
  (function mountMpPanel() {
    const modeGroup = document.querySelector(".mode-btn-group");
    const card = document.querySelector(".control-card");
    if (card) {
      const wrapper = document.createElement("div");
      wrapper.style.marginTop = "10px";
      wrapper.style.paddingTop = "10px";
      wrapper.style.borderTop = "1px solid #f0f0f0";
      const titleRow = document.createElement("div");
      titleRow.style.display = "flex";
      titleRow.style.justifyContent = "space-between";
      titleRow.style.alignItems = "center";
      titleRow.style.marginBottom = "10px";
      const h3 = document.createElement("h3");
      h3.style = "font-size:0.9em;color:#888;margin:0;font-weight:500;";
      h3.innerText = "手动规划参数";
      titleRow.appendChild(h3);
      titleRow.appendChild(mpParamToggle);
      wrapper.appendChild(titleRow);
      wrapper.appendChild(mpParamPanel);
      card.appendChild(wrapper);
    }
  })();

  mpParamToggle.onclick = () => {
    if (mpParamPanel.style.display === "none") {
      mpParamPanel.style.display = "block";
      mpParamToggle.innerText = "收起";
    } else {
      mpParamPanel.style.display = "none";
      mpParamToggle.innerText = "展开";
    }
  };

  // 渲染 MP 参数滑块
  (function renderMpSliders() {
    const container = document.getElementById("mpParamSliders");
    if (!container) return;
    for (const [k, cfg] of Object.entries(mpSliders)) {
      const div = document.createElement("div");
      div.className = "slider-group";
      div.style.marginBottom = "10px";
      div.innerHTML = `
        <label>${k}: <span id="${cfg.label}">${cfg.def}</span></label>
        <input type="range" id="${cfg.id}" min="${cfg.min}" max="${cfg.max}" step="${cfg.step}" value="${cfg.def}">
      `;
      container.appendChild(div);
      const slider = document.getElementById(cfg.id);
      const valLab = document.getElementById(cfg.label);
      if (slider && valLab) {
        slider.oninput = function() {
          valLab.innerText = this.value;
          updateSliderBackground(this);
          sendMpParam(k, this.value);
        };
        updateSliderBackground(slider);
      }
    }
  })();
</script>
</body>
</html>
)rawliteral";