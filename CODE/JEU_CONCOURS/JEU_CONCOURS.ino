#include <LedControl.h>

// ============================================================
//  CONFIGURATION MATÉRIELLE (HARDWARE CONFIG)
// ============================================================
#define DIN_PIN 11
#define CLK_PIN 13
#define LOAD_PIN 10
#define NB_AFFICH 1
#define RNOK 3
#define ROK 2
#define MODE 1
#define R0 0
#define ANO 4
#define CAT 5
#define HP 6
#define BJ1 7
#define BJ2 8
#define BJ3 9
#define BJ4 12
//leds
LedControl lc = LedControl(DIN_PIN, CLK_PIN, LOAD_PIN, NB_AFFICH);

// variables affichage de temps
int TEMPS_QUESTION = 15; 
int tempsQuestion = TEMPS_QUESTION;
int temps_derniere_action = 0;
// ============================================================
//  PROTOTYPES DES FONCTIONS (MODULARITÉ)
// ============================================================
void initialiserAffichage();
void configurerComposants();
void afficherChaine(const char* texte);
void effacerEcran();
byte charTo7Seg(char c);
String obtenirTempsString();

void afficherTemps();

void aBuzze();
void tempsEcoule();
void verifierReponse();
// ============================================================
//  FONCTION PRINCIPALE : SETUP
// ============================================================
void setup() {
  configurerComposants();
  initialiserAffichage();

  // Test de la modularité
  afficherChaine("HELLO");
  delay(1500);
  effacerEcran();
  afficherChaine("JEU STIC");  // Message de ton schéma
  delay(1500);
  afficherChaine("temps");
  delay(2000);
  effacerEcran();
  temps_derniere_action = millis();
}

// ============================================================
//  FONCTION PRINCIPALE : LOOP
// ============================================================
void loop() {
  // Le loop reste vide pour l'instant, prêt pour la logique de jeu

  afficherTemps();
  verifierReponse();
}

// ============================================================
//  COUCHE 1 : GESTION BAS NIVEAU (SEGMENTS)
// ============================================================

/**
 * Convertit un caractère ASCII en code 7-segments.
 * Facile à mettre à jour si tu veux ajouter des symboles.
 */
byte charTo7Seg(char c) {
  switch (toupper(c)) {
    // Lettres
    case 'A': return 0x77;
    case 'B': return 0x1F;
    case 'C': return 0x4E;
    case 'D': return 0x3D;
    case 'E': return 0x4F;
    case 'F': return 0x47;
    case 'G': return 0x5E;
    case 'H': return 0x37;
    case 'I': return 0x30;
    case 'J': return 0x3C;
    case 'L': return 0x0E;
    case 'O': return 0x7E;
    case 'P': return 0x67;
    case 'R': return 0x05;
    case 'S': return 0x5B;
    case 'T': return 0x0F;
    case 'U': return 0x3E;
    // caractères vraiment spéciaux
    case 'M': return 0x76;
    case 'N': return 0x25;
    // Chiffres
    case '0': return 0x7E;
    case '1': return 0x30;
    case '2': return 0x6D;
    case '3': return 0x79;
    case '4': return 0x33;
    case '5': return 0x5B;
    case '6': return 0x5F;
    case '7': return 0x70;
    case '8': return 0x7F;
    case '9': return 0x7B;
    // Symboles
    case '-': return 0x01;
    case '?': return 0x6B;
    case '_': return 0x08;
    case ' ': return 0x00;
    default: return 0x00;  // Caractère inconnu = éteint
  }
}

// ============================================================
//  COUCHE 2 : INTERFACE D'AFFICHAGE (LOGIC)
// ============================================================

/**
 * Configure le MAX7219 au démarrage.
 */
void initialiserAffichage() {
  lc.shutdown(0, false);  // Réveil
  lc.setIntensity(0, 8);  // Luminosité moyenne
  lc.clearDisplay(0);     // Nettoyage
}

/**
 * Affiche une chaîne de caractères (max 8) de gauche à droite.
 * Gère l'inversion observée sur ton Proteus.
 */
void afficherChaine(const char* texte) {
  effacerEcran();
  int n = strlen(texte);

  for (int i = 0; i < n && i < 8; i++) {
    // On utilise l'index 'i' car ton afficheur Proteus
    // semble mapper DIG0 à la position la plus à gauche.
    lc.setRow(0, i, charTo7Seg(texte[i]));
  }
}

/**
 * Éteint tous les segments de l'afficheur.
 */
void effacerEcran() {
  lc.clearDisplay(0);
}

/**
 * Retourne le temps écoulé depuis le démarrage.
 * Format : "MM-SS" (ex: "01-25" pour 1min 25s)
 */
String obtenirTempsString() {
  
  tempsQuestion = TEMPS_QUESTION - (millis() - temps_derniere_action)/1000;
  int minutes = tempsQuestion/ 60;
  int secondes = tempsQuestion % 60;
  

  String tempsFormate = "";

  // Ajout du zéro initial pour les minutes si < 10
  if (minutes < 10) tempsFormate += "0";
  tempsFormate += String(minutes);

  tempsFormate += "-";  // Séparateur

  // Ajout du zéro initial pour les secondes si < 10
  if (secondes < 10) tempsFormate += "0";
  tempsFormate += String(secondes);

  return tempsFormate;
}

void afficherTemps() {
  String tempsActuel = obtenirTempsString();

  // On convertit la String en tableau de caractères (char*)

  // pour notre fonction afficherChaine

  afficherChaine(tempsActuel.c_str());
  }
  
  // Ici, il n'y a PAS de delay. 
  // L'Arduino sort de la fonction instantanément si les 500ms ne sont pas passées.

void aBuzze() {
  afficherChaine("bu22");
  delay(1000);
  temps_derniere_action = millis();
}
void tempsEcoule() {
  afficherChaine("ecoule ");
  delay(1000);
  temps_derniere_action = millis();
}

void verifierReponse(){
  if (!digitalRead(BJ1))
  {
    aBuzze();

  }
  else if(tempsQuestion <=0 )
  {
    tempsEcoule();
  }
  
}
void configurerComposants(){
  int inputPins[8] = {12,7,8,9,0,1,2,3};
  int outputPins[3] = {4,5,6};

  for (int i = 0; i < 8 ; i++)
  {
    pinMode(inputPins[i], INPUT);
  }
   for (int i = 0; i < 3 ; i++)
  {
    pinMode(outputPins[i], OUTPUT);
  }
}