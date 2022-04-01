/*
 Name:		Regenautomat_Steeger.ino
 Created:	13.07.2021 13:31:38
 Author:	XSSTEF
*/

/*
Errorcodes:
    1   -->   Krannummer existiert nicht (0 oder größer als maximale Anzahl an Kränen)
    2   -->   Laufzeit zu klein eingestellt (0 min)
    3   -->   Maximale Anzahl an Programmplaetze erreicht
    4   -->   Pumpe ist nicht angelaufen
    5   -->   Pumpe ist nicht ausgegangen
    6   -->   Keypressed Buffer ist voll
*/

//Libraries
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//Constants
    //User interface
const unsigned int maxNumberOfValves = 2;           //Max number of valves in the garden
const unsigned long valveTime5 = 300000;            //Runtime 5 min in ms
const unsigned long valveTime10 = 600000;           //Runtime 10 min in ms
const unsigned long valveTime15 = 900000;           //Runtime 15 min in ms
const unsigned long valveTime20 = 1200000;          //Runtime 20 min in ms
const unsigned long waitTimeFirstRun = 5000;        //Wait 5 sec before first valve switch
const char* nameRelais1 = "HAUPTGARTEN";            //Name valve 1
const char* nameRelais2 = "VORGARTEN";              //Name valve 2
const char* nameRelais3 = "Placeholder";            //Name valve 3
const char* nameRelais4 = "PUMPE";                  //Name valve 4
const char* nameRelais5 = "Placeholder";            //Name valve 5
const char* nameRelais6 = "Placeholder";            //Name valve 6
const char* nameRelais7 = "Placeholder";            //Name valve 7
const char* nameRelais8 = "Placeholder";            //Name valve 8

    //General
const unsigned int displayI2CAdr = 0x27;            //I2C address display
const unsigned int displayRows = 16;                //Setup display
const unsigned int displayColumns = 2;              //Setup display
const unsigned int displayRow1 = 0;                 //Setup display
const unsigned int displayRow2 = 1;                 //Setup display
const unsigned int numberOfValves = 20;             //Max number of valves in the buffer
const unsigned int numberOfChars = 10;              //Max number of characters in the buffer
const unsigned int lcdRefresh = 1000;               //Refresh time LCD display in START mode --> 1 sec

//Right pinheader
const unsigned int digitalIO33 = 2;                 //Keypad row1      
const unsigned int digitalIO34 = 3;                 //Keypad row2      
const unsigned int digitalIO35 = 4;                 //Keypad row3      
const unsigned int digitalIO36 = 5;                 //Keypad row4 
const unsigned int digitalIO17 = 6;                 //Keypad col1      
const unsigned int digitalIO18 = 7;                 //Keypad col2      
const unsigned int digitalIO19 = 8;                 //Keypad col3      
const unsigned int digitalIO20 = 9;                 //Keypad col4           
const unsigned int digitalIO41 = 10;                //Relais 1    
const unsigned int digitalIO42 = 11;                //Relais 2       
const unsigned int digitalIO44 = 12;                //Relais 4 
//Ausgang 13 kann wegen LED inbuild nicht belegt werden

//States
typedef enum
{
    INITIAL,
    SETUP_VALVENUMBER,
    SETUP_VALVETIME,
    READY_START,
    START,
    STOP
}
enumstate_t;

//Set keypad 
const byte rows = 4;
const byte cols = 4;

char keypad_matrix[rows][cols] =
{
  {'1' , '2' , '3' , 'A'},
  {'4' , '5' , '6' , 'B'},
  {'7' , '8' , '9' , 'C'},
  {'*' , '0' , '#' , 'D'}
};

byte rowPins[rows] = { digitalIO33, digitalIO34, digitalIO35, digitalIO36 };
byte colPins[cols] = { digitalIO17, digitalIO18, digitalIO19, digitalIO20 };
Keypad keypad = Keypad(makeKeymap(keypad_matrix), rowPins, colPins, rows, cols);

