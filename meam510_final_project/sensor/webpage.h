const char body[] PROGMEM = R"===(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;500;700&display=swap" rel="stylesheet">
    <style>
      body {
        display: flex;
        flex-direction: column;
        justify-content: flex-start;
        align-items: center;
        height: 100%;
        margin: 0;
        font-family: 'Roboto', sans-serif;
        overflow: hidden;
      }

      .tabs {
        display: flex;
        justify-content: center;
        width: 100%;
        margin-bottom: 10px;
        border-bottom: 4px solid #ddd;
      }

      .tab {
        flex: 1;
        text-align: center;
        padding: 10px 0;
        cursor: pointer;
        font-size: 14px;
        background-color: #f0f0f0;
        border: none;
        outline: none;
        transition: background-color 0.3s, color 0.3s;
      }

      .tab:hover {
        background-color: #f1f1f1;
        color: #007bff;
      }

      .tab.active {
        background-color: #0056b3;
        color: white;
        border-bottom: 4px solid #0056b3;
        box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.1);
      }

      .tab-content {
        display: none;
        flex-direction: column;
        align-items: center;
        width: 100%;
        max-width: 600px;
        background-color: #ffffff;
        border-radius: 10px;
        padding: 10px;
        box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.1);
        margin-top: 20px;
      }

      .tab-content.active {
        display: flex;
      }

      .toggle-buttons {
        display: flex;
        justify-content: center;
        margin-bottom: 10px;
        flex-wrap: wrap;
      }

      .toggle-button {
        padding: 8px 16px;
        margin: 5px;
        font-size: 14px;
        border: none;
        border-radius: 5px;
        cursor: pointer;
        background-color: #f0f0f0;
        transition: background-color 0.3s, color 0.3s;
      }

      .toggle-button:hover {
        background-color: #dcdcdc;
      }

      .toggle-button.active {
        background-color: #007bff;
        color: white;
        box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.1);
      }

      h2 {
        font-size: 24px;
        color: #333;
        margin-bottom: 10px;
        text-align: center;
      }

      .labels {
        display: flex;
        flex-wrap: wrap;
        justify-content: space-around;
        width: 100%;
        margin-bottom: 10px;
      }

      .label {
        display: flex;
        flex-direction: column;
        align-items: center;
        flex: 1;
        padding: 10px;
        margin: 5px;
        background-color: #f9f9f9;
        border-radius: 5px;
        box-shadow: 0px 2px 4px rgba(0, 0, 0, 0.1);
      }

      .static-label {
        font-size: 14px;
        color: #555;
        margin-bottom: 5px;
      }

      .dynamic-value {
        font-size: 16px;
        font-weight: bold;
        color: #333;
      }

      .button-container {
        display: flex;
        flex-direction: column;
        justify-content: center;
        align-items: center;
        width: 100%;
      }

      .row {
        display: flex;
        justify-content: center;
        margin: 5px 0;
        width: 100%;
      }

      /* Adjusting the stop button */
      .space-button {
        padding: 20px 60px; /* Increased padding for wider button */
        font-size: 18px; /* Increased font size for better visibility */
        background-color: grey;
        color: white;
        border-radius: 12px;
      }

      .space-button:hover {
        background-color: #cc0000;
      }

      .space-button:active {
        background-color: #990000;
      }

      /* Adjusting the arrow buttons */
      .row button {
        padding: 20px 30px; /* Increased padding for bigger buttons */
        font-size: 18px; /* Increased font size for bigger arrows */
        margin: 5px;
        border: none;
        border-radius: 8px;
        cursor: pointer;
        transition: background-color 0.3s, transform 0.1s;
        box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
      }

      .row button:hover {
        background-color: #007bff;
        color: white;
      }

      .row button:active {
        transform: scale(0.95);
        background-color: #6c757d;
      }
    </style>
  </head>
  <body>
    <div class="tabs">
      <button class="tab active" onclick="switchTab('automatic')">Command</button>
      <button class="tab" onclick="switchTab('manual')">Data</button>
    </div>

    <!-- Data Tab Content -->
    <div id="manual" class="tab-content">
      <h2>Time of Flight Sensors</h2>

      <div class="labels">
        <div class="label">
          <div class="static-label">Front Left</div>
          <div class="dynamic-value" id="frontLeftTofLabel">0</div>
        </div>
        <div class="label">
          <div class="static-label">Front Right</div>
          <div class="dynamic-value" id="frontRightTofLabel">0</div>
        </div>
      </div>

      <div class="labels">
        <div class="label">
          <div class="static-label">Back Left</div>
          <div class="dynamic-value" id="backLeftTofLabel">0</div>
        </div>
        <div class="label">
          <div class="static-label">Back Right</div>
          <div class="dynamic-value"> X</div>
        </div>
      </div>

      <h2>Vive Position Data</h2>
      <div class="labels">
        <div class="label">
          <div class="static-label">X</div>
          <div class="dynamic-value" id="viveXLabel">0</div>
        </div>
        <div class="label">
          <div class="static-label">Y</div>
          <div class="dynamic-value" id="viveYLabel">0</div>
        </div>
      </div>

      <div class="labels">
        <div class="label">
          <div class="static-label">Heading</div>
          <div class="dynamic-value" id="viveThetaLabel">0</div>
        </div>
      </div>
    </div>

    <!-- Automatic Tab Content -->
    <div id="automatic" class="tab-content active">

      <div class="toggle-buttons">
        <button id="manualMode" class="toggle-button active" onclick="setManualMode()">Manual</button>
        <button id="wallFollowMode" class="toggle-button" onclick="setWallFollowMode()">Wall Follow</button>
        <button id="autoNavRed" class="toggle-button" onclick="setAutoNavRed()">AutoRed</button>
        <button id="autoNavBlue" class="toggle-button" onclick="setAutoNavBlue()">AutoBlue</button>
      </div>

      <div class="button-container">
        <div class="row">
          <button type="button" onclick="hitW()">&#8593;</button>
        </div>
        <div class="row">
          <button type="button" onclick="hitA()">&#x21B0;</button>
          <button type="button" onclick="hitS()">&#8595;</button>
          <button type="button" onclick="hitD()">&#x21B1;</button>
        </div>
        <div class="row">
          <button class="space-button" type="button" onclick="hitSpace()">Stop</button>
        </div>
        <div class="row">
          <button type="button" onclick="setServoOn()">Servo On</button>
          <button type="button" onclick="setServoOff()">Servo Off</button>
        </div>
      </div>
    </div>

    <script>
      function switchTab(tabId) {
        const tabs = document.querySelectorAll(".tab");
        const contents = document.querySelectorAll(".tab-content");

        tabs.forEach((tab) => {
          if (tab.innerText.toLowerCase() === tabId) {
            tab.classList.add("active");
          } else {
            tab.classList.remove("active");
          }
        });

        contents.forEach((content) => {
          if (content.id === tabId) {
            content.classList.add("active");
          } else {
            content.classList.remove("active");
          }
        });
      }

      function clearActiveButton() {
  const buttons = document.querySelectorAll(".toggle-button");
  buttons.forEach((button) => {
    button.classList.remove("active"); // Remove active class from all toggle buttons
  });
}

      function setManualMode() {
        clearActiveButton();
        document.getElementById("manualMode").classList.add("active");
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "setManualMode", true);
        xhttp.send();
      }

      function setWallFollowMode() {
        clearActiveButton();
        document.getElementById("wallFollowMode").classList.add("active");
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "setWallFollowMode", true);
        xhttp.send();
      }

      function setAutoNavRed() {
        clearActiveButton();
        document.getElementById("autoNavRed").classList.add("active");
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "setAutoNavRed", true);
        xhttp.send();
      }

      function setAutoNavBlue() {
        clearActiveButton();
        document.getElementById("autoNavBlue").classList.add("active");
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "setAutoNavBlue", true);
        xhttp.send();
      }

      function setServoOn() {
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "setServoOn", true);
        xhttp.send();
      }

      function setServoOff() {
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "setServoOff", true);
        xhttp.send();
      }


      function hitW() {
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "hitW", true);
        xhttp.send();
      }

      function hitA() {
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "hitA", true);
        xhttp.send();
      }

      function hitS() {
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "hitS", true);
        xhttp.send();
      }

      function hitD() {
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "hitD", true);
        xhttp.send();
      }

      function hitSpace() {
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "hitSpace", true);
        xhttp.send();
      }

      // Updating Vive X, Y, and Theta values
      setInterval(updateViveX, 100);
      function updateViveX() {
        var xttp = new XMLHttpRequest();
        xttp.onreadystatechange = function () {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById("viveXLabel").innerHTML = this.responseText;
          }
        };
        xttp.open("GET", "viveX", true);
        xttp.send();
      }

      setInterval(updateViveY, 100);
      function updateViveY() {
        var xttp = new XMLHttpRequest();
        xttp.onreadystatechange = function () {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById("viveYLabel").innerHTML = this.responseText;
          }
        };
        xttp.open("GET", "viveY", true);
        xttp.send();
      }

      setInterval(updateViveTheta, 100);
      function updateViveTheta() {
        var xttp = new XMLHttpRequest();
        xttp.onreadystatechange = function () {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById("viveThetaLabel").innerHTML = this.responseText;
          }
        };
        xttp.open("GET", "viveTheta", true);
        xttp.send();
      }

      setInterval(updateFrontLeftTof, 100);
      function updateFrontLeftTof() {
        var xttp = new XMLHttpRequest();
        xttp.onreadystatechange = function () {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById("frontLeftTofLabel").innerHTML = this.responseText;
          }
        };
        xttp.open("GET", "frontLeftTof", true);
        xttp.send();
      }

      setInterval(updateFrontRightTof, 100);
      function updateFrontRightTof() {
        var xttp = new XMLHttpRequest();
        xttp.onreadystatechange = function () {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById("frontRightTofLabel").innerHTML = this.responseText;
          }
        };
        xttp.open("GET", "frontRightTof", true);
        xttp.send();
      }

      setInterval(updateBackLeftTof, 100);
      function updateBackLeftTof() {
        var xttp = new XMLHttpRequest();
        xttp.onreadystatechange = function () {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById("backLeftTofLabel").innerHTML = this.responseText;
          }
        };
        xttp.open("GET", "backLeftTof", true);
        xttp.send();
      }
    </script>
  </body>
</html>
)===";
