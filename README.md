TODO:
✅ uartTaskin if (programState == ...) lauseet
✅ sensorTaskin if (programState == ...) lauseet
✅ liikesensorin datan analysointi
✅ Alusta etäyhteys!
🟨 Sävellä musiikit!
🟨 Selvitä, voiko painonappia painaa pohjassa ja samaan aikaan käyttää liikesensoria


Toiminnallisuudet ja niiden ehdot:

* EAT (ravinto)
  * Painonappi
  * Voisi liittää esim. TILA_PAIKALLAAN ehdon mukaan?
* PET (hoiva)
  * sisainenState == TILA_PIMEA && temperature > 35
    * sisainenState == TILA_PAIKALLAAN && ambientLight < 5
      * sisainenState == TILA_0 && ax < 5 && ay < 10 && az < 5
* EXERCISE (liikunta)
  * sisainenState == TILA_LIIKKUU
    * sisainenState == TILA_0 && ax < -0.5
* ACTIVATE
  * sisainenState == TILA_VALO
    * sisainenState == TILA_PAIKALLAAN && ambientLight > 150


- Lämpötilasensori lisäksi, kun kasvatetaan hoivaa (PET)
- Kellofunktio, 10 sekunnin välein ravinto--, hoiva-- ja liikunta--
- taustajärjestelmän lähettämiin viesteihin reagoiminen
- Punainen ledi päälle-toiminto pois? Onko turha


programState kaavio: sensorTaskista uartTaskiin !

Data_ready (programState) -> tila_0 (sisainenState)
tila_0 (sisainenState) -> tila_liikkuu (sisainenState)
tila_liikkuu (sisainenState) -> liikunta_tila (programState, sensorTask -> UART)
liikunta_tila (programState, sensorTask -> UART) -> WAITING (programState, UART)



Konsolin tulostus:

uartTask
programState: 1

0.01 lux
0.02, 0.00, -0.98, -0.05, -0.56, 0.18

ZzzZzZ...
ravinto: 9
leikki: 7
energia: 7
aurinko: 0



**Toteutusjärjestys**

1. Työsuunnitelma
    - Globaalit muuttujat
    - Tilakone
    - SensorTag RTOS
2. Laitteessa sensoridatan keruu ja analyysi komentojen tulkitsemista varten.
    - Datankeräystaskit
    - Datan visualisointi
3. Tunnistusalgoritmin toteutus C-kielellä ensin työasemalla kerättyyn dataan perustuen.
4. Toimivan algoritmin siirto laitteeseen, toiminnallisuuksien testaus livenä.
    - Laitteen liu'utus pöydänpintaa pitkin
    - Laitteen kallistus ja/tai liike
    - Laitteen hyppyytys (=käy välillä ilmassa) pöydänpintaa pitkin
    - Laitteen liikuttelu ilman tukea "3D:nä" ilmassa.
5. Käyttöliittymän tuunaus, viestien lähetys/vastaanotto langattomasti/sarjaliikenteenä.
    - Painonapit
    - Ledit
    - Äänimerkit
6. Lisäominaisuudet.


* Käyttöliittymä
    - Painonapit
    - Ledit
    - Äänimerkit
* Testidatan kerääminen
    - Datankeräystaski (MPU9250-anturi)
        - Kiihtyvyys (acceleration)
        - Asentomuutos (gyroscope)
    - Aikaleima (ajallinen kesto)
    - Datan visualisointi
        - matplotlib
        - gnuplot
        - Microsoft Excel
* Ohjauskomennot
    - Komennon tulkitseminen
        - Voimakkuus
        - Kesto
    - Laitetta liikutellaan eri suuntiin (ylös, alas, vasemmalle)..
        - Laitteen liu'utus pöydänpintaa pitkin
        - Laitteen kallistus ja/tai liike
        - Laitteen hyppyytys (=käy välillä ilmassa) pöydänpintaa pitkin
        - Laitteen liikuttelu ilman tukea "3D:nä" ilmassa.
    - Kiihtyvyysanturi (MPU9250)
    - Asentoanturi (MPU9250)
* Viestintä
    - IoT-taustajärjestelmä
    - Komennon tunnistaminen
        - Viesti taustajärjestelmälle merkiksi
        - Äänimerkki tai ledi
    - Viestit taustajärjestelmältä
        - Viesteihin reagoiminen
* Toteutus
    - SensorTagin RTOS
    - Tilakone