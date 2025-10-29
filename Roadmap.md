# DomoticsCore - Roadmap pour installation GitHub simplifiée

## Contexte
DomoticsCore v1.0 est actuellement un monorepo avec 12 composants modulaires. L'installation depuis GitHub via PlatformIO nécessite des configurations manuelles complexes (12 chemins `-I` dans build_flags, script embed_webui.py qui échoue, etc.).

**Objectif**: Rendre l'installation triviale avec juste:
```ini
lib_deps = https://github.com/JN0V/DomoticsCore.git#v1.0.0
```

## Solution retenue: Option B - Fixer le monorepo pour PlatformIO

### Tâche 1: Mettre à jour library.json racine

Modifier `/library.json` pour que PlatformIO détecte automatiquement tous les composants:

```json
{
  "name": "DomoticsCore",
  "version": "1.0.0",
  "description": "ESP32 domotics framework with WiFi, MQTT, web interface, Home Assistant integration, and persistent storage",
  "keywords": ["esp32", "domotics", "iot", "mqtt", "home-assistant", "wifi"],
  "repository": {
    "type": "git",
    "url": "https://github.com/JN0V/DomoticsCore.git"
  },
  "authors": [
    {
      "name": "JN0V",
      "maintainer": true
    }
  ],
  "license": "MIT",
  "homepage": "https://github.com/JN0V/DomoticsCore",
  "frameworks": ["arduino"],
  "platforms": ["espressif32"],
  "export": {
    "include": [
      "DomoticsCore-*/include"
    ]
  },
  "build": {
    "includeDir": ".",
    "flags": [
      "-IDomoticsCore-Core/include",
      "-IDomoticsCore-System/include",
      "-IDomoticsCore-Wifi/include",
      "-IDomoticsCore-LED/include",
      "-IDomoticsCore-Storage/include",
      "-IDomoticsCore-WebUI/include",
      "-IDomoticsCore-MQTT/include",
      "-IDomoticsCore-HomeAssistant/include",
      "-IDomoticsCore-OTA/include",
      "-IDomoticsCore-NTP/include",
      "-IDomoticsCore-RemoteConsole/include",
      "-IDomoticsCore-SystemInfo/include"
    ],
    "srcFilter": [
      "+<DomoticsCore-*/src/*>"
    ],
    "lib_archive": false
  },
  "dependencies": [
    {
      "owner": "bblanchon",
      "name": "ArduinoJson",
      "version": "^7.0.0"
    },
    {
      "owner": "ESP32Async",
      "name": "ESPAsyncWebServer",
      "version": "^3.8.0"
    },
    {
      "owner": "ESP32Async",
      "name": "AsyncTCP",
      "version": "^3.4.8"
    },
    {
      "owner": "knolleary",
      "name": "PubSubClient",
      "version": "^2.8"
    }
  ]
}
```

**Points clés:**
- `export.include`: Exporte tous les dossiers `include` des composants
- `build.flags`: Ajoute automatiquement tous les chemins include
- `build.srcFilter`: Compile tous les fichiers .cpp des composants
- `build.lib_archive: false`: Évite le cache problématique
- `dependencies`: Déclare les dépendances externes

### Tâche 2: Fixer embed_webui.py

Le script `DomoticsCore-WebUI/embed_webui.py` échoue quand installé via PlatformIO car il assume la structure du monorepo local.

**Solution: Rendre le script robuste pour détecter l'environnement**

Modifier `embed_webui.py` pour détecter automatiquement si on est en développement local ou installé via PlatformIO:

```python
Import("env")
import os
from pathlib import Path

def find_package_root():
    """Trouve la racine du package, que ce soit en dev local ou via PlatformIO"""
    try:
        # Essayer avec __file__ (marche en local et PlatformIO)
        script_path = Path(__file__).resolve()
        
        # Monorepo local: chercher le dossier 'packages' en remontant
        current = script_path.parent
        while current.parent != current:
            if (current / "packages").exists():
                # Dev local: retourner packages/
                return current / "packages"
            current = current.parent
        
        # Installation PlatformIO: pas de dossier 'packages' trouvé
        # Le script est dans DomoticsCore-WebUI/
        webui_root = script_path.parent
        
        # Chercher webui_src/ dans DomoticsCore-WebUI/
        webui_src = webui_root / "webui_src"
        if webui_src.exists():
            return webui_src
        
        # Fallback: peut-être dans le parent (structure alternative)
        parent_webui_src = webui_root.parent / "packages"
        if parent_webui_src.exists():
            return parent_webui_src
        
        raise RuntimeError(
            f"Could not locate WebUI sources.\n"
            f"Searched in:\n"
            f"  - {webui_src}\n"
            f"  - {parent_webui_src}\n"
            f"Please ensure 'webui_src/' or 'packages/' directory exists."
        )
        
    except NameError:
        # __file__ non disponible (environnement SCons très spécial)
        raise RuntimeError(
            "Unable to resolve paths: __file__ is not available.\n"
            "This is an unusual SCons environment."
        )

# Continuer avec la génération normale
package_root = find_package_root()
print(f"Using WebUI sources from: {package_root}")

# Suite du code de génération...
# ...
```

### Tâche 3: Ajouter un exemple d'installation GitHub

Créer `examples/GitHubInstallation/` avec un exemple minimal qui fonctionne:

**examples/GitHubInstallation/platformio.ini:**
```ini
; Example: Installing DomoticsCore from GitHub
; Just add the GitHub URL to lib_deps and it works!

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v1.0.0

build_flags = 
    -DCORE_DEBUG_LEVEL=3
```

