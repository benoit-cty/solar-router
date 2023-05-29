#include <AsyncTCP.h>           // Install "AsyncTCP" from library manager
#include <ESPAsyncWebServer.h>  // Download from https://github.com/me-no-dev/ESPAsyncWebServer
#include <ESPDash.h>            // Install "ESP-DASH" from library manager

// Configure Dashboard web
AsyncWebServer server(80);
ESPDash dashboard(&server);
Card button(&dashboard, BUTTON_CARD, "Auto");
Card slider(&dashboard, SLIDER_CARD, "Puissance sortie 1 en %", "", 0, 95);
Card voltage(&dashboard, GENERIC_CARD, "Voltage", "V");
Card power2(&dashboard, GENERIC_CARD, "Power2", "W");
Card frequency(&dashboard, GENERIC_CARD, "frequency", "Hz");
Card consommationsurplus(&dashboard, GENERIC_CARD, "surplus / consommation",
                         "watts");
Card puissance(&dashboard, GENERIC_CARD, "puissance envoyée au ballon",
               "watts");
Card intensite1(&dashboard, GENERIC_CARD, "Intensité 1", "A");
Card intensite2(&dashboard, GENERIC_CARD, "Intensité 2", "A");
Card energy1(&dashboard, GENERIC_CARD, "énergie sauvée", "kwh");
Card energy_saved_daily(&dashboard, GENERIC_CARD, "énergie sauvée aujourd'hui", "kwh");
Card energy2(&dashboard, GENERIC_CARD, "énergie 2", "kwh");
Card valdim1(&dashboard, PROGRESS_CARD, "sortie 1", "%", 0, 95);
Card valdim2(&dashboard, PROGRESS_CARD, "sortie 2", "%", 0, 95);

void dashboard_loop(float Voltage, float Intensite1, float Power1, float Energy1, float EnergySavedDaily, float Frequency, float PowerFactor2, float Intensite2, float Power2,
                    float Energy2, float ajustePuissance, float valDim1, float valDim2, boolean Auto) {
  consommationsurplus.update(ajustePuissance);
  voltage.update(Voltage);
  frequency.update(Frequency);
  intensite1.update(Intensite1);
  intensite2.update(Intensite2);
  puissance.update(Power1);
  power2.update(Power2);
  energy1.update(Energy1);
  energy_saved_daily.update(EnergySavedDaily);
  energy2.update(Energy2);
  valdim1.update(valDim1);
  valdim2.update(valDim2);
  button.update(Auto);
  dashboard.sendUpdates();
}
