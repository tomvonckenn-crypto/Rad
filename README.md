# Gemotoriseerd rad van fortuin

PlatformIO-project voor **Wemos D1 mini / ESP8266**. Versie 0.2.0 voegt handmatige browserbediening toe. De motor begint altijd vergrendeld op PWM 0; de pagina openen of firmware uploaden start de motor niet.

## Wifi zonder internet te verliezen

De Wemos gebruikt `WIFI_AP_STA`: hij behoudt het open netwerk **Rad-Wemos** op **192.168.4.1** en verbindt tegelijk met jouw 2,4 GHz-wifi of telefoonhotspot. Je computer blijft daarna op die gewone wifi en uploadt rechtstreeks naar het IP-adres van de Wemos. `rad-wemos.local` kan ook werken als mDNS op het netwerk beschikbaar is.

Omdat de oude firmware alleen een eigen netwerk heeft, wisselt het bootstrap-script eenmalig automatisch van wifi. Internet is tijdens die eerste upload kort weg. Het script draait lokaal door en vraagt ook bij een uploadfout Windows om terug te verbinden. Toekomstige uploads wisselen niet meer van netwerk.

Een telefoonhotspot moet verbindingen tussen zijn aangesloten apparaten toestaan. Als de hotspot die blokkeert, gebruik dan een gewone 2,4 GHz-router of een tweede netwerkverbinding op de computer. AP en station gebruiken op de ESP8266 dezelfde radio en hetzelfde kanaal. De Wemos levert via zijn eigen AP geen internetdoorvoer.

## Installatie en eerste upload

In PowerShell in deze map (de lokale omgeving is al ingericht als `.venv` bestaat):

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install platformio
```

Configureer de Wemos met een opgeslagen Windows-wifiprofiel. Het script verwerkt de sleutel lokaal, verwijdert het tijdelijke XML-bestand en toont het wachtwoord niet:

```powershell
.\scripts\Configure-Wifi.ps1 -ProfileName 'Galaxy A37 5G 9FCD'
```

`include/wifi_secrets.h` is lokaal en uitgesloten van Git. De firmware en `.pio`-buildbestanden bevatten deze gegevens ook; deel die niet als je de wifisleutel prive wilt houden. Voor een andere computer kun je `include/wifi_secrets.example.h` kopieren naar `include/wifi_secrets.h` en zelf invullen.

Zorg dat de Wemos gevoed is en het bestaande Windows-profiel `Rad-Wemos` beschikbaar is. Deze stap vereist dat de **huidige firmware ArduinoOTA ondersteunt**, zoals eerder getest:

```powershell
.\scripts\Bootstrap-Ota.ps1 -HomeProfile 'Galaxy A37 5G 9FCD'
```

Gebruik bij een PowerShell execution-policy-blokkade alleen voor dit proces:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Bootstrap-Ota.ps1 -HomeProfile 'Galaxy A37 5G 9FCD'
```

## Daarna uploaden en status bekijken

Vind het Wemos-adres in de lijst met verbonden apparaten van je hotspot/router. Open `http://<IP-adres>/` voor de statuspagina. De firmware vermeldt ook zijn adres via serieel op 115200 baud. Bij een werkende mDNS-resolutie kun je `http://rad-wemos.local/` gebruiken.

Je kunt het adres ook automatisch laten zoeken zonder van wifi te wisselen:

```powershell
.\.venv\Scripts\python.exe scripts\Discover-Wemos.py
```

