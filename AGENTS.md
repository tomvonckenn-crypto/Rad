# Werkafspraken voor het rad

- Stel eerst een plan voor en wacht op expliciete instemming voordat je grote projectstappen uitvoert.
- Commit en push na iedere grote, goedgekeurde wijziging de projectmap naar `https://github.com/tomvonckenn-crypto/Rad.git`. Controleer eerst de build en relevante tests. Push geen wifiwachtwoorden, buildbestanden of lokale logs en gebruik geen force-push. Bewaar bestaande remote-geschiedenis.
- De gebruiker wil eerst rustig de hardware controleren. Geen firmware uploaden, van wifi wisselen of de motor aansturen zonder expliciet akkoord voor die stap. Inspectie van bestanden en documentatie mag wel.
- De laatst via HTTP bevestigde firmware is 0.1.0-netwerk. De OTA-overdracht van 0.2.0-handmatig is geslaagd, maar de statuspagina reageert daarna niet via de hotspot of het reserve-AP. `Rad-Wemos` is zichtbaar en verbinden lukte; oorzaak onbekend. Claim dus nog geen werkende hardwarebediening. R_EN en L_EN zijn vast met 5 V verbonden: lage PWM-uitgangen betekenen niet dat de vermogensbrug volledig is uitgeschakeld.
- De gebruiker heeft op 9 oktober 2026 bevestigd dat de onderstaande tabel de daadwerkelijke bedrading beschrijft. De bedrading is niet door de agent fysiek gecontroleerd.

| Driverpin | Daadwerkelijke aansluiting volgens gebruiker |
| --- | --- |
| VCC | 5 V |
| GND | GND van ESP8266 |
| R_EN | 5 V |
| L_EN | 5 V |
| RPWM | GPIO4 (D2 op Wemos D1 mini) |
| LPWM | GPIO5 (D1 op Wemos D1 mini) |
| R_IS | Niet aangesloten |
| L_IS | Niet aangesloten |

- Voeding volgens gebruiker: aparte 12 V-voeding voor de motorzijde; buckconverter levert gemeten ongeveer 5,0 V aan Wemos 5V-pin en driverlogica. Alle massa's zijn verbonden.
- Nog vaststellen: geschiktheid van eventuele ingangsbuffer voor 3,3 V-signalen, huidige overbrenging en mechanica, en het werkelijke remgedrag. De gebruiker wil de bufferchip niet controleren; behandel de compatibiliteit als onbekend.
- Wifi en OTA werken via de Galaxy-hotspot; toekomstige uploads gebruiken scripts/Upload.ps1 en vereisen normaal geen wifiwissel. Hardwaretests en adressen staan in STATUS.md.
