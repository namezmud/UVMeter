/***************************************************
   UV Sensor v1.0-ML8511
   <http://www.dfrobot.com/index.php?route=product/product&product_id=1195&search=uv&description=true>
 ***************************************************
   This example reads UV intensity from UV Sensor v1.0-ML8511.

   Created 2014-9-23
   By Phoebe <phoebe.wang@dfrobot.com>
   Modified 2014-9-23
   By Phoebe phoebe.wang@dfrobot.com>

   GNU Lesser General Public License.
   See <http://www.gnu.org/licenses/> for details.
   All above must be included in any redistribution
 ****************************************************/

/***********Notice and Trouble shooting***************
   1.Connect ML8511 UV Sensor to Arduino A0
   <http://www.dfrobot.com/wiki/index.php/File:SEN0175_Diagram.png>
   2.This code is tested on Arduino Uno, Leonardo, Mega boards.
 ****************************************************/

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "Adafruit_STMPE610.h"

// For the Adafruit shield, these are the default.
#define TFT_DC 9
#define TFT_CS 10

#define DEBUG 0

#define  BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

int ReadUVintensityPin = A4; //Output from the sensor.  Avoid the touch screen pins A0 -> A3.

float max_intensity = 0;
float zero_voltage = 0.97;
float *thresh = 0;
float tubeThresh[] = {0.3, 1.0};
float compactThresh[] = {0.8, 1.3};

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

class Button
{
  private:
    int x, y, width, height;
    unsigned int bk_color, text_color, select_color;
    Adafruit_ILI9341* tft;
    String text = "";
    bool selected = false;

  public:

    Button(Adafruit_ILI9341* _tft, int _x, int _y, int _width, int _height, String _text, unsigned _bk_color = WHITE, unsigned int _text_color = BLACK, unsigned int _select_color = GREEN)
    {
      x = _x;
      y = _y;
      height = _height;
      width = _width;
      text = _text;
      bk_color = _bk_color;
      text_color = _text_color;
      tft = _tft;
      select_color = _select_color;
    }

    void setSelected(bool s)
    {
      selected = s;
    }

    bool isSelected()
    {
      return selected;
    }

    void display()
    {
      unsigned int col = (selected) ? select_color : bk_color;
      tft->fillRect(x, y, width, height, col);
      tft->setCursor(x + 10, y + 10);
      tft->setTextColor(text_color);
      tft->setTextSize(2);

      tft->println(text);
    }

    bool isIn(TS_Point p)
    {
      bool in =  ((p.x >= x && p.x <= (x + width)) &&
                  (p.y >= y && p.y <= y + height));


      if (DEBUG && in) {
        Serial.print("Is in "); Serial.println(text);
      }
      return in;

    }

};
Button btnZero = Button(&tft, 20, 250, 90, 40, "Reset", WHITE, BLACK);
Button btnReset = Button(&tft, 130, 250, 90, 40, "Zero", WHITE, BLACK);
Button btnTube = Button(&tft, 20, 200, 90, 40, "Tube", WHITE, BLACK, GREEN);
Button btnCompact = Button(&tft, 130, 200, 90, 40, "Compact", WHITE, BLACK, GREEN);

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 150
#define TS_MINY 130
#define TS_MAXX 3800
#define TS_MAXY 4000

// The STMPE610 uses hardware SPI on the shield, and #8
#define STMPE_CS 8
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);


void setup()
{
  pinMode(ReadUVintensityPin, INPUT);
  Serial.begin(9600); //open serial port, set the baud rate to 9600 bps
  Serial.println("Starting up...");

  tft.begin();

  // read diagnostics (optional but can help debug problems)
  uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDMADCTL);
  Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDPIXFMT);
  Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDIMGFMT);
  Serial.print("Image Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDSELFDIAG);
  Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX);
  Serial.print("Screen Width : "); Serial.println(tft.width());
  Serial.print("Screen Height : "); Serial.println(tft.height());

  if (!ts.begin()) {
    Serial.println("Couldn't start touchscreen controller");
    while (1);
  }
  Serial.println("Touchscreen started");

  tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);

  tft.setCursor(20, 100);
  tft.setTextColor(WHITE);    tft.setTextSize(3);
  tft.print(4.0);

  btnTube.setSelected(true);
  thresh = tubeThresh;
  
  display_buttons();
}

void reset_max()
{
  max_intensity = 0;
  Serial.println("Reset Max");
}

void zero(float v)
{
  zero_voltage = v;
  Serial.println("Zero Voltage");
}

void process_buttons(float currentVoltage) {
  // See if there's any  touch data for us
  while (!ts.bufferEmpty()) {

    // Retrieve a point
    TS_Point p = ts.getPoint();
    p.x = map(p.x, TS_MINY, TS_MAXY, 0, tft.width());
    p.y = map(p.y, TS_MINX, TS_MAXX, 0, tft.height());

    if (DEBUG) {
      Serial.print("Click ");
      Serial.print(p.x);
      Serial.print(" ");
      Serial.println(p.y);
    }

    if (btnZero.isIn(p)) {
      zero(currentVoltage);
    }

    if (btnReset.isIn(p)) {
      reset_max();
    }

    if (btnTube.isIn(p)) {
      btnTube.setSelected(true);
      btnCompact.setSelected(false);
      btnTube.display();
      btnCompact.display();
      thresh = tubeThresh;
    }
    if (btnCompact.isIn(p)) {
      btnTube.setSelected(false);
      btnCompact.setSelected(true);
      btnTube.display();
      btnCompact.display();
      thresh = compactThresh;
    }

  }
}

void display_buttons() {
  btnZero.display();
  btnReset.display();
  btnTube.display();
  btnCompact.display();
}

void loop()
{
  int uvLevel = averageAnalogRead(ReadUVintensityPin);

  float outputVoltage = 5.0 * uvLevel / 1024;
  float uvIntensity = mapfloat(outputVoltage, zero_voltage, 2.9, 0.0, 15.0);

  Serial.print("UVAnalogOutput: ");
  Serial.print(uvLevel);

  Serial.print(" OutputVoltage: ");
  Serial.print(outputVoltage);

  Serial.print(" UV Intensity: ");
  Serial.print(uvIntensity);
  Serial.print(" mW/cm^2");

  if (uvIntensity > max_intensity) {
    max_intensity = uvIntensity;
  }

  Serial.print(" Max UV Intensity: ");
  Serial.print(max_intensity);
  Serial.print(" mW/cm^2");

  Serial.println();

  process_buttons(outputVoltage);
  delay(100);
}

//Takes an average of readings on a given pin
//Returns the average
int averageAnalogRead(int pinToRead)
{
  byte numberOfReadings = 8;
  unsigned int runningValue = 0;

  for (int x = 0 ; x < numberOfReadings ; x++)
    runningValue += analogRead(pinToRead);
  runningValue /= numberOfReadings;

  return (runningValue);

}

//The Arduino Map function but for floats
//From: http://forum.arduino.cc/index.php?topic=3922.0
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


