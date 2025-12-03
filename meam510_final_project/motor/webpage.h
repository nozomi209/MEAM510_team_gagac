const char body[] PROGMEM = R"===(
<!DOCTYPE html>
<html>
  <head> 
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
      /* Body styling */
      body {
        display: flex;
        flex-direction: column;
        justify-content: center;
        align-items: center;
        height: 100vh;
        margin: 0;
        background-color: #f0f0f0;
        font-family: Verdana, sans-serif;
      }

      .tabs {
        display: flex;
        justify-content: center;
        width: 100%;
        max-width: 600px;
        margin-bottom: 20px;
        border-bottom: 2px solid #ddd;
      }

      .tab {
        flex: 1;
        text-align: center;
        padding: 10px 20px;
        cursor: pointer;
        font-size: 18px;
        background-color: #f0f0f0;
        border: none;
        outline: none;
        transition: background-color 0.3s, color 0.3s;
      }

      .tab.active {
        background-color: #007bff;
        color: white;
        border-bottom: 2px solid #007bff;
      }

      /* Tab content styling */
      .tab-content {
        display: none;
        flex-direction: column;
        align-items: center;
        width: 100%;
        max-width: 600px;
      }

      .tab-content.active {
        display: flex;
      }

      /* Encoder labels styling */
      .encoder-labels {
        display: flex;
        justify-content: space-between;
        width: 100%;
        max-width: 500px;
        margin-bottom: 20px;
        text-align: center;
        font-size: 18px;
      }

      .encoder-label {
        flex: 1;
      }

      .static-label {
        font-size: 20px;
        margin-bottom: 5px;
      }

      .dynamic-value {
        font-size: 18px;
        color: #007bff;
      }

      /* Button styling */
      button {
        padding: 20px 30px;
        font-size: 20px;
        font-family: inherit; /* Inherit the font from the body */
        margin: 10px;
        border: none;
        border-radius: 8px;
        cursor: pointer;
        transition: background-color 0.3s, transform 0.1s;
        box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
      }

      /* Highlight on click (active state) */
      button:active {
        transform: scale(0.95);
        background-color: #6c757d;
        color: white;
      }

      /* Hover effect */
      button:hover {
        background-color: #007bff;
        color: white;
      }

      /* Container for the WASD layout */
      .button-container {
        display: flex;
        flex-direction: column;
        justify-content: center;
        align-items: center;
      }

      /* Row for W, A, S, D buttons */
      .row {
        display: flex;
        justify-content: center;
      }

      .space-button {
        width: 300px; /* Make space button wider */
        margin-top: 20px;
      }
    </style>

  </head>
  <body>

    <div class="tabs">
      <button class="tab active" onclick="switchTab('manual')">Command</button>
      <button class="tab" onclick="switchTab('automatic')">Map</button>
    </div>

    <!-- Tab Content -->
    <div id="manual" class="tab-content active">
      <!-- Encoder labels -->
      <div class="encoder-labels">
        <div class="encoder-label">
          <div class="static-label">Left Encoder RPM</div>
          <div class="dynamic-value" id="leftEncoderLabel">0</div>
        </div>
        <div class="encoder-label">
          <div class="static-label">Right Encoder RPM</div>
          <div class="dynamic-value" id="rightEncoderLabel">0</div>
        </div>
      </div>

      <!-- Buttons -->
      <div class="button-container">
        <!-- Row for W, A, S, D buttons -->
        <div class="row">
          <button type="button" onclick="hitW()"> ↑ </button>
        </div>
        <div class="row">
          <button type="button" onclick="hitA()"> ↰ </button>
          <button type="button" onclick="hitS()"> ↓ </button>
          <button type="button" onclick="hitD()"> ↱ </button>
        </div>
        <!-- Space button at the bottom -->
        <div class="row">
          <button class="space-button" type="button" onclick="hitSpace()"> Stop </button>
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

        setInterval(updateLeftEncoder, 250);
        function updateLeftEncoder() {
          var xttp = new XMLHttpRequest();
          xttp.onreadystatechange = function () {
            if (this.readyState == 4 && this.status == 200) {
              document.getElementById("leftEncoderLabel").innerHTML = this.responseText;
            }
          };
          xttp.open("GET", "leftEncoder", true);
          xttp.send();
        }

        setInterval(updateRightEncoder, 250);
        function updateRightEncoder() {
          var xttp = new XMLHttpRequest();
          xttp.onreadystatechange = function () {
            if (this.readyState == 4 && this.status == 200) {
              document.getElementById("rightEncoderLabel").innerHTML = this.responseText;
            }
          };
          xttp.open("GET", "rightEncoder", true);
          xttp.send();
        }
      </script>
      </body>
    </html>
)===";
