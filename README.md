Lähdekoodi löytyy tiedostosta project_main.c

Konsoliin tulostuu:

0.01 lux
0.02, 0.00, -0.98, -0.05, -0.56, 0.18

ZzzZzZ...
ravinto: 9
leikki: 7
energia: 7

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
6. Lisäominaisuudet
