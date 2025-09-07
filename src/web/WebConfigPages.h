#pragma once

#include <Arduino.h>

// PROGMEM HTML templates for better memory usage
const char HTML_HEADER[] PROGMEM = R"(
<!DOCTYPE html><html><head><title>%s</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
body{font-family:Arial;margin:40px;background:#f0f0f0}
.container{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
h1{color:#333;text-align:center}
.info{background:#e7f3ff;padding:15px;border-radius:5px;margin:10px 0}
.error{background:#ffe7e7;padding:15px;border-radius:5px;margin:10px 0;color:#d00}
.success{background:#e7ffe7;padding:15px;border-radius:5px;margin:10px 0;color:#0a0}
.button{display:inline-block;padding:10px 20px;margin:5px;background:#007cba;color:white;text-decoration:none;border-radius:5px;border:none;cursor:pointer}
.button:hover{background:#005a87}
input[type=text],input[type=password],input[type=number]{width:100%%;padding:8px;margin:5px 0;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}
input[type=checkbox]{margin:5px}
label{display:block;margin:10px 0 5px 0;font-weight:bold}
</style></head><body>
)";

const char HTML_FOOTER[] PROGMEM = "</body></html>";

const char MQTT_CONFIG_PAGE[] PROGMEM = R"(
<div class='container'><h1>MQTT Configuration</h1>
<div class='info'><h3>Current Status</h3>
<p><strong>MQTT Enabled:</strong> %s</p>
%s
</div>
<form method='POST' action='/mqtt'>
<label><input type='checkbox' name='enabled' %s> Enable MQTT</label>
<label>MQTT Server:</label><input type='text' name='server' value='%s' placeholder='mqtt.example.com'>
<label>Port:</label><input type='number' name='port' value='%d' min='1' max='65535'>
<label>Username (optional):</label><input type='text' name='user' value='%s'>
<label>Password (optional):</label><input type='password' name='password' value='' placeholder='(unchanged)'>
<label>Client ID:</label><input type='text' name='clientid' value='%s'>
<br><br><input type='submit' value='Save Configuration' class='button'>
</form>
<br><a href='/' class='button'>Back to Main</a>
</div>
)";

const char ADMIN_CONFIG_PAGE[] PROGMEM = R"(
<div class='container'><h1>Admin Settings</h1>
<div class='info'><p>Change the web interface credentials used for protected pages.</p></div>
<form method='POST' action='/admin'>
<label>Username:</label><input type='text' name='user' value='%s'>
<label>New Password (leave blank to keep current):</label><input type='password' name='pass' value='' placeholder='(unchanged)'>
<br><br><input type='submit' value='Save Admin Credentials' class='button'>
</form>
<br><a href='/' class='button'>Back to Main</a>
</div>
)";