**examples/GitHubInstallation/src/main.cpp:**
```cpp
#include <Arduino.h>
#include <DomoticsCore/System.h>

using namespace DomoticsCore;

System* domotics = nullptr;

void setup() {
    Serial.begin(115200);
    
    SystemConfig config = SystemConfig::fullStack();
    config.deviceName = "GitHub-Install-Test";
    config.wifiSSID = "";  // AP mode
    config.wifiPassword = "";
    config.ledPin = 2;
    
    domotics = new System(config);
    
    if (!domotics->begin()) {
        Serial.println("System init failed!");
        while(1) delay(1000);
    }
    
    Serial.println("System ready! Access point or WebUI available.");
}

void loop() {
    domotics->loop();
}
```

**examples/GitHubInstallation/README.md:**
```markdown
# Installing DomoticsCore from GitHub

This example demonstrates how simple it is to use DomoticsCore directly from GitHub.

## Installation

1. Create your project with this `platformio.ini`:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v1.0.0
```

2. Include and use:
```cpp
#include <DomoticsCore/System.h>
```

3. Build and upload:
```bash
pio run -t upload -t monitor
```

That's it! No manual configuration needed.

## What you get automatically
- All DomoticsCore components
- Correct include paths
- All dependencies (ArduinoJson, ESPAsyncWebServer, etc.)
- WebUI assets generated at compile time

## Troubleshooting
If compilation fails, ensure you're using DomoticsCore v1.0.0 or later:
```ini
lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v1.0.0
```
```

### Tâche 4: Mettre à jour GETTING_STARTED.md

Ajouter une section "Installation from GitHub" au début de GETTING_STARTED.md:

```markdown
## 🚀 Quick Start

### Installation from GitHub (Recommended during development)

**Step 1: Add to platformio.ini**
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v1.0.0
```

**Step 2: Write your code**
```cpp
#include <DomoticsCore/System.h>

using namespace DomoticsCore;
System* domotics = nullptr;

void setup() {
    SystemConfig config = SystemConfig::fullStack();
    config.deviceName = "MyDevice";
    config.wifiSSID = "";  // Empty = AP mode
    
    domotics = new System(config);
    domotics->begin();
}

void loop() {
    domotics->loop();
}
```

**Step 3: Build**
```bash
pio run -t upload -t monitor
```

That's it! Everything is configured automatically.

---

### Installation from PlatformIO Registry (Coming soon)
Once published on the registry:
```ini
lib_deps = 
    DomoticsCore-System
```
```

### Tâche 5: Test de validation

Créer un test automatisé dans `.github/workflows/test-github-install.yml`:

```yaml
name: Test GitHub Installation

on: [push, pull_request]

jobs:
  test-install:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.10'
      
      - name: Install PlatformIO
        run: |
          pip install platformio
      
      - name: Create test project
        run: |
          mkdir test-install
          cd test-install
          cat > platformio.ini << EOF
          [env:esp32dev]
          platform = espressif32
          board = esp32dev
          framework = arduino
          lib_deps = 
              https://github.com/JN0V/DomoticsCore.git#${GITHUB_SHA}
          EOF
          
          mkdir src
          cat > src/main.cpp << 'EOF'
          #include <DomoticsCore/System.h>
          using namespace DomoticsCore;
          System* sys = nullptr;
          void setup() {
              SystemConfig config;
              config.deviceName = "Test";
              sys = new System(config);
              sys->begin();
          }
          void loop() { sys->loop(); }
          EOF
      
      - name: Test compilation
        run: |
          cd test-install
          pio run
```

## Ordre d'implémentation

1. ✅ **Mettre à jour library.json** racine avec les chemins include
2. ✅ **Modifier embed_webui.py** pour détecter l'environnement (local vs PlatformIO)
3. ✅ **Créer l'exemple GitHubInstallation/**
4. ✅ **Mettre à jour GETTING_STARTED.md**
5. ✅ **Ajouter le workflow de test** GitHub Actions
6. ✅ **Tester manuellement** avec un projet vierge
7. ⏳ **Publier v1.0.1** avec ces corrections

## Test manuel

```bash
# Dans un nouveau dossier vide
mkdir /tmp/test-domotics && cd /tmp/test-domotics

cat > platformio.ini << EOF
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = https://github.com/JN0V/DomoticsCore.git#v1.0.0
EOF

mkdir src
cat > src/main.cpp << 'EOF'
#include <DomoticsCore/System.h>
using namespace DomoticsCore;
System* sys = nullptr;
void setup() {
    SystemConfig config;
    sys = new System(config);
    sys->begin();
}
void loop() { sys->loop(); }
EOF

pio run  # DOIT compiler sans erreur ✅
```

## Résultat attendu

Après ces modifications, un utilisateur pourra simplement faire:

```ini
lib_deps = https://github.com/JN0V/DomoticsCore.git#v1.0.0
```

Et `#include <DomoticsCore/System.h>` fonctionnera immédiatement, sans:
- ❌ Chemins include manuels
- ❌ Scripts extra
- ❌ Configuration complexe
- ❌ Erreurs de compilation

## Notes supplémentaires

- Garder la structure modulaire actuelle intacte
- Ces changements sont rétro-compatibles avec le développement local
- Une fois stable, publier sur le registre PlatformIO pour `lib_deps = DomoticsCore-System`
