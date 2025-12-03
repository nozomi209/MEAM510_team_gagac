const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Mobile Base Control</title>
<style>
  body {
    background: #ffffff;
    font-family: "Poppins", sans-serif;
    display: flex;
    justify-content: center;
    align-items: center;
    height: 100vh;
    margin: 0;
  }
  .control-card {
    background: #fff;
    border-radius: 18px;
    box-shadow: 0 8px 20px rgba(0,0,0,0.12);
    padding: 30px;
    width: 320px;
    text-align: center;
  }
  h2 {
    color: #333;
    font-size: 1.3em;
    margin-bottom: 20px;
  }
  .slider-group {
    margin-bottom: 20px;
    text-align: left;
  }
  label {
    font-size: 0.9em;
    color: #555;
  }
  input[type=range] {
    width: 100%;
    accent-color: #7fa87f;
  }
  .control-pad {
    display: grid;
    grid-template-columns: 80px 80px 80px;
    grid-template-rows: 80px 80px 80px;
    justify-content: center;
    align-items: center;
    margin-top: 10px;
  }
  .btn {
    background: #7fa87f;
    border: none;
    color: white;
    font-size: 1.2em;
    border-radius: 12px;
    height: 60px;
    width: 60px;
    cursor: pointer;
    transition: background 0.2s;
  }
  .btn:hover {
    background: #689668;
  }
  footer {
    color: #aaa;
    font-size: 0.8em;
    margin-top: 15px;
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
      <button class="btn stop-btn" id="btnS">S</button>
      <button class="btn" id="btnR">R</button>

      <div></div>
      <button class="btn" id="btnB">B</button>
      <div></div>
    </div>

    <div class="slider-group" style="margin-top: 20px; padding-top: 20px; border-top: 1px solid #eee;">
      <h3 style="font-size: 1em; color: #333; margin-bottom: 10px;">VIVE Tracking Data</h3>
      <div style="text-align: left; font-size: 0.85em; color: #666;">
        <div>X: <span id="viveXVal">0</span> mm</div>
        <div>Y: <span id="viveYVal">0</span> mm</div>
        <div>Angle: <span id="viveAngleVal">0</span>°</div>
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

  // current status
  let isMoving = false;
  let currentMoveDirection = null;    // "F" or "B"
  let isTurning = false;
  let currentTurnDirection = null;    // "L" or "R"

  // slider UI update
  speedSlider.oninput = () => {
    speedVal.innerText = speedSlider.value + "%";
  };
  turnSlider.oninput = () => {
    turnVal.innerText = turnSlider.value + "%";
  };

  function sendCommand(cmd) {
    fetch("/cmd?data=" + cmd).catch(err => console.log(err));
  }

  //adjust Speed slider(Q/W)
  function adjustSpeed(delta) {
    let v = parseInt(speedSlider.value) + delta;
    v = Math.max(0, Math.min(100, v));
    speedSlider.value = v;
    speedVal.innerText = v + "%";

    // update motor while forwarding ro backing
    if (isMoving && currentMoveDirection) {
      sendCommand(currentMoveDirection + v);
    }
  }

  //adjust Turn slider(A/S)
  function adjustTurn(delta) {
    let v = parseInt(turnSlider.value) + delta;
    v = Math.max(0, Math.min(100, v));
    turnSlider.value = v;
    turnVal.innerText = v + "%";

    // if turning, update turn factor live
    if (isTurning && currentTurnDirection) {
      sendCommand(currentTurnDirection + v);
    }
  }

  //mouse control
  buttons.F.onmousedown = () => {
    isMoving = true;
    currentMoveDirection = "F";
    isTurning = false;
    currentTurnDirection = null;
    sendCommand("F" + speedSlider.value);
  };
  buttons.B.onmousedown = () => {
    isMoving = true;
    currentMoveDirection = "B";
    isTurning = false;
    currentTurnDirection = null;
    sendCommand("B" + speedSlider.value);
  };
  buttons.L.onmousedown = () => {
    isTurning = true;
    currentTurnDirection = "L";
    isMoving = false;
    currentMoveDirection = null;
    sendCommand("L" + turnSlider.value);
  };
  buttons.R.onmousedown = () => {
    isTurning = true;
    currentTurnDirection = "R";
    isMoving = false;
    currentMoveDirection = null;
    sendCommand("R" + turnSlider.value);
  };

  buttons.S.onmousedown = () => {
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

  //keyboard control
  document.addEventListener("keydown", (e) => {
    if (e.repeat) return;

    switch (e.key) {
      
      case "ArrowUp":
        isMoving = true;
        currentMoveDirection = "F";
        isTurning = false;
        currentTurnDirection = null;
        sendCommand("F" + speedSlider.value);
        break;
      case "ArrowDown":
        isMoving = true;
        currentMoveDirection = "B";
        isTurning = false;
        currentTurnDirection = null;
        sendCommand("B" + speedSlider.value);
        break;
      case "ArrowLeft":
        isTurning = true;
        currentTurnDirection = "L";
        isMoving = false;
        currentMoveDirection = null;
        sendCommand("L" + turnSlider.value);
        break;
      case "ArrowRight":
        isTurning = true;
        currentTurnDirection = "R";
        isMoving = false;
        currentMoveDirection = null;
        sendCommand("R" + turnSlider.value);
        break;

      // Q / W:Speed slider
      case "q":
      case "Q":
        adjustSpeed(-5);
        break;
      case "w":
      case "W":
        adjustSpeed(+5);
        break;

      // A / S:Turn slider
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

  // only release direction key will send commands
  document.addEventListener("keyup", (e) => {
    if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"].includes(e.key)) {
      isMoving = false;
      isTurning = false;
      currentMoveDirection = null;
      currentTurnDirection = null;
      sendCommand("S");
    }
  });

  // Update VIVE tracking data - 优化：合并为一个请求以减少网络包
  function updateViveData() {
    fetch("/viveData")
      .then(response => response.json())
      .then(data => {
        document.getElementById("viveXVal").innerText = parseFloat(data.x).toFixed(1);
        document.getElementById("viveYVal").innerText = parseFloat(data.y).toFixed(1);
        document.getElementById("viveAngleVal").innerText = parseFloat(data.angle).toFixed(1);
      })
      .catch(err => console.log("VIVE data error:", err));
  }

  // 更新间隔改为500ms，减少网络包（从100ms改为500ms，减少80%的网络请求）
  // 如果还需要更少，可以改为1000ms（1秒更新一次）
  setInterval(updateViveData, 500);
</script>
</body>
</html>
)rawliteral";