//Globals
LiquidCrystal_I2C lcd(displayI2CAdr, displayRows, displayColumns);
enumstate_t state = INITIAL;
char keyPressed;
char keyPressedBuffer[numberOfChars];
unsigned long startTime;
unsigned long firstStart;
unsigned long lcdRefreshInterval;
unsigned int timeMin = 0;
unsigned int timeSec = 0;
unsigned int keyPressedBufferCounter = 0;
unsigned int valveNumber[numberOfValves];
unsigned long valveTime[numberOfValves];
unsigned int valveCounter = 0;
unsigned int programCounter = 0;
unsigned int errorValue = 0;
bool lcdClear = false;
bool programDone = false;
bool errorOccurred = false;
bool programCancel = false;
bool displayOn = true;
bool firstRun = false;
bool isProgramA = false;
bool isProgramB = false;
bool isProgramC = false;
bool isProgramX = false;
bool isPumpCheck = false;

//Funktion zum Schalten der Magentventile (8 Stueck vordefiniert)
void switchRelais()
{
    if (firstRun)
    {
        lcdRefreshInterval = lcdRefresh;
        digitalWrite(digitalIO44, LOW);     //Pumpe einschalten, danach 5sec warten
        firstStart = millis();
    }

    while (firstRun)
    {
        keyPressed = keypad.getKey();
        
        //Fehler 4: Pumpe ist nicht angelaufen
        if (digitalRead(digitalIO44) == HIGH)
        {
            lcdClear = false;
            errorOccurred = true;
            errorValue = 4;
            state = STOP;
            break;
        }
        
        if (millis() - firstStart >= waitTimeFirstRun)
        {
            firstRun = false;
            lcd.clear();
            lcdClear = true;
            break;
        }
        if (millis() - startTime >= lcdRefreshInterval)
        {
            timeMin = ((waitTimeFirstRun - (millis() - firstStart)) / 1000) / 60;
            timeSec = ((waitTimeFirstRun - (millis() - firstStart)) / 1000) % 60;

            lcd.setCursor(0, displayRow1);
            lcd.print("PROGRAMM ");
            if (isProgramA)
                lcd.print("A");
            else if (isProgramB)
                lcd.print("B");
            else if (isProgramC)
                lcd.print("C");
            else if (isProgramX)
                lcd.print("#");
            lcd.print(" IN:");
            lcd.setCursor(0, displayRow2);
            if (timeMin < 10)
                lcd.print("0");
            lcd.print(timeMin);
            lcd.print(":");
            if (timeSec < 10)
                lcd.print("0");
            lcd.print(timeSec);
            lcdRefreshInterval = lcdRefreshInterval + lcdRefresh;
        }
        if (keyPressed != NO_KEY)
        {
            if (keyPressed == '*')
            {
                if (!displayOn)
                {
                    lcd.backlight();
                    displayOn = true;
                }
                else
                {
                    lcd.noBacklight();
                    displayOn = false;
                }
            }
            if (keyPressed == 'D')
            {
                lcdClear = false;
                programCancel = true;
                state = STOP;
                break;
            }
        }
    }

    if (!firstRun)
    {
        if (valveNumber[programCounter] == 1)
        {
            digitalWrite(digitalIO41, HIGH);
            digitalWrite(digitalIO42, HIGH);
            delay(1000);
            digitalWrite(digitalIO41, LOW);
        }
        else if (valveNumber[programCounter] == 2)
        {
            digitalWrite(digitalIO41, HIGH);
            digitalWrite(digitalIO42, HIGH);
            delay(1000);
            digitalWrite(digitalIO42, LOW);
        }       
    }
}

//Funktion zur Benennung der Magnetventile (maximal 8 Zeichen lang, 8 Stueck vordefiniert)
void nameRelais()
{
    if (valveNumber[programCounter] == 1)
        lcd.print(nameRelais1);
    else if (valveNumber[programCounter] == 2)
        lcd.print(nameRelais2);
}

void setup()
{
    //DISPLAY
    lcd.init();
    lcd.backlight();

    //INPUTS

    //OUTPUTS
    pinMode(digitalIO41, OUTPUT);
    pinMode(digitalIO42, OUTPUT);
    pinMode(digitalIO44, OUTPUT);

    digitalWrite(digitalIO41, HIGH);
    digitalWrite(digitalIO42, HIGH);
    digitalWrite(digitalIO44, HIGH);
}

