// Tilakone Tamagotchin toiminnoille (ruokkiminen, leikkiminen, nukkuminen, varoitus, hyytyminen)
enum state
{
    WAITING = 1,
    DATA_READY,
    RUOKINTA_TILA, // ravinto++
    LIIKUNTA_TILA, // leikki++
    ENERGIA_TILA,  // energia++
    TOIMINTO_VAROITUS,
    KARKAAMINEN
};
// Globaali tilamuuttuja, alustetaan odotustilaan
enum state programState = WAITING;

// Tilakone SensorTagin toiminnoille
enum tila
{
    TILA_0 = 1,
    TILA_PAIKALLAAN, // liikesensori
    TILA_LIIKKUU,    // liikesensori
    TILA_VALO,       // valosensori
    STATE_PIMEA,     // valosensori
};
// Globaali tilamuuttuja, alustetaan odotustilaan
enum tila sisainenState = TILA_0;





if (programState == DATA_READY)
{
    sisainenState = TILA_0;
}

if ((sisainenState == TILA_0) && (ax < -0.5) && (leikki < 10))
{
    leikki++;
    sprintf(debug_msg, "Leikitaan!: %d\n", leikki);
    System_printf(debug_msg);
    System_flush();
    buzzerPlay();
    programState = WAITING;
}

if (sisainenState == TILA_PAIKALLAAN && ambientLight < 15 && energia < 10)
{
    // tamagotchi nukkuu
    energia++;
    System_printf("asetetaan energiatila\n");
    sprintf(energia_merkkijono, "energia: %d\n", energia);
    System_printf(energia_merkkijono);
    System_flush();
    musiikkiFunktioEnergia();
    programState = WAITING;
}