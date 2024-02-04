#pragma once

const char htmlHomePage[] PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html lang="en">

<head>
   <title>ESP32CAM-PTZ-Websocket</title>
   <meta charset="utf-8">
   <meta name="viewport" content="width=device-width, initial-scale=1">
   <style>
      * {
         box-sizing: border-box;
         -moz-box-sizing: border-box;
         -webkit-box-sizing: border-box;
      }

      html,
      body,
      label,
      input {
         cursor: pointer;
      }

      .container {
         display: flex;
         flex-direction: column;
         align-items: center;
         justify-content: center;
         gap: 20px;
      }

      .custom {
         -webkit-appearance: none;
         width: 70%;
         height: 15px;
         background: #d3d3d3;
         outline: none;
      }

      .custom::-webkit-slider-thumb {
         -webkit-appearance: none;
         appearance: none;
         width: 20px;
         height: 20px;
         background: #04AA6D;
         cursor: pointer;
      }

      .num {
         width: 3rem;
         padding: 5px;
         border-radius: 3px;
         text-align: center;
         color: #fff;
         background: #0045ff;
         box-shadow: 0 0 2px 0 #555;
         display: inline-block;
         vertical-align: middle;
      }

      .slider {
         width: 500px;
         display: flex;
         align-items: center;
         justify-content: space-around;
      }

      .switch {
         position: relative;
         display: inline-block;
         width: 60px;
         height: 34px;
      }

      .slider1 {
         position: absolute;
         cursor: pointer;
         top: 0;
         left: 0;
         right: 0;
         bottom: 0;
         background-color: #ccc;
         -webkit-transition: .4s;
         transition: .4s;
      }

      .slider1:before {
         position: absolute;
         content: "";
         height: 26px;
         width: 26px;
         left: 4px;
         bottom: 4px;
         background-color: white;
         -webkit-transition: .4s;
         transition: .4s;
      }

      input:checked+.slider1 {
         background-color: #2196F3;
      }

      input:checked+.slider1:before {
         -webkit-transform: translateX(26px);
         -ms-transform: translateX(26px);
         transform: translateX(26px);
      }

      .slider>label,
      p {
         font-weight: bold;
      }
   </style>
</head>

<body>
   <div class="container">
      <img id="cameraImage" src="" style="width:800px;height:600px;background-color: black;" alt="TikTok@hieup25">
      <div class="slider">
         <label for="pan">Angle PAN: </label>
         <input type="range" id="pan" min="0" max="180" step="1" value="90" class="custom"
            onchange="chg_pan(this.value)" />
         <label for="pan" id="label_pan" class="num"></label>
      </div>
      <div class="slider">
         <label for="tilt">Angle TILT: </label>
         <input type="range" id="tilt" min="0" max="90" step="1" value="45" class="custom"
            onchange="chg_tilt(this.value)" />
         <label for="tilt" id="label_tilt" class="num"></label>
      </div>
      <div style="display: flex; align-items: center; gap: 10px;">
         <p>LED: </p>
         <label class="switch">
            <input type="checkbox" name="checkbox">
            <span class="slider1"></span>
         </label>
      </div>
   </div>
   <script type="text/javascript">
      var ws;
      let l_pan = document.getElementById("label_pan");
      let l_tilt = document.getElementById("label_tilt");
      let imageId = document.getElementById("cameraImage");
      let checkbox = document.querySelector("input[name=checkbox]");
      let json_pt = { "Pan": 90, "Tilt": 45 };
      document.addEventListener("DOMContentLoaded", function () {
         l_pan.innerText = document.getElementById("pan").value;
         l_tilt.innerText = document.getElementById("tilt").value;
         if ("WebSocket" in window) {
            ws = new WebSocket('ws://' + window.location.host + ':81/');
            // ws = new WebSocket('ws://192.168.0.128:81/');
            ws.onopen = function () { };
            ws.onmessage = function (evt) {
               const type = evt.data;
               if (type instanceof Blob) {
                  imageId.src = URL.createObjectURL(evt.data);
               } else if (typeof type === 'string') {
                  const json = JSON.parse(evt.data);
                  console.log("Data CFG:" + JSON.stringify(json));
                  if ("height" in json && "width" in json &&
                     "pan" in json && "tilt" in json && "led" in json) {
                     let w = json["width"];
                     let h = json["height"];
                     let p = json_pt.Pan = json["pan"];
                     let t = json_pt.Tilt = json["tilt"];
                     let l = json["led"];
                     imageId.style.width = `${w}px`;
                     imageId.style.height = `${h}px`;
                     checkbox.checked = l;
                     l_pan.innerText = p;
                     l_tilt.innerText = t;
                     document.getElementById("pan").value = p;
                     document.getElementById("tilt").value = t;
                     console.log(`cfg width: ${imageId.style.width}, height: ${imageId.style.height}, led: ${l}, pan: ${p}, tilt: ${t}`);
                  } else {
                     console.log("cfg default: width: 800px height: 600px, pan: 90, tilt: 45, led: off");
                  }
               }
            };
            ws.onclose = function () { };
         } else {
            alert("WebSocket NOT supported by your Browser!");
         }
      });
      checkbox.addEventListener('change', () => {
         const msg = {
            mode: "LED",
            data: { "status": true }
         };
         if (checkbox.checked) {
            msg.data.status = true;
         } else {
            msg.data.status = false;
         }
         ws.send(JSON.stringify(msg));
      });
      function chg_pan(val) {
         l_pan.innerText = val;
         json_pt.Pan = val;
         const msg = {
            mode: "PT",
            data: json_pt
         };
         ws.send(JSON.stringify(msg));
      }
      function chg_tilt(val) {
         l_tilt.innerText = val;
         json_pt.Tilt = val;
         const msg = {
            mode: "PT",
            data: json_pt
         };
         ws.send(JSON.stringify(msg));
      }
   </script>
</body>

</html>
)HTMLHOMEPAGE";