void loop()
{
    switch (state)
    {

        //Einstiegspunkt in das Programm
    case INITIAL:
        if (!lcdClear)
        {
            lcd.clear();
            lcdClear = true;
        }

        keyPressed = keypad.getKey();

        lcd.setCursor(0, displayRow1);
        lcd.print("USER: STEEGER");
        lcd.setCursor(0, displayRow2);
        lcd.print("PROGRAMM WAEHLEN");

        if (keyPressed != NO_KEY)
        {
            //Display An/Aus
            if (keyPressed == '*')
            {
                if (!displayOn)
                {
                    lcd.backlight();
                    displayOn = true;
                }
                else
                {
                    lcd.noBacklight();
                    displayOn = false;
                }
            }
            //Eigenes Programm erstellen
            if (keyPressed == '#')
            {
                lcdClear = false;
                isProgramX = true;
                state = SETUP_VALVENUMBER;
            }
            //Starte Programm 'A' mit festgelegten Werten und allen Magnetventilen
            else if (keyPressed == 'A')
            {
                lcdClear = false;
                isProgramA = true;

                for (int n = 0; n < maxNumberOfValves; n++)
                {
                    valveNumber[n] = n + 1;              //Nummeriere Krannummern durch bis MaxValve erreicht
                    valveTime[n] = valveTime10;          //Setze alle Kranzeiten auf 10 Minuten
                }

                valveCounter = maxNumberOfValves;
                firstRun = true;
                state = START;
            }
            //Starte Programm 'B' mit festgelegten Werten und allen Magnetventilen
            else if (keyPressed == 'B')
            {
                lcdClear = false;
                isProgramB = true;

                for (int n = 0; n < maxNumberOfValves; n++)
                {
                    valveNumber[n] = n + 1;             //Nummeriere Krannummern durch bis MaxValve erreicht
                    valveTime[n] = valveTime15;         //Setze alle Kranzeiten auf 15 Minuten
                }

                valveCounter = maxNumberOfValves;
                firstRun = true;
                state = START;
            }
            //Starte Programm 'C' mit festgelegten Werten und allen Magnetventilen
            else if (keyPressed == 'C')
            {
                lcdClear = false;
                isProgramC = true;

                for (int n = 0; n < maxNumberOfValves; n++)
                {
                    valveNumber[n] = n + 1;             //Nummeriere Krannummern durch bis MaxValve erreicht
                    valveTime[n] = valveTime20;         //Setze alle Kranzeiten auf 20 Minuten
                }

                valveCounter = maxNumberOfValves;
                firstRun = true;
                state = START;
            }
        }
        break;

        //Auswaehlen des Magnetventils (nur in eigener Programmerstellung)
    case SETUP_VALVENUMBER:
        if (!lcdClear)
        {
            lcd.clear();
            lcdClear = true;
        }

        keyPressed = keypad.getKey();

        lcd.setCursor(0, displayRow1);
        lcd.print("VENTILNUMMER: ");

        if (keyPressed != NO_KEY)
        {
            if (keyPressed == '*')
            {
                if (!displayOn)
                {
                    lcd.backlight();
                    displayOn = true;
                }
                else
                {
                    lcd.noBacklight();
                    displayOn = false;
                }
            }
            if ((keyPressed >= 48) && (keyPressed <= 57))
            {
                keyPressedBuffer[keyPressedBufferCounter] = keyPressed;
                lcd.setCursor(keyPressedBufferCounter, displayRow2);
                lcd.print(keyPressed);
                keyPressedBufferCounter++;

                //Fehler 6: keyPressed Buffer ist voll
                if (keyPressedBufferCounter >= numberOfChars)
                {
                    lcdClear = false;
                    errorOccurred = true;
                    errorValue = 6;
                    state = STOP;
                    break;
                }
            }
            else if (keyPressed == '#' && keyPressedBufferCounter > 0)
            {
                //Fehler 1: Magnetventil nicht vorhanden
                if (atoi(keyPressedBuffer) > maxNumberOfValves || atoi(keyPressedBuffer) == 0)
                {
                    lcdClear = false;
                    errorOccurred = true;
                    errorValue = 1;
                    state = STOP;
                    break;
                }
                keyPressedBuffer[keyPressedBufferCounter] = NO_KEY;
                keyPressedBufferCounter = 0;
                valveNumber[valveCounter] = atoi(keyPressedBuffer);
                memset(keyPressedBuffer, 0, sizeof(keyPressedBuffer));
                lcdClear = false;
                state = SETUP_VALVETIME;
            }
            else if (keyPressed == 'D')
            {
                lcdClear = false;
                programCancel = true;
                state = STOP;
            }
        }
        break;

        //Auswaehlen der Laufzeit (nur in eigener Programmerstellung)
    case SETUP_VALVETIME:
        if (!lcdClear)
        {
            lcd.clear();
            lcdClear = true;
        }

        keyPressed = keypad.getKey();

        lcd.setCursor(0, displayRow1);
        lcd.print("LAUFZEIT IN MIN:");

        if (keyPressed != NO_KEY)
        {
            if (keyPressed == '*')
            {
                if (!displayOn)
                {
                    lcd.backlight();
                    displayOn = true;
                }
                else
                {
                    lcd.noBacklight();
                    displayOn = false;
                }
            }
            if ((keyPressed >= 48) && (keyPressed <= 57))
            {
                keyPressedBuffer[keyPressedBufferCounter] = keyPressed;
                lcd.setCursor(keyPressedBufferCounter, displayRow2);
                lcd.print(keyPressed);
                keyPressedBufferCounter++;

                //Fehler 6: keyPressed Buffer ist voll
                if (keyPressedBufferCounter >= numberOfChars)
                {
                    lcdClear = false;
                    errorOccurred = true;
                    errorValue = 6;
                    state = STOP;
                    break;
                }
            }
            else if (keyPressed == '#' && keyPressedBufferCounter > 0)
            {
                //Fehler 2: Zeit zu klein (0 min)
                if (atoi(keyPressedBuffer) == 0)
                {
                    lcdClear = false;
                    errorOccurred = true;
                    errorValue = 2;
                    state = STOP;
                    break;
                }
                keyPressedBuffer[keyPressedBufferCounter] = NO_KEY;
                keyPressedBufferCounter = 0;
                valveTime[valveCounter] = atol(keyPressedBuffer) * 60 * 1000;
                memset(keyPressedBuffer, 0, sizeof(keyPressedBuffer));
                lcdClear = false;
                valveCounter++;

                //Fehler 3: Maximale Anzahl an Programmplaetzen erreicht
                if (valveCounter >= numberOfValves)
                {
                    lcdClear = false;
                    errorOccurred = true;
                    errorValue = 3;
                    state = STOP;
                    break;
                }

                state = READY_START;
            }
            else if (keyPressed == 'D')
            {
                lcdClear = false;
                programCancel = true;
                state = STOP;
            }
        }
        break;

        //Eigenes Programm starten oder weiteres Magnetventil hinzufügen
    case READY_START:
        if (!lcdClear)
        {
            lcd.clear();
            lcdClear = true;
        }

        keyPressed = keypad.getKey();

        lcd.setCursor(0, displayRow1);
        lcd.print("WEITER MIT #");
        lcd.setCursor(0, displayRow2);
        lcd.print("NEUER KRAN MIT 0");

        if (keyPressed != NO_KEY)
        {
            if (keyPressed == '*')
            {
                if (!displayOn)
                {
                    lcd.backlight();
                    displayOn = true;
                }
                else
                {
                    lcd.noBacklight();
                    displayOn = false;
                }
            }
            if (keyPressed == '0')
            {
                lcdClear = false;
                state = SETUP_VALVENUMBER;
            }
            else if (keyPressed == '#')
            {
                lcdClear = false;
                firstRun = true;
                state = START;
            }
            else if (keyPressed == 'D')
            {
                lcdClear = false;
                programCancel = true;
                state = STOP;
            }
        }
        break;

        //Programm starten (bis beendet, abgebrochen, oder Fehler aufgetreten)
    case START:
        if (!lcdClear)
        {
            lcd.clear();
            lcdClear = true;
        }

        switchRelais();

        if (programCancel)
            break;

        startTime = millis();
        lcdRefreshInterval = lcdRefresh;

        while (1)
        {
            keyPressed = keypad.getKey();

            if (((millis() - startTime >= valveTime[programCounter]) && (programCounter < valveCounter)) || (keyPressed == '#'))
            {
                programCounter++;

                if (programCounter == valveCounter)
                {
                    lcdClear = false;
                    programDone = true;
                    state = STOP;
                    break;
                }

                lcdClear = false;
                break;
            }
            if (millis() - startTime >= lcdRefreshInterval)
            {
                timeMin = ((valveTime[programCounter] - (millis() - startTime)) / 1000) / 60;
                timeSec = ((valveTime[programCounter] - (millis() - startTime)) / 1000) % 60;

                lcd.setCursor(0, displayRow1);
                nameRelais();
                lcd.setCursor(0, displayRow2);
                lcd.print("RESTZEIT: ");
                if (timeMin < 10)
                    lcd.print("0");
                lcd.print(timeMin);
                lcd.print(":");
                if (timeSec < 10)
                    lcd.print("0");
                lcd.print(timeSec);
                lcdRefreshInterval = lcdRefreshInterval + lcdRefresh;
            }
            if (keyPressed != NO_KEY)
            {
                if (keyPressed == '*')
                {
                    if (!displayOn)
                    {
                        lcd.backlight();
                        displayOn = true;
                    }
                    else
                    {
                        lcd.noBacklight();
                        displayOn = false;
                    }
                }
                if (keyPressed == 'D')
                {
                    lcdClear = false;
                    programCancel = true;
                    state = STOP;
                    break;
                }
            }
        }
        break;

        //Programm hat gestoppt (durch beendet, abgebrochen, oder Fehler aufgetreten)
    case STOP:
        if (!lcdClear)
        {
            digitalWrite(digitalIO41, HIGH);
            digitalWrite(digitalIO42, HIGH);
            
            if (digitalRead(digitalIO44 == LOW))
            {
                delay(1000);
                digitalWrite(digitalIO44, HIGH);        //Pumpe ausschalten nach 1sec

                //Fehler 5: Pumpe ist nicht ausgegangen
                if (digitalRead(digitalIO44) == LOW)
                {
                    errorOccurred = true;
                    errorValue = 5;
                }
            }
            
            lcd.clear();
            lcdClear = true;
        }

        keyPressed = keypad.getKey();
        
        lcd.setCursor(0, displayRow1);
        if (programDone)
        {
            lcd.print("PRG. BEENDET");
        }
        else if (errorOccurred)
        {
            lcd.print("FEHLERCODE:");
            lcd.print(errorValue);
        }
        else if (programCancel)
        {
            lcd.print("PRG. ABGEBROCHEN");
        }
        lcd.setCursor(0, displayRow2);
        lcd.print("WEITER MIT #");
        
        if (keyPressed != NO_KEY)
        {
            if (keyPressed == '*')
            {
                if (!displayOn)
                {
                    lcd.backlight();
                    displayOn = true;
                }
                else
                {
                    lcd.noBacklight();
                    displayOn = false;
                }
            }
            //Zuruecksetzen der Variablen auf den Standardwert, bevor das Programm an den Einstiegspunkt zurueck springt
            if (keyPressed == '#')
            {
                memset(valveTime, 0, sizeof(valveTime));
                memset(valveNumber, 0, sizeof(valveNumber));
                memset(keyPressedBuffer, 0, sizeof(keyPressedBuffer));
                keyPressedBufferCounter = 0;
                valveCounter = 0;
                programCounter = 0;
                errorValue = 0;
                programDone = false;
                errorOccurred = false;
                programCancel = false;
                lcdClear = false;
                firstRun = false;
                isProgramA = false;
                isProgramB = false;
                isProgramC = false;
                isProgramX = false;
                state = INITIAL;
            }
        }
        break;
    }
}

