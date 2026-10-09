# Projectstatus

- Doelboard: Wemos D1 mini / ESP8266 volgens de aangeleverde context; fysiek board niet geinspecteerd.
- Eerste versie: netwerk, ArduinoOTA, browserstatus en beide motoruitgangen uit.
- Motor, driveraansluitingen, overbrenging en sensoren nog niet getest.
- Build: rad_bootstrap is succesvol gecompileerd met ESP8266 Arduino Core 3.1.2; RAM 29292 bytes, flash 334491 bytes.
- Eerste agent-uploadpoging: gestopt voor firmwareoverdracht omdat PlatformIO offline een extra tool wilde downloaden; gewone wifi automatisch hersteld.
- Gebruikerstest: Wemos beantwoordt de OTA-uitnodiging, maar TCP-terugverbinding op poort 8267 kwam niet binnen (`No response from device`). Firmwareoverdracht niet gestart.
- Bootstrap-script aangepast: direct lokale uploader met de gewone Python-installatie; echte foutmelding in `.local/ota.log`.
- Hardwaretest op 9 oktober 2026: eerste OTA-upload met de aangepaste uploader geslaagd; Windows automatisch terugverbonden met Galaxy A37 5G 9FCD.
- Wemos vervolgens via de hotspot bereikbaar op 10.195.248.80. `/api/status` bevestigt stationverbinding, reserve-AP 192.168.4.1 en motorbediening uit.
- Tweede OTA-upload via `scripts/Upload.ps1` geslaagd op dezelfde hotspot, zonder wifiwissel. Na herstart de nieuwe build en de statuspagina via HTTP gecontroleerd; Windows bleef op Galaxy A37 5G 9FCD.
- Beide PWM-waarden worden door de firmware als 0 gerapporteerd. Geen elektrische meting of motortest uitgevoerd.
- Laatst gevonden adres in `.local/wemos-address.txt`; na een gewijzigde DHCP-toewijzing opnieuw `scripts/Discover-Wemos.py` uitvoeren.

## Handmatige bediening 0.2.0 — 9 oktober 2026

- Met toestemming van Tom browserbediening en een rode softwarestop toegevoegd. Vrijgeven en richting kiezen beginnen op PWM 0; geen motorbeweging door de agent aangevraagd.
- Laatste build geslaagd: RAM 30004 bytes (36,6%), flash 348027 bytes (33,3%). Zeven compile-time controles van de echte motorcontroller slagen, inclusief stopvergrendeling, heartbeat-timeout, timer-overloop en volledige PWM-rampen.
- Negen controles van het echte paginascript met gesimuleerde DOM/API-antwoorden geslaagd. Geen echte browserweergave gecontroleerd: er was geen browser beschikbaar in de automatiseringsomgeving.
- OTA-overdracht via de gewone hotspot geslaagd volgens de uploader; Windows bleef op Galaxy A37 5G 9FCD.
- Controle na de upload nog niet geslaagd: het oude adres 10.195.248.80 reageert niet; de discovery-scan vindt geen statuspagina en de wifi-scan ziet alleen de hotspot. Oorzaak nog onbekend. Nieuwe versie en PWM-uitgangen zijn nog niet op het apparaat bevestigd.
- Geen elektrische meting, fysieke motorproef of werkende noodstop op bewegend rad aangetoond. De webknop onderbreekt de 12 V-motorvoeding niet.
- Voeding en bedrading zijn door Tom bevestigd: 12 V motorzijde, buckconverter circa 5,0 V naar Wemos 5V en driverlogica, gemeenschappelijke massa. Ingangsbuffer en 3,3 V-compatibiliteit onbevestigd.
