#include <U8g2lib.h>  // Install "U8g2" from library manager
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

byte ecran = 0;  // variable affichage kwh sauvés jour / total ///

void screen_loop(float Voltage, float Intensite1, float Power1, float Energy1, float EnergySavedDaily, float Frequency, float PowerFactor2, float Intensite2, float Power2,
                 float Energy2, float ajustePuissance, float valDim1, float valDim2, boolean Auto) {

  u8g2.clearBuffer();  // on efface ce qui se trouve déjà dans le buffer

  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.setCursor(25, 10);     // position du début du texte
  u8g2.print("Le Profes'S");  // écriture de texte
  u8g2.setFont(u8g2_font_unifont_t_symbols);
  u8g2.drawGlyph(70, 13, 0x2600);
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.setCursor(81, 10);      // position du début du texte
  u8g2.print("laire  v6.31");  // écriture de texte

  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.setCursor(40, 30);
  u8g2.print(Power1);
  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.setCursor(110, 30);
  u8g2.print("W");  // écriture de texte
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.setCursor(10, 47);
  u8g2.print(ajustePuissance);  // injection ou surplus //
  u8g2.setCursor(10, 64);
  u8g2.print("Sauvés : ");  // écriture de texte


  u8g2.setCursor(115, 64);
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.print("KWH");  // écriture de texte
  u8g2.drawRFrame(30, 15, 70, 22, 7);
  u8g2.setCursor(10, 47);
  u8g2.setCursor(75, 47);
  u8g2.print(WiFi.localIP());
  ecran = ecran + 1;

  if (ecran == 1 || ecran == 2 || ecran == 3 || ecran == 4) {
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.setCursor(10, 64);
    u8g2.print("Sauvés T : ");  // écriture de texte
    u8g2.setCursor(85, 64);
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.print(Energy1);  // écriture de texte
    delay(500);
  }
  if (ecran == 5 || ecran == 6 || ecran == 7) {
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.setCursor(10, 64);
    u8g2.print("Sauvés J : ");  // écriture de texte
    u8g2.setCursor(85, 64);
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.print(EnergySavedDaily);  // écriture de texte
    delay(500);
  }

  if (ecran == 8) {
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.setCursor(10, 64);
    u8g2.print("Sauvés J : ");  // écriture de texte
    u8g2.setCursor(85, 64);
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.print(EnergySavedDaily);  // écriture de texte
    delay(500);
    ecran = 0;
  }
  if (Power1 > 20) {
    u8g2.setFont(u8g2_font_emoticons21_tr);
    u8g2.drawGlyph(12, 36, 0x0036);

  }

  else {
    u8g2.setFont(u8g2_font_emoticons21_tr);
    u8g2.drawGlyph(12, 36, 0x0026);
  }

  u8g2.sendBuffer();  // display data on screen
}