Dit bewaart het gevonden adres lokaal. Zonder `-Address` gebruikt `Upload.ps1` dat adres. Zoek opnieuw als de hotspot een ander IP-adres heeft uitgedeeld.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Upload.ps1
# Of geef zelf het adres:
.\scripts\Upload.ps1 -Address 'rad-wemos.local'
# Bij ontbrekende mDNS: vervang het adres door het echte Wemos-IP.
```

In VS Code zijn taken beschikbaar voor bouwen en uploaden. De uploadtaak vraagt het adres. De PlatformIO-extensie is aanbevolen; de taken gebruiken de lokale Python-omgeving en vereisen die extensie niet.

ArduinoOTA gebruikt UDP 8266 op de Wemos en in dit project TCP 8267 terug naar de computer. Het bootstrap-script bouwt eerst online en gebruikt na de wifiwissel direct de lokale `espota.py` met de gewone Python-installatie. Er worden tijdens die upload geen PlatformIO-pakketten gedownload. De echte uploadmelding wordt opgeslagen in `.local/ota.log`.

`Waiting for device...` gevolgd door `No response from device` betekent dat de Wemos de UDP-uitnodiging heeft beantwoord, maar de TCP-terugverbinding niet binnenkomt. Controleer dan Windows Firewall voor het getoonde **Upload-Python**-pad. Schakel de firewall niet uit. Een eventuele inkomende regel kan beperkt worden tot TCP 8267, dat Python-programma en het Wemos-adres. Controleer na de eerste upload ook of de hotspot apparaten onderling laat communiceren.

## Handmatige bediening

1. Open de Wemos-statuspagina via zijn adres op je gewone wifi.
2. Controleer dat het rad fysiek stilstaat. **Vrijgeven** vraagt een expliciete bevestiging en zet geen PWM aan.
3. Kies **Start links op PWM 0** of **Start rechts op PWM 0**. Ook deze stap blijft op 0.
4. Verhoog PWM met **+1** of de schuifregelaar, uitsluitend wanneer je klaar bent voor een motortest. De voorlopige bovengrens is 300 van 1023. Actuele PWM verandert maximaal 1 stap per 30 ms; PWM-frequentie is 1000 Hz.
5. **Rustig stoppen** bouwt de PWM af. Wacht vervolgens op fysieke stilstand en bevestig opnieuw voordat je een richting kiest. Er zijn nog geen sensoren die fysieke stilstand meten.

De browser houdt de bedieningssessie actief met een heartbeat. Na 2 seconden zonder geldige heartbeat zet de controller PWM op 0 en vergrendelt hij. Een tabblad sluiten, herladen of langdurig op de achtergrond zetten kan deze stop veroorzaken. De controle gebeurt in de firmware-loop; dit is geen onafhankelijk hardwarebeveiligingssysteem.

## Rode NOODSTOP-knop

De rode knop is een **softwarestop via wifi**, geen fysieke of gecertificeerde noodstop. Een ontvangen stopcommando zet beide PWM-uitgangen op 0 en vergrendelt de bediening. Opnieuw starten vereist een nieuwe bevestiging; oude bedieningssessies en vertraagde vrijgavecommando's worden geweigerd. De stoproute werkt ook zonder vrijgegeven sessie.

Bij ontbrekende bevestiging meldt de pagina **STOP NIET BEVESTIGD**. Voor een echte noodstop is een geschikte fysieke onderbreker van de motorvoeding nodig. Met R_EN en L_EN vast op 5 V kan de Wemos de brug niet volledig uitschakelen. Twee lage PWM-uitgangen kunnen de motor elektrisch remmen; dit is geen aangetoonde vrije uitloop. De firmware schakelt ook tijdens OTA de PWM uit. Een OTA-fout houdt de bediening geblokkeerd tot herstart.

## Motorhardware en volgende stap

- RPWM: **GPIO4 / D2**; LPWM: **GPIO5 / D1**.
- Controller en BTS7960 moeten een gezamenlijke GND hebben.
- Volgens de gebruiker: VCC, R_EN en L_EN op 5 V; R_IS en L_IS niet aangesloten. 12 V voedt de motorzijde, de buckconverter levert gemeten ongeveer 5 V aan Wemos 5V en driverlogica; alle massa's zijn gemeenschappelijk.
- De 3,3 V-ingangscompatibiliteit van de drivermodule is onbevestigd. De gebruiker heeft de bufferchipcontrole overgeslagen en heeft toestemming gegeven de handmatige firmware te uploaden. Een softwaretest bewijst deze elektrische compatibiliteit niet.
- ESP8266-ingangen zijn niet geschikt voor rechtstreeks 5 V.
- Deze versie begint op PWM 0, houdt tijdens OTA de PWM op 0 en gebruikt bij handbediening nooit beide PWM-uitgangen tegelijk. Dit is geen meting van fysieke stilstand en garandeert geen vrije uitloop van de motor.
- De open AP- en OTA-configuratie uit de projectcontext blijven behouden.

Automatisch draaien en Hall-sensoren zijn nog niet toegevoegd. PWM begrenst het koppel niet; controleer de geprinte as en de overbrenging voor motortests. De agent voert geen motortest uit zonder expliciete toestemming voor die test.

## Verificatie

Zie `STATUS.md` voor de werkelijk uitgevoerde builds, uploads en hardwarecontroles. Een geslaagde build is geen bewijs van een geslaagde upload of werkende motor.

`src/motor_checks.cpp` laat de compiler de echte controllerlogica uitvoeren met `static_assert`: opstarten, ramp en limiet, volledige versnelling/vertraging, stopvergrendeling, verplichte nieuwe stilstandbevestiging, heartbeat-verloop inclusief timer-overloop en permanente OTA-fout. `tests/ui-checks.js` toetst het echte paginascript met gesimuleerde DOM/API-antwoorden, waaronder vertraagde antwoorden na een stop en verbindingsherstel. Er wordt hierbij geen motor aangestuurd. Met Node kan deze test ook via `node tests/ui-checks.js` draaien. `scripts/Preview-Controls.py` is uitsluitend een lokale UI-fixture op 127.0.0.1:8765 en maakt geen verbinding met de Wemos.

Bronnen: [ESP8266 OTA](https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html), [station en access point](https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html), [PlatformIO D1 mini](https://docs.platformio.org/en/latest/boards/espressif8266/d1_mini.html).
