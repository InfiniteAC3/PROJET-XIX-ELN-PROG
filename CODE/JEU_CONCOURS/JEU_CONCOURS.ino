#include <LedControl.h>

// Configuration pour tes broches exactes
#define DIN_PIN   11 
#define CLK_PIN   13 
#define LOAD_PIN  10  
#define NB_AFFICH  1 

LedControl lc = LedControl(DIN_PIN, CLK_PIN, LOAD_PIN, NB_AFFICH);

void setup() {
  // 1. Sortir du mode veille (Crucial !)
  lc.shutdown(0, false);
  
  // 2. Désactiver le mode Test (Celui qui allume tout à 8888)
  // La bibliothèque le fait normalement, mais on assure.
  
  // 3. Luminosité
  lc.setIntensity(0, 8);
  
  // 4. Nettoyer
  lc.clearDisplay(0);

  // 5. Affichage direct (Sans décodage BCD)
  // 'H' 'E' 'L' 'L' 'O'
 // On inverse les positions pour remettre "HELLO" à l'endroit
  lc.setRow(0, 0, 0x37); // H était en 7, on le met en 0
  lc.setRow(0, 1, 0x4F); // E était en 6, on le met en 1
  lc.setRow(0, 2, 0x0E); // L était en 5, on le met en 2
  lc.setRow(0, 3, 0x0E); // L était en 4, on le met en 3
  lc.setRow(0, 4, 0x7E); // O était en 3, on le met en 4
}

void loop() {
  // On ne fait rien, on attend de voir le HELLO
}