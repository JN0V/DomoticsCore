#ifndef DOMOTICS_CORE_OTA_PAGES_H
#define DOMOTICS_CORE_OTA_PAGES_H

#include <pgmspace.h>

const char HTML_OTA_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>OTA Update</title>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <style>
    body{font-family:Arial,sans-serif;margin:40px;background:#f0f0f0;color:#333;}
    .container{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:600px;margin:auto;}
    h1{color:#333;text-align:center;}
    .error{background:#ffe7e7;padding:15px;border-radius:5px;margin:10px 0;color:#d00;}
    .success{background:#e7ffe7;padding:15px;border-radius:5px;margin:10px 0;color:#0a0;}
    .button{display:inline-block;padding:10px 20px;margin:5px;background:#007cba;color:white;text-decoration:none;border-radius:5px;border:none;cursor:pointer;}
    input[type=file]{margin:10px 0;}
    form{margin-top:20px;}
  </style>
</head>
<body>
  <div class='container'>
    <h1>OTA Firmware Update</h1>
    %ERROR%
    <form method='POST' action='/update' enctype='multipart/form-data'>
      <p>Select firmware file (.bin):</p>
      <input type='file' name='update' accept='.bin' required>
      <br><br>
      <input type='submit' value='Upload Firmware' class='button'>
    </form>
    <br>
    <a href='/' class='button'>Back to Main</a>
  </div>
</body>
</html>
)rawliteral";

#endif // DOMOTICS_CORE_OTA_PAGES_H